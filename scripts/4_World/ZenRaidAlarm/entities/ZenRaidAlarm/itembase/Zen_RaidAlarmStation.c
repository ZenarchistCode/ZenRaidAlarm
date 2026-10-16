class Zen_RaidAlarmStation extends ItemBase
{
	// Persistent data
	protected string m_ZenBaseName = "";
	protected string m_ZenWebhook1 = "";
	protected string m_ZenWebhook2 = "";
	protected string m_ZenWebhook3 = "";
	protected bool m_ZenHadValidRadar = false;
	protected int m_ZenWebhook1_FailCount = 0;
	protected int m_ZenWebhook2_FailCount = 0;
	protected int m_ZenWebhook3_FailCount = 0;

	// Session-specific data
	protected Zen_RaidAlarmRadar m_ZenAttachedRaidRadar;
	protected int m_ZenLastDiscordMessageTime = 0;
	protected bool m_ZenDontNotifyAboutPower = false;
	protected bool m_ZenNotifiedLowPower = false;
	protected bool m_ManualTurnOnOff = false;
	protected ref Timer m_ZenRoofCheckTimer;
	protected ref Timer m_ZenResetAlarmTimer;
	protected string m_ZenRuntimeStationId = "";
	protected bool m_ZenPlaySound = true;
	protected float m_ZenBatteryUsage = 1.0;
	protected bool m_ZenRadarRoofBlocked;
	protected bool m_ZenRadarRoofChecked;
	protected bool m_ZenRaidDiscordPending;

	// Client and/or Server
	protected int m_ZenRaidAlarmStatus = ZEN_ALARM_STATUS_OFF;
	protected int m_ZenRaidAlarmClientStatus = -1;
	protected EffectSound m_ZenSoundAlarmLoop;
	protected ZenRaidAlarmLight m_ZenBlinkingLight;

	// Constants 
	static const int ZEN_ALARM_STATUS_OFF = 0;
	static const int ZEN_ALARM_STATUS_ARMED = 1;
	static const int ZEN_ALARM_STATUS_TRIGGERED = 2;
	static const int ZEN_ALARM_STATUS_WASTRIGGERED = 3;

	///////////////////////////////////////////////////////////////////////
	//! CLIENT-ONLY 
	///////////////////////////////////////////////////////////////////////
#ifndef SERVER
	override void OnVariablesSynchronized()
	{
		super.OnVariablesSynchronized();

		// Check for new status sync
		if (m_ZenRaidAlarmClientStatus != m_ZenRaidAlarmStatus)
		{
			switch (m_ZenRaidAlarmStatus)
			{
			case ZEN_ALARM_STATUS_OFF:
				StopAlarmClient();
				break;
			case ZEN_ALARM_STATUS_ARMED:
				ArmAlarmClient();
				break;
			case ZEN_ALARM_STATUS_TRIGGERED:
				TriggerAlarmClient();
				break;
			case ZEN_ALARM_STATUS_WASTRIGGERED:
				StopAlarmSoundClient();
				break;
			}

			m_ZenRaidAlarmClientStatus = m_ZenRaidAlarmStatus;
		}
	}

	override string GetTooltip()
	{
		string temp;

		if (!DescriptionOverride(temp))
			temp = ConfigGetString("descriptionShort");

		string newTooltip = temp;
		if (GetCompEM() && m_ZenBatteryUsage > 0)
		{
			// Use the server-configured consumption rate synchronized with this station.
			newTooltip = newTooltip + " | " + ZenDiscordAPIFunctions.GetTimeToString(GetQuantity() / m_ZenBatteryUsage, true, true, false);
		}

		return newTooltip;
	}

	void CreateLight()
	{
		if (m_ZenBlinkingLight != null)
			return;

		m_ZenBlinkingLight = ZenRaidAlarmLight.Cast(ScriptedLightBase.CreateLightAtObjMemoryPoint(ZenRaidAlarmLight, this, "lightpos"));
		if (!m_ZenBlinkingLight)
			Error("[ZenDiscordAPI] Failed to create ZenRaidAlarmLight!");
	}

	void ArmAlarmClient()
	{
		CreateLight();
		m_ZenBlinkingLight.SetColor(0, 1, 0);
		m_ZenBlinkingLight.SetBlinkingSpeed(0);
	}

	void PlayAlarmSound()
	{
		if (!m_ZenPlaySound)
			return;

		if (!m_ZenSoundAlarmLoop)
		{
			if (GetMegaphone() != null && GetMegaphone().GetCompEM().CanWork())
			{
				PlaySoundSetLoop(m_ZenSoundAlarmLoop, "Zen_RaidAlarm_SoundSet", 0, 0);
			}
		}
	}

	void TriggerAlarmClient()
	{
		PlayAlarmSound();
		CreateLight();
		m_ZenBlinkingLight.SetColor(1, 0, 0);
		m_ZenBlinkingLight.SetBlinkingSpeed(10);
	}

	void StopAlarmSoundClient()
	{
		if (m_ZenSoundAlarmLoop)
			StopSoundSet(m_ZenSoundAlarmLoop);

		CreateLight();
		m_ZenBlinkingLight.SetColor(1, 0.6, 0);
		m_ZenBlinkingLight.SetBlinkingSpeed(0);
	}

	void StopAlarmClient()
	{
		if (m_ZenSoundAlarmLoop)
			StopSoundSet(m_ZenSoundAlarmLoop);

		CreateLight();
		m_ZenBlinkingLight.SetBlinkingSpeed(0);
		m_ZenBlinkingLight.SetBrightnessAdjusted(0);
	}
#endif

	///////////////////////////////////////////////////////////////////////
	//! SHARED
	///////////////////////////////////////////////////////////////////////
#ifdef GameLabs
	ref _Event _registeredInstanceZenGL;
#endif

	override void InitItemVariables()
	{
		super.InitItemVariables();

		#ifdef SERVER
		if (GetCompEM())
		{
			m_ZenPlaySound = GetZenRaidAlarmConfig().RaidPlaySound;
			m_ZenBatteryUsage = GetZenRaidAlarmConfig().RaidAlarmBatteryUsagePerSec;
			GetCompEM().SetEnergyUsage(m_ZenBatteryUsage);
		}
		#endif

		RegisterNetSyncVariableInt("m_ZenRaidAlarmStatus");
		RegisterNetSyncVariableBool("m_ZenPlaySound");
		RegisterNetSyncVariableFloat("m_ZenBatteryUsage", 0, 0, 3);
	}

	override void EEDelete(EntityAI parent)
	{
		if (g_Game && g_Game.IsDedicatedServer())
		{
			g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(InformConnection);
			g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(InformDisconnection);
			g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(InformWebhookUpdate);
			g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(SendRaidDiscordAlert);
			g_Game.GetCallQueue(CALL_CATEGORY_GAMEPLAY).Remove(InformConnection);
			g_Game.GetCallQueue(CALL_CATEGORY_GAMEPLAY).Remove(InformDisconnection);
			g_Game.GetCallQueue(CALL_CATEGORY_GAMEPLAY).Remove(InformWebhookUpdate);

			UnregisterRaidStation("Entity deleted.");

			if (m_ZenRoofCheckTimer)
			{
				m_ZenRoofCheckTimer.Stop();
				m_ZenRoofCheckTimer = null;
			}

			if (m_ZenResetAlarmTimer)
			{
				m_ZenResetAlarmTimer.Stop();
				m_ZenResetAlarmTimer = null;
			}

			g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(InformConnection);

			//InformDisconnectionSafeNoContext("", true);
		}

		super.EEDelete(parent);

		if (g_Game && g_Game.IsClient())
		{
			if (m_ZenSoundAlarmLoop)
				m_ZenSoundAlarmLoop.SoundStop();

			if (m_ZenBlinkingLight)
				m_ZenBlinkingLight.Destroy();
		}
	}

	int GetAlarmStatus()
	{
		return m_ZenRaidAlarmStatus;
	}

	override bool IsRefresherSignalingViable()
	{
		return !IsRuined();
	}

	override bool CanReceiveAttachment(EntityAI attachment, int slotId)
	{
		if (!super.CanReceiveAttachment(attachment, slotId))
			return false;

		if (slotId == InventorySlots.GetSlotIdFromString("Back"))
			return attachment.IsInherited(BaseRadio);

		return true;
	}

	bool HasWorkingBaseRadio()
	{
		if (!GetBaseRadio() || !GetBaseRadio().IsLockedInSlot())
			return false;

		CarBattery battery = GetCarBattery();
		if (!battery)
			return false;

		return battery.GetCompEM() && battery.GetCompEM().CanWork();
	}

	BaseRadio GetBaseRadio()
	{
		int slot = InventorySlots.GetSlotIdFromString("Back");
		BaseRadio radio = BaseRadio.Cast(GetInventory().FindAttachment(slot));
		if (!radio)
			return null;

		return radio;
	}

	CarBattery GetCarBattery()
	{
		BaseRadio radio = GetBaseRadio();
		if (!radio)
			return null;

		int slot = InventorySlots.GetSlotIdFromString("CarBattery");
		CarBattery battery = CarBattery.Cast(radio.GetInventory().FindAttachment(slot));
		if (!battery || battery.IsRuined())
			return null;

		return battery;
	}

	Megaphone GetMegaphone()
	{
		int slot = InventorySlots.GetSlotIdFromString("ZenMegaphone");
		Megaphone megaphone = Megaphone.Cast(GetInventory().FindAttachment(slot));
		if (!megaphone)
			return null;

		return megaphone;
	}

	override void SetActions()
	{
		super.SetActions();

		AddAction(ActionTurnOffZenRaidAlarm);
		AddAction(ActionTurnOnZenRaidAlarm);
		AddAction(ActionViewRaidAlarmGUI);
		AddAction(ActionFoldZenRaidAlarmKit);
	}

	override bool CanPutInCargo(EntityAI parent)
	{
		return false;
	}

	override bool CanPutIntoHands(EntityAI parent)
	{
		return false;
	}

	override void OnWorkStart()
	{
		super.OnWorkStart();

#ifdef SERVER
		UpdateBatteryEnergy();
		RegisterRaidStation();

#ifdef GameLabs 
		if (!_registeredInstanceZenGL)
		{
			_registeredInstanceZenGL = new _Event(GetType(), "bell", this);
			GetGameLabs().RegisterEvent(_registeredInstanceZenGL);
		}
#endif
#endif
	}

	override void OnWorkStop()
	{
		super.OnWorkStop();

#ifdef SERVER
		UpdateBatteryEnergy();

		if (m_ZenRoofCheckTimer)
			m_ZenRoofCheckTimer.Stop();

		if (!m_ZenDontNotifyAboutPower)
		{
			InformDisconnection();
		}
		
		UnregisterRaidStation("OnWorkStop.");

		if (GetAlarmStatus() != ZEN_ALARM_STATUS_WASTRIGGERED)
			SetAlarmStatus(ZEN_ALARM_STATUS_OFF);

		m_ZenDontNotifyAboutPower = false;

#ifdef GameLabs 
		if (GetGameLabs() && _registeredInstanceZenGL)
		{
			GetGameLabs().RemoveEvent(_registeredInstanceZenGL);
		}
#endif
#endif
	}

	override void OnWork(float consumed_energy)
	{
		super.OnWork(consumed_energy);

		//! CLIENT
#ifndef SERVER
		if (GetAlarmStatus() == ZEN_ALARM_STATUS_TRIGGERED)
		{
			// Do this here not on OnVarsSync() because otherwise upon login megaphone & battery is not detected in time.
			PlayAlarmSound();
		}

		return;
#endif

		//! SERVER ONWORK

		// If radar is required.... well, check for radar
		if (GetZenRaidAlarmConfig().RaidDiscordAlertRequiresRadar)
		{
			if (GetZenRaidAlarmConfig().RaidRadarDishCheckRoofTimerMinutes > 0 && (!m_ZenRoofCheckTimer || !m_ZenRoofCheckTimer.IsRunning()))
			{
				if (!m_ZenRoofCheckTimer)
					m_ZenRoofCheckTimer = new Timer();
				m_ZenRoofCheckTimer.Run(GetZenRaidAlarmConfig().RaidRadarDishCheckRoofTimerMinutes * 60, this, "CheckForRoof", null, true);
			}

			// Check discord radar status
			if (!GetRaidAlarmRadar(false))
			{
				if (m_ZenHadValidRadar)
				{
					InformDisconnection("", true);
					m_ZenHadValidRadar = false;
				}
			}
			else
			{
				if (!m_ZenHadValidRadar)
				{
					InformConnection();
					m_ZenHadValidRadar = true;
				}
			}
		}

		if (!GetCarBattery())
		{
			GetCompEM().SwitchOff();
			return;
		}

		// Check power status
		if (!m_ZenNotifiedLowPower && GetCompEM().IsWorking() && GetCompEM().GetEnergy0To1() < 0.1)
		{
			m_ZenNotifiedLowPower = true;
			InformLowBattery();
		}

		// Sync our energy to battery
		GetCarBattery().GetCompEM().SetEnergy0To1(GetCompEM().GetEnergy0To1());
		SetSynchDirty();
	}

	static bool GetTerritoryPermission(string uid, vector pos, PlayerBase player = null, bool checkMembership = false)
	{
		if (!checkMembership && !GetZenRaidAlarmConfig().RaidAlarmRequiresTerritory)
			return true;

		bool anyChecksPassed = false;

		// Check Expansion
#ifdef EXPANSIONMODBASEBUILDING
		ExpansionTerritoryModule expansionTerritoryModule = ExpansionTerritoryModule.Cast(CF_ModuleCoreManager.Get(ExpansionTerritoryModule));
		if (expansionTerritoryModule != null && expansionTerritoryModule.IsInsideOwnTerritoryOrPerimeter(pos, -1, -1, uid))
		{
			anyChecksPassed = true;
		}
#endif

		// Check basic territories by Daemonforge
#ifdef BASICTERRITORIES
		TerritoryFlag tflag = GetNearestTerritoryFlag(pos);
		if (tflag != null && tflag.HasRaisedFlag())
		{
			if (tflag.IsTerritoryMember(uid))
			{
				anyChecksPassed = true;
			}
		}
#endif

#ifdef ZenTerritories
		if (player)
		{
			TerritoryFlag zenFlag = GetZenTerritoryPlugin().GetNearestFlag(pos, TerritoryFlag.GetZenRefresherRadius() * 2);
			if (zenFlag)
			{
				if (zenFlag.BelongsToZenTerritory(player))
				{
					anyChecksPassed = true;
				}
			}
		}
#endif

		return anyChecksPassed;
	}

	///////////////////////////////////////////////////////////////////////
	//! SERVER-ONLY 
	///////////////////////////////////////////////////////////////////////

	array<string> GetWebhooks()
	{
		array<string> webhooks = new array<string>;
		webhooks.Insert(m_ZenWebhook1);
		webhooks.Insert(m_ZenWebhook2);
		webhooks.Insert(m_ZenWebhook3);
		return webhooks;
	}

	bool HasValidWebhook()
	{
		return m_ZenWebhook1 != "" || m_ZenWebhook2 != "" || m_ZenWebhook3 != "";
	}

	void SetWebhooks(array<string> p_hooks, string p_name = "")
	{
		if (!p_hooks)
			return;

		m_ZenWebhook1 = "";
		m_ZenWebhook2 = "";
		m_ZenWebhook3 = "";
		if (p_hooks.Count() >= 1)
			m_ZenWebhook1 = p_hooks.Get(0);

		if (p_hooks.Count() >= 2)
			m_ZenWebhook2 = p_hooks.Get(1);

		if (p_hooks.Count() >= 3)
			m_ZenWebhook3 = p_hooks.Get(2);

		InformWebhookUpdate(p_name);
		g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(InformConnection, 1500, false, p_name);
	}

	void SetBaseName(string p_name)
	{
		m_ZenBaseName = p_name;
	}

	string GetBaseName()
	{
		return m_ZenBaseName;
	}

	void InitBaseName()
	{
#ifdef EXPANSIONMODBASEBUILDING
		if (m_ZenBaseName == "")
		{
			TerritoryFlag theFlag = GetNearestTerritoryFlag(GetPosition());
			if (!theFlag)
				return;

			ExpansionTerritory territory = theFlag.GetTerritory();
			if (territory != null)
				m_ZenBaseName = territory.GetTerritoryName();
		}
#endif
	}

	void OpenRaidAlarmGUI(notnull PlayerIdentity identity)
	{
		GetRPCManager().SendRPC("ZenMod_RPC", "RPC_ReceiveZenRaidAlarmServerWebhooks", new Param3<array<string>, Zen_RaidAlarmStation, string>(GetWebhooks(), this, GetBaseName()), true, identity);
	}

	void CheckForRoof()
	{
		GetRaidAlarmRadar(true);
	}

	bool GetRaidAlarmRadar(bool checkForRoof = true)
	{
		if (!GetCompEM())
			return false;

		if (!GetZenRaidAlarmConfig().RaidDiscordAlertRequiresRadar)
			return true;

		Zen_RaidAlarmRadar previousRadar = m_ZenAttachedRaidRadar;
		m_ZenAttachedRaidRadar = null;
		CableReel cable = CableReel.Cast(GetCompEM().GetPluggedDevice());
		for (int link = 0; cable && link < 3; link++)
		{
			EntityAI device = cable.GetCompEM().GetPluggedDevice();
			m_ZenAttachedRaidRadar = Zen_RaidAlarmRadar.Cast(device);
			if (m_ZenAttachedRaidRadar)
				break;
			cable = CableReel.Cast(device);
		}

		if (previousRadar != m_ZenAttachedRaidRadar)
		{
			m_ZenRadarRoofChecked = false;
			m_ZenRadarRoofBlocked = false;
		}
		if (!m_ZenAttachedRaidRadar || m_ZenAttachedRaidRadar.IsRuined())
			return false;

		int roofMinutes = GetZenRaidAlarmConfig().RaidRadarDishCheckRoofTimerMinutes;
		if (roofMinutes < 0)
			m_ZenRadarRoofBlocked = false;
		else if (checkForRoof && (roofMinutes > 0 || !m_ZenRadarRoofChecked))
		{
			bool wasBlocked = m_ZenRadarRoofBlocked;
			m_ZenRadarRoofBlocked = MiscGameplayFunctions.IsUnderRoof(m_ZenAttachedRaidRadar, GameConstants.ROOF_CHECK_RAYCAST_DIST * 2);
			m_ZenRadarRoofChecked = true;
			if (m_ZenRadarRoofBlocked && !wasBlocked)
				InformObstruction();
		}

		return !m_ZenRadarRoofBlocked;
	}

	void UpdateBatteryEnergy()
	{
		float energy = 0;

		CarBattery battery = GetCarBattery();
		if (battery != null)
			energy = battery.GetCompEM().GetEnergy0To1();

		if (energy >= 0.1)
			m_ZenNotifiedLowPower = false;

		SetQuantity(GetQuantityMax() * energy);
		GetCompEM().SetEnergy0To1(energy);

		if (energy == 0)
			GetCompEM().SwitchOff();
	}

	void ResetRaidAlarm()
	{
		SetAlarmStatus(ZEN_ALARM_STATUS_WASTRIGGERED);
		m_ZenDontNotifyAboutPower = true;
		GetCompEM().SwitchOff();
	}

	void TurnOffAlarm(string playerName)
	{
		m_ManualTurnOnOff = true;
		m_ZenDontNotifyAboutPower = true;
		InformDisconnection(playerName, m_ZenHadValidRadar);
		GetCompEM().SwitchOff();
		GetCompEM().InteractBranch(this);
		SetAlarmStatus(ZEN_ALARM_STATUS_OFF);
	}

	void TurnOnAlarm(string playerName)
	{
		m_ManualTurnOnOff = true;
		GetCompEM().SwitchOn();
		GetCompEM().InteractBranch(this);
		SetAlarmStatus(ZEN_ALARM_STATUS_ARMED);
		InformConnection(playerName);
	}

	void TriggerRaidAlarm()
	{
		// If no power source or turned off, or we already triggered an alert this session, don't alert players
		if (!GetCompEM().IsWorking() || GetAlarmStatus() == ZEN_ALARM_STATUS_TRIGGERED)
			return;

		// Sync alarm state and schedule reset
		SetAlarmStatus(ZEN_ALARM_STATUS_TRIGGERED);

		// Alert any online players with notifications 
		NotifyAllTerritoryMembers();

		m_ZenRaidDiscordPending = true;
		SendRaidDiscordAlert();
	}

	void SendRaidDiscordAlert()
	{
		if (!m_ZenRaidDiscordPending)
			return;

		int remaining = GetZenRaidAlarmConfig().RaidDiscordAlertCooldownSecs * 1000 - (g_Game.GetTime() - m_ZenLastDiscordMessageTime);
		if (remaining > 0)
		{
			g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(SendRaidDiscordAlert);
			g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(SendRaidDiscordAlert, remaining + 1, false);
			return;
		}

		m_ZenRaidDiscordPending = false;
		if (!CanSendDiscordMessage())
			return;

		string footer = "" + GetZenEnergyCorrected() + "% " + GetZenRaidAlarmConfig().RaidAlarmBatteryWarning;
		ZenDiscordMessage msg = new ZenDiscordMessage(GetZenRaidAlarmConfig().RaidAlarmMessageTitle, ZEN_RAID_ICON);
		msg.SetTitle(GetZenRaidAlarmConfig().RaidAlarmMessageTitle);
		msg.SetMessage(GetZenRaidAlarmConfig().RaidAlarmAttackMessage + "\n\n" + ZenGameFunctions.GetMapLinkPosition(GetPosition(), m_ZenBaseName));
		msg.SetFooter(footer);
		msg.SetColor(255, 0, 0);
		msg.AddWebhooks(GetWebhooks());
		GetZenDiscordAPI().SendMessage(msg, "RaidAlarm", GetZenRuntimeStationId());
		m_ZenLastDiscordMessageTime = g_Game.GetTime();
	}

	//! TERRITORY STUFF 

	static TerritoryFlag GetNearestTerritoryFlag(vector pos)
	{
#ifdef ZenTerritories
		TerritoryFlag zenFlag = GetZenTerritoryPlugin().GetNearestFlag(pos, TerritoryFlag.GetZenRefresherRadius() * 2);
		if (zenFlag && zenFlag.IsValidZenTerritory())
			return zenFlag;
#endif
#ifdef EXPANSIONMODBASEBUILDING
		ExpansionTerritoryModule expansionTerritoryModule = ExpansionTerritoryModule.Cast(CF_ModuleCoreManager.Get(ExpansionTerritoryModule));
		if (expansionTerritoryModule != null)
		{
			TerritoryFlag tflag = expansionTerritoryModule.GetFlagAtPosition3D(pos);
			if (tflag != null)
				return tflag;
		}
#endif

#ifdef BASICTERRITORIES
		array<Object> objects = new array<Object>;
		array<CargoBase> proxyCargos = new array<CargoBase>;

		float theRadius = GameConstants.REFRESHER_RADIUS * 1.5;
		g_Game.GetObjectsAtPosition3D(pos, theRadius, objects, proxyCargos);
		TerritoryFlag theFlag;

		for (int i = 0; i < objects.Count(); i++)
		{
			if (Class.CastTo(theFlag, objects.Get(i)))
			{
				if (theFlag.HasRaisedFlag())
				{
					return theFlag;
				}
			}
		}
#endif

		return null;
	}

	void NotifyAllTerritoryMembers()
	{
		vector pos = GetPosition();
		string raidNotification = GetZenRaidAlarmConfig().RaidAlarmAttackMessage + "\n(" + m_ZenBaseName + " @ " + pos[0] + " / " + pos[2] + ")";

		array<Man> onlinePlayers = new array<Man>;
		g_Game.GetPlayers(onlinePlayers);
		PlayerBase pb;

		TerritoryFlag theFlag = GetNearestTerritoryFlag(GetPosition());
		if (!theFlag)
			return;

#ifdef ZenTerritories
		if (theFlag.IsValidZenTerritory())
		{
			foreach (Man zenMan : onlinePlayers)
			{
				pb = PlayerBase.Cast(zenMan);
				if (pb && pb.GetIdentity() && theFlag.BelongsToZenTerritory(pb) && HasRequiredAlarmItem(pb))
					NotificationSystem.SendNotificationToPlayerIdentityExtended(pb.GetIdentity(), 15.0, GetZenRaidAlarmConfig().RaidAlarmMessageTitle, raidNotification, "set:ccgui_enforce image:MapShieldBooster");
			}
			return;
		}
#endif

		// Check Expansion
#ifdef EXPANSIONMODBASEBUILDING
		ExpansionTerritory territory = theFlag.GetTerritory();
		if (!territory || territory.GetTerritoryMembers().Count() == 0)
			return;
		
		if (!territory.GetTerritoryMemberIDs())
			return;

		foreach(Man man1 : onlinePlayers)
		{
			pb = PlayerBase.Cast(man1);
			if (pb && pb.GetIdentity() && territory.IsMember(pb.GetCachedID()) && HasRequiredAlarmItem(pb))
			{
				NotificationSystem.SendNotificationToPlayerIdentityExtended(pb.GetIdentity(), 15.0, GetZenRaidAlarmConfig().RaidAlarmMessageTitle, raidNotification, "set:ccgui_enforce image:MapShieldBooster");
			}
		}

		return;
#endif

#ifdef BASICTERRITORIES
		// Check basic territories by Daemonforge
		foreach(Man man2 : onlinePlayers)
		{
			pb = PlayerBase.Cast(man2);
			if (pb && pb.GetIdentity())
			{
				if (theFlag.IsTerritoryMember(pb.GetCachedID()))
				{
					if (HasRequiredAlarmItem(pb))
					{
						NotificationSystem.SendNotificationToPlayerIdentityExtended(pb.GetIdentity(), 15.0, GetZenRaidAlarmConfig().RaidAlarmMessageTitle, raidNotification, "set:ccgui_enforce image:MapShieldBooster");
					}
				}
			}
		}
#endif
	}

	bool HasRequiredAlarmItem(notnull PlayerBase player)
	{
		if (!GetZenRaidAlarmConfig().RequireAnItemForIngameAlert)
			return true;

		array<EntityAI> itemsArray = new array<EntityAI>;
		player.GetInventory().EnumerateInventory(InventoryTraversalType.PREORDER, itemsArray);

		ItemBase itemInHands = player.GetItemInHands();
		if (itemInHands != null)
		{
			itemsArray.Insert(itemInHands);
		}

		ItemBase item;
		for (int i = 0; i < itemsArray.Count(); i++)
		{
			ItemBase.CastTo(item, itemsArray.Get(i));

			if (!item)
				continue;

			if (item.IsRuined())
				continue;

			string itemType = item.GetType();
			itemType.ToLower();

			// Check if item matches config types
			if (!GetZenRaidAlarmConfig().ItemsRequiredForIngameAlert.Contains(itemType))
				continue;

			// Check if battery is required
			if (!GetZenRaidAlarmConfig().ItemsRequiredForIngameAlert.Get(itemType))
				return true;

			if (!item.HasEnergyManager())
				continue;

			// CanWork() instead of IsWorking() - Syberia PDA isn't actually ever "on"... shouldn't really matter,
			// depends on immersive you wanna be I guess but I'll leave this here - feel free to edit & repack.
			if (!item.GetCompEM().CanWork())
				continue;

			return true;
		}

		return false;
	}

	void InformDisconnection(string playerName = "", bool overrideRadarCheck = false)
	{
		if (!CanSendDiscordMessage(overrideRadarCheck))
			return;

		string footer = playerName;
		if (footer != "")
			footer = footer + " | ";

		footer = footer + GetZenEnergyCorrected() + "% " + GetZenRaidAlarmConfig().RaidAlarmBatteryWarning;

		// Note: Hard ref is deleted when message is sent. Normally I don't have ref in function scope as it's a bad practice but without it the msg is null by the time it gets to DiscordAPI
		ref ZenDiscordMessage msg = new ZenDiscordMessage(GetZenRaidAlarmConfig().RaidAlarmMessageTitle, ZEN_RAID_ICON);
		msg.SetTitle(GetZenRaidAlarmConfig().RaidAlarmMessageTitle);
		msg.SetMessage(GetZenRaidAlarmConfig().RaidAlarmConnectionOffline + "\n\n" + ZenGameFunctions.GetMapLinkPosition(GetPosition(), m_ZenBaseName));
		msg.SetFooter(footer);
		msg.SetColor(255, 255, 0);
		msg.AddWebhooks(GetWebhooks());
		GetZenDiscordAPI().SendMessage(msg, "RaidAlarm", GetZenRuntimeStationId());
		m_ZenLastDiscordMessageTime = g_Game.GetTime();
	}

	void InformDisconnectionSafeNoContext(string playerName = "", bool overrideRadarCheck = false)
	{
		if (!HasValidWebhook())
			return;

		string footer = playerName;

		if (footer != "")
		{
			footer = footer + " | ";
		}

		if (GetCompEM())
		{
			footer = footer + GetZenEnergyCorrected() + "% " + GetZenRaidAlarmConfig().RaidAlarmBatteryWarning;
		}

		ref ZenDiscordMessage msg = new ZenDiscordMessage(GetZenRaidAlarmConfig().RaidAlarmMessageTitle, ZEN_RAID_ICON);
		msg.SetTitle(GetZenRaidAlarmConfig().RaidAlarmMessageTitle);
		msg.SetMessage(GetZenRaidAlarmConfig().RaidAlarmConnectionOffline + "\n\n" + ZenGameFunctions.GetMapLinkPosition(GetPosition(), m_ZenBaseName));
		msg.SetFooter(footer);
		msg.SetColor(255, 255, 0);
		msg.AddWebhooks(GetWebhooks());

		GetZenDiscordAPI().SendMessage(msg);
	}

	void InformConnection(string playerName = "")
	{
		if (!CanSendDiscordMessage())
			return;

		string footer = playerName;
		if (footer != "")
			footer = footer + " | ";

		footer = footer + GetZenEnergyCorrected() + "% " + GetZenRaidAlarmConfig().RaidAlarmBatteryWarning;

		// Note: Hard ref is deleted when message is sent. Normally I don't have ref in function scope as it's a bad practice but without it the msg is null by the time it gets to DiscordAPI
		ref ZenDiscordMessage msg = new ZenDiscordMessage(GetZenRaidAlarmConfig().RaidAlarmMessageTitle, ZEN_RAID_ICON);
		msg.SetTitle(GetZenRaidAlarmConfig().RaidAlarmMessageTitle);
		msg.SetMessage(GetZenRaidAlarmConfig().RaidAlarmConnectionOnline + "\n\n" + ZenGameFunctions.GetMapLinkPosition(GetPosition(), m_ZenBaseName));
		msg.SetFooter(footer);
		msg.SetColor(0, 255, 0);
		msg.AddWebhooks(GetWebhooks());
		GetZenDiscordAPI().SendMessage(msg, "RaidAlarm", GetZenRuntimeStationId());
		m_ZenLastDiscordMessageTime = g_Game.GetTime();
	}

	void InformLowBattery()
	{
		if (!CanSendDiscordMessage())
			return;

		// Note: Hard ref is deleted when message is sent. Normally I don't have ref in function scope as it's a bad practice but without it the msg is null by the time it gets to DiscordAPI
		ref ZenDiscordMessage msg = new ZenDiscordMessage(GetZenRaidAlarmConfig().RaidAlarmMessageTitle, ZEN_RAID_ICON);
		msg.SetTitle(GetZenRaidAlarmConfig().RaidAlarmMessageTitle);
		msg.SetMessage("" + GetZenEnergyCorrected() + "% " + GetZenRaidAlarmConfig().RaidAlarmBatteryWarning + "\n\n" + ZenGameFunctions.GetMapLinkPosition(GetPosition(), m_ZenBaseName));
		msg.SetColor(255, 165, 0);
		msg.AddWebhooks(GetWebhooks());
		GetZenDiscordAPI().SendMessage(msg, "RaidAlarm", GetZenRuntimeStationId());
		m_ZenLastDiscordMessageTime = g_Game.GetTime();
	}

	void InformWebhookUpdate(string playerName = "")
	{
		if (!CanSendDiscordMessage())
			return;

		string footer = playerName;
		if (footer != "")
			footer = footer + " | ";

		footer = footer + GetZenEnergyCorrected() + "% " + GetZenRaidAlarmConfig().RaidAlarmBatteryWarning;

		// Note: Hard ref is deleted when message is sent. Normally I don't have ref in function scope as it's a bad practice but without it the msg is null by the time it gets to DiscordAPI
		ref ZenDiscordMessage msg = new ZenDiscordMessage(GetZenRaidAlarmConfig().RaidAlarmMessageTitle, ZEN_RAID_ICON);
		msg.SetTitle(GetZenRaidAlarmConfig().RaidAlarmMessageTitle);
		msg.SetMessage(GetZenRaidAlarmConfig().RaidAlarmWebhookUpdate + "\n\n" + ZenGameFunctions.GetMapLinkPosition(GetPosition(), m_ZenBaseName));
		msg.SetFooter(footer);
		msg.SetColor(255, 255, 0);
		msg.AddWebhooks(GetWebhooks());
		GetZenDiscordAPI().SendMessage(msg, "RaidAlarm", GetZenRuntimeStationId());
		m_ZenLastDiscordMessageTime = g_Game.GetTime();
	}

	void InformObstruction()
	{
		if (!CanSendDiscordMessage(true))
			return;

		// Note: Hard ref is deleted when message is sent. Normally I don't have ref in function scope as it's a bad practice but without it the msg is null by the time it gets to DiscordAPI
		ref ZenDiscordMessage msg = new ZenDiscordMessage(GetZenRaidAlarmConfig().RaidAlarmMessageTitle, ZEN_RAID_ICON);
		msg.SetTitle(GetZenRaidAlarmConfig().RaidAlarmMessageTitle);
		msg.SetMessage(GetZenRaidAlarmConfig().RaidAlarmConnectionObstruction + "\n\n" + ZenGameFunctions.GetMapLinkPosition(GetPosition(), m_ZenBaseName));
		msg.SetColor(255, 255, 0);
		msg.AddWebhooks(GetWebhooks());
		GetZenDiscordAPI().SendMessage(msg, "RaidAlarm", GetZenRuntimeStationId());
		m_ZenLastDiscordMessageTime = g_Game.GetTime();
	}

	bool HasZenRaidDish()
	{
		if (!GetZenRaidAlarmConfig().RaidDiscordAlertRequiresRadar)
			return true;

		return m_ZenAttachedRaidRadar != null && !m_ZenAttachedRaidRadar.IsRuined() && !m_ZenRadarRoofBlocked;
	}

	bool CanSendDiscordMessage(bool overrideRadarCheck = false)
	{
		ProcessQueuedWebhookRemovals();

		if (!HasValidWebhook())
		{
			return false;
		}

		GetRaidAlarmRadar(!overrideRadarCheck);

		// If no radar connected to this station and config disables alert, don't alert discord.
		if (!HasZenRaidDish() && GetZenRaidAlarmConfig().RaidDiscordAlertRequiresRadar && !overrideRadarCheck)
		{
			return false;
		}

		if (g_Game.GetTime() - m_ZenLastDiscordMessageTime < (GetZenRaidAlarmConfig().RaidDiscordAlertCooldownSecs * 1000))
		{
			return false;
		}

		return true;
	}

	void SetAlarmStatus(int status)
	{
		if (m_ZenResetAlarmTimer)
			m_ZenResetAlarmTimer.Stop();
		if (status == ZEN_ALARM_STATUS_OFF || status == ZEN_ALARM_STATUS_ARMED)
		{
			m_ZenRaidDiscordPending = false;
			g_Game.GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(SendRaidDiscordAlert);
		}

		m_ZenRaidAlarmStatus = status;
		SetSynchDirty();

		if (m_ZenRaidAlarmStatus == ZEN_ALARM_STATUS_TRIGGERED)
		{
			if (m_ZenResetAlarmTimer)
				m_ZenResetAlarmTimer.Stop();
			else
				m_ZenResetAlarmTimer = new Timer();
			
			m_ZenResetAlarmTimer.Run(Math.Max(0.1, GetZenRaidAlarmConfig().RaidAlarmResetInMinutes * 60), this, "ResetRaidAlarm", null, false);
		}
	}

	override bool EEOnDamageCalculated(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
	{
		if (g_Game.IsDedicatedServer())
		{
			if (super.EEOnDamageCalculated(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef))
			{
				if (GetZenRaidAlarmConfig().RaidStationInvincible)
				{
					return false;
				}

				if (GetZenRaidAlarmConfig().BaseDamageTriggersRaidAlert)
				{
					ZenRaidAlarmPlugin plugin = ZenRaidAlarmPlugin.Cast(GetPlugin(ZenRaidAlarmPlugin));
					plugin.AlertNearestRaidStation(GetPosition());
				}
			
				return true;
			}
		}

		return false;
	}

	override void EEItemAttached(EntityAI item, string slot_name)
	{
		super.EEItemAttached(item, slot_name);

#ifdef SERVER
		if (item.IsInherited(BaseRadio))
		{
			// Turn battery off when attached - we now interact with the battery through raid mod.
			BaseRadio radio = BaseRadio.Cast(item);
			if (radio.GetCompEM() && radio.GetCompEM().IsWorking())
			{
				radio.GetCompEM().SwitchOff();
			}
		}
#endif
	}

	void LockBaseRadio(bool lock)
	{
		BaseRadio radio = GetBaseRadio();

		if (!radio)
			return;

		if (lock)
		{
			if (!radio.IsLockedInSlot())
			{
				radio.LockToParent();
				UpdateBatteryEnergy();
			}

			if (HasWorkingBaseRadio() && GetCompEM() && GetCompEM().IsWorking() && GetRaidAlarmRadar(false))
			{
				RegisterRaidStation();
			}

			return;
		}

		if (radio.IsLockedInSlot())
		{
			m_ZenDontNotifyAboutPower = true;

			InformDisconnection("", true);
			UnregisterRaidStation("Unlocked from base.");

			m_ZenWebhook1 = "";
			m_ZenWebhook2 = "";
			m_ZenWebhook3 = "";
			m_ZenWebhook1_FailCount = 0;
			m_ZenWebhook2_FailCount = 0;
			m_ZenWebhook3_FailCount = 0;
			m_ZenBaseName = "";

			radio.UnlockFromParent();

			if (GetCompEM())
			{
				GetCompEM().SwitchOff();
				GetCompEM().InteractBranch(this);
			}

			SetAlarmStatus(ZEN_ALARM_STATUS_OFF);
			m_ZenHadValidRadar = false;
			m_ZenNotifiedLowPower = false;

			SetSynchDirty();

			ZMPrint("[ZenRaidAlarm] Base radio unlocked @ " + GetPosition() + " - unregistered station and wiped webhooks.");
		}
	}

	void RegisterRaidStation()
	{
		if (!HasWorkingBaseRadio())
			return;

		ZenRaidAlarmPlugin plugin = ZenRaidAlarmPlugin.Cast(GetPlugin(ZenRaidAlarmPlugin));
		if (plugin)
			plugin.RegisterRaidStation(this);
	}

	void UnregisterRaidStation(string whyMsg)
	{
		ZenRaidAlarmPlugin plugin = ZenRaidAlarmPlugin.Cast(GetPlugin(ZenRaidAlarmPlugin));
		if (plugin)
		{
			plugin.UnregisterRaidStation(this, whyMsg);
		}
	}

	override void EEHealthLevelChanged(int oldLevel, int newLevel, string zone)
	{
		super.EEHealthLevelChanged(oldLevel, newLevel, zone);

#ifdef SERVER
		if (newLevel == GameConstants.STATE_RUINED)
		{
			MiscGameplayFunctions.DropAllItemsInInventoryInBounds(this, vector.Zero);
		}
#endif
	}

	override void OnStoreSave(ParamsWriteContext ctx)
	{
		super.OnStoreSave(ctx);

		// Save webhook array data
		ctx.Write(m_ZenBaseName);
		ctx.Write(m_ZenWebhook1);
		ctx.Write(m_ZenWebhook2);
		ctx.Write(m_ZenWebhook3);
		ctx.Write(m_ZenRaidAlarmStatus);
		ctx.Write(m_ZenHadValidRadar);
		ctx.Write(m_ZenWebhook1_FailCount);
		ctx.Write(m_ZenWebhook2_FailCount);
		ctx.Write(m_ZenWebhook3_FailCount);
	}

	override bool OnStoreLoad(ParamsReadContext ctx, int version)
	{
		if (!super.OnStoreLoad(ctx, version))
			return false;

		// Get name 
		if (!ctx.Read(m_ZenBaseName))
			return false;

		// Get webhooks
		if (!ctx.Read(m_ZenWebhook1))
			return false;

		if (!ctx.Read(m_ZenWebhook2))
			return false;

		if (!ctx.Read(m_ZenWebhook3))
			return false;

		if (!ctx.Read(m_ZenRaidAlarmStatus))
			return false;

		if (!ctx.Read(m_ZenHadValidRadar))
			return false;
		
		if (!ctx.Read(m_ZenWebhook1_FailCount))
			return false;
		
		if (!ctx.Read(m_ZenWebhook2_FailCount))
			return false;
		
		if (!ctx.Read(m_ZenWebhook3_FailCount))
			return false;

		return true;
	}

	override void AfterStoreLoad()
	{
		super.AfterStoreLoad();

		SetAlarmStatus(m_ZenRaidAlarmStatus);
	}

	void SetZenRuntimeStationId(string id)
	{
		m_ZenRuntimeStationId = id;
	}

	string GetZenRuntimeStationId()
	{
		return m_ZenRuntimeStationId;
	}
	
	int GetZenEnergyCorrected()
	{
		return (int)Math.Round(GetCompEM().GetEnergy0To1() * 100);
	}
	
	void OnDiscordWebhookSucceeded(string webhook)
	{
		ZMPrint("[ZenRaidAlarm] Successful webhook received.");
		
		// reset failure counter for this webhook
		if (webhook == m_ZenWebhook1)
		{
			m_ZenWebhook1_FailCount = 0;
		}
		else
		if (webhook == m_ZenWebhook2)
		{
			m_ZenWebhook2_FailCount = 0;
		}
		else
		if (webhook == m_ZenWebhook3)
		{
			m_ZenWebhook3_FailCount = 0;
		}
	}
	
	void OnDiscordWebhookFailed(string webhook, int errorCode)
	{
		// Never treat errorCode 8 as failure (handled in API already, but belt+suspenders)
		if (errorCode == ERestResultState.EREST_ERROR_APPERROR)
			return;

		if (webhook == m_ZenWebhook1)
		{
			m_ZenWebhook1_FailCount++;
			
			ZMPrint("[ZenRaidAlarm] Error code " + errorCode + " received @ " + GetPosition() + " - failCount=" + m_ZenWebhook1_FailCount + " for hook: " + webhook);
			
			if (m_ZenWebhook1_FailCount > GetZenRaidAlarmConfig().RaidRemoveWebhookFailCount)
			{
				RemoveStoredWebhook(webhook);
				m_ZenWebhook1_FailCount = 0;
				return;
			}
		}
		else
		if (webhook == m_ZenWebhook2)
		{
			m_ZenWebhook2_FailCount++;
			
			ZMPrint("[ZenRaidAlarm] Error code " + errorCode + " received @ " + GetPosition() + " - failCount=" + m_ZenWebhook2_FailCount + " for hook: " + webhook);
			
			if (m_ZenWebhook2_FailCount > GetZenRaidAlarmConfig().RaidRemoveWebhookFailCount)
			{
				RemoveStoredWebhook(webhook);
				m_ZenWebhook2_FailCount = 0;
				return;
			}
		}
		else
		if (webhook == m_ZenWebhook3)
		{
			m_ZenWebhook3_FailCount++;
			
			ZMPrint("[ZenRaidAlarm] Error code " + errorCode + " received @ " + GetPosition() + " - failCount=" + m_ZenWebhook3_FailCount + " for hook: " + webhook);
			
			if (m_ZenWebhook3_FailCount > GetZenRaidAlarmConfig().RaidRemoveWebhookFailCount)
			{
				RemoveStoredWebhook(webhook);
				m_ZenWebhook3_FailCount = 0;
				return;
			}
		}
	}
	
	protected void RemoveStoredWebhook(string bad)
	{
		ZMPrint("[ZenRaidAlarm] Removed failing webhook @ " + GetPosition());
		
		array<string> current = GetWebhooks();
		ref array<string> clean = new array<string>;
	
		foreach (string h : current)
		{
			if (h == "" || h == bad)
				continue;
	
			if (clean.Find(h) == -1)
				clean.Insert(h);
		}
	
		// Clear + repack to keep slots compact
		m_ZenWebhook1 = "";
		m_ZenWebhook2 = "";
		m_ZenWebhook3 = "";
	
		if (clean.Count() >= 1) m_ZenWebhook1 = clean[0];
		if (clean.Count() >= 2) m_ZenWebhook2 = clean[1];
		if (clean.Count() >= 3) m_ZenWebhook3 = clean[2];
	
		// Not strictly required for persistence (OnStoreSave will persist later),
		// but safe if you want immediate state updates.
		SetSynchDirty();
	}

	void ProcessQueuedWebhookRemovals()
	{
		if (m_ZenRuntimeStationId == "")
			return;

		ZenRaidAlarmPlugin plugin = ZenRaidAlarmPlugin.Cast(GetPlugin(ZenRaidAlarmPlugin));

		if (!plugin)
			return;

		array<string> removals = plugin.TakeQueuedWebhookRemovals(m_ZenRuntimeStationId);

		if (!removals)
			return;

		foreach (string webhook : removals)
		{
			if (webhook != "")
			{
				RemoveStoredWebhook(webhook);
			}
		}
	}
}
