class ZenRaidAlarmPlugin extends PluginBase
{
    protected ref array<Zen_RaidAlarmStation> m_RaidStations;
    protected ref map<string, int> m_RaidTriggers; // Can't use vector map? Fuck it, use a string map then

    override void OnInit()
    {
        ZenPrint("[ZenRaidAlarmPlugin] :: OnInit");
        m_RaidStations = new array<Zen_RaidAlarmStation>;
        m_RaidTriggers = new map<string, int>;
		
		GetZenDiscordAPI().GetOnWebhookError().Insert(OnDiscordWebhookError);
		GetZenDiscordAPI().GetOnWebhookSuccess().Insert(OnDiscordWebhookSuccess);
    }
	
	void ~ZenRaidAlarmPlugin()
	{
		GetZenDiscordAPI().GetOnWebhookError().Remove(OnDiscordWebhookError);
		GetZenDiscordAPI().GetOnWebhookSuccess().Remove(OnDiscordWebhookSuccess);
	}
	
	void OnDiscordWebhookError(ZenDiscordWebhookCB cb, int errorCode)
	{
		Zen_RaidAlarmStation st = Zen_RaidAlarmStation.Cast(cb.GetContext());
		if (!st) 
			return;

		st.OnDiscordWebhookFailed(cb.GetWebhook(), errorCode);
	}

	void OnDiscordWebhookSuccess(ZenDiscordWebhookCB cb, string data, int dataSize)
	{
		Zen_RaidAlarmStation st = Zen_RaidAlarmStation.Cast(cb.GetContext());
		if (!st) 
			return;

		st.OnDiscordWebhookSucceeded(cb.GetWebhook());
	}

    array<Zen_RaidAlarmStation> GetRaidStations()
    {
        return m_RaidStations;
    }

    void RegisterRaidStation(Zen_RaidAlarmStation station)
    {
        if (m_RaidStations.Find(station) == -1)
        {
            m_RaidStations.Insert(station);
            g_Game.GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(DelayedRegisterMessage, 1000, false, station);
        }
    }

    void UnregisterRaidStation(Zen_RaidAlarmStation station)
    {
        int idx = m_RaidStations.Find(station);
        if (idx != -1)
        {
            m_RaidStations.Remove(idx);
            ZenPrint("[ZenRaidAlarmPlugin] Unregistered raid station @ " + station.GetPosition());
        }
    }

    // CompEM Switches on before webhooks are loaded from storage .bin files which "registers" the raid alarm
    // before we have the webhooks ready, so I delay the registration message by 1 sec to allow time to load the webhooks
    void DelayedRegisterMessage(Zen_RaidAlarmStation station)
    {
        if (station != NULL)
        {
            ZenPrint("[ZenRaidAlarmPlugin] Registered raid station @ " + station.GetPosition() + " - Has Valid Webhook: " + station.HasValidWebhook());
        }
    }

    Zen_RaidAlarmStation GetNearestRaidStation(vector pos)
    {
        for (int i = 0; i < m_RaidStations.Count(); i++)
        {
            Zen_RaidAlarmStation station = m_RaidStations.Get(i);

            if (!station)
            {
                m_RaidStations.Remove(i);
                i--;
                continue;
            }

            if (station.IsSetForDeletion())
            {
                m_RaidStations.Remove(i);
                i--;
                continue;
            }

            if (vector.Distance(station.GetPosition(), pos) <= GetZenRaidAlarmConfig().RaidDetectionDistance)
                return station;
        }

        return NULL;
    }

    void AlertNearestRaidStation(vector objectPos)
    {
        // Trigger player webhooks (and admin webhooks if station is detected)
        Zen_RaidAlarmStation station = GetNearestRaidStation(objectPos);
        if (station)
            station.TriggerRaidAlarm();

        for (int i = 0; i < m_RaidTriggers.Count(); i++)
        {
            if (vector.Distance(m_RaidTriggers.GetKey(i).ToVector(), objectPos) <= GetZenRaidAlarmConfig().RaidDetectionDistance)
            {
                // We've already been notified of this raid this session
                if (g_Game.GetTickTime() - m_RaidTriggers.GetElement(i) < 3600)
                    return;
            }
        }

#ifdef GAMELABS
        Zen_RaidAlarm_GameLabsDummy gameLabsDummy = Zen_RaidAlarm_GameLabsDummy.Cast(g_Game.CreateObject("Zen_RaidAlarm_GameLabsDummy", objectPos));
        ZenDiscordAPIFunctions.GameLabs_SendServerDeployed(objectPos, "RAID IN PROGRESS!");
#endif

        // Trigger admin webhooks - only if no station is detected. Otherwise we send the same player alert to the admins in TriggerRaidAlarm() ^
        if (GetZenRaidAlarmConfig().TriggerAdminRaidAlert)
        {
            // Collect player IDs involved in raid (victims included)
            int playersInvolvedCount = 0;
            string playersInvolved = "";
            array<Man> onlinePlayers = new array<Man>;
		    g_Game.GetPlayers(onlinePlayers);
            foreach (Man man : onlinePlayers)
            {
                PlayerBase pb = PlayerBase.Cast(man);
                if (pb != NULL && vector.Distance(objectPos, pb.GetPosition()) <= GetZenRaidAlarmConfig().RaidDetectionDistance)
                {
                    playersInvolved = playersInvolved + "[" + pb.GetCachedName() + "](http://steamcommunity.com/profiles/" + pb.GetIdentity().GetPlainId() + ")" + " (" + pb.GetIdentity().GetId() + ")\n";
                    playersInvolvedCount++;
                }
            }

            string mapLink = "";
		    if (GetZenCoreConfig().MapIzurviveURL == "")
			    mapLink = " @ " + objectPos[0] + " / " + objectPos[2];
            else
		        mapLink = "@ [" + objectPos[0] + " / " + objectPos[2] + "](" + GetZenCoreConfig().MapIzurviveURL + "#location=" + objectPos[0] + ";" + objectPos[2] + ")";

            // Craft alert 
            ZenDiscordMessage msg = new ZenDiscordMessage(GetZenRaidAlarmConfig().RaidAlarmMessageTitle, ZEN_RAID_ICON);
		    msg.SetTitle(GetZenRaidAlarmConfig().RaidAlarmMessageTitle);
		    msg.SetMessage("A raid is occuring:\n\n" + mapLink + "\n\n" + playersInvolvedCount + " Players Involved : \n" + playersInvolved);
		    msg.SetColor(255, 0, 0);
		    msg.AddWebhooks(GetZenRaidAlarmConfig().AdminWebhooks);
		    GetZenDiscordAPI().SendMessage(msg);
            m_RaidTriggers.Set(objectPos.ToString(false), g_Game.GetTickTime());
        }
    }

    static void ZenPrint(string msg)
    {
        #ifdef ZenModPack
        ZMPrint(msg);
        #else
        Print(msg);
        #endif
    }
}