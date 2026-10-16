class ActionTurnOffZenRaidAlarm: ActionInteractBase
{	
	void ActionTurnOffZenRaidAlarm()
	{
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_INTERACTONCE;
		m_Text = "#switch_off";
	}

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		Object targetObject = target.GetObject();
		ItemBase target_IB = ItemBase.Cast(targetObject);
		
		if (target_IB.HasEnergyManager() && target_IB.GetCompEM().IsWorking())
			return true;

		return false;
	}

	override void OnExecuteServer(ActionData action_data)
	{
		if (!action_data.m_Player || !action_data.m_Player.GetIdentity())
			return;
	
		Zen_RaidAlarmStation station = Zen_RaidAlarmStation.Cast(action_data.m_Target.GetObject());
		if (station)
		{
			string playerDetails = action_data.m_Player.GetCachedName() + "(" + action_data.m_Player.GetCachedID() + ")";
			
			float dist = vector.Distance(action_data.m_Player.GetPosition(), station.GetPosition());
			if (dist > 3)
			{
				string errorMsg = "[ZenRaidAlarm] Anticheat - Player " + playerDetails + " was " + dist + "m away and attempted to turn off raid alarm @ " + station.GetPosition() + " - either lag/desync, or potential cheater.";
				
				Error(errorMsg);
				
				ZenDiscordMessage msg = new ZenDiscordMessage(GetZenRaidAlarmConfig().RaidAlarmMessageTitle, ZEN_RAID_ICON);
				msg.SetTitle(GetZenRaidAlarmConfig().RaidAlarmMessageTitle);
				msg.SetMessage(errorMsg, false);
				msg.SetColor(255, 0, 0);
				msg.AddWebhooks(GetZenRaidAlarmConfig().AdminWebhooks);

				GetZenDiscordAPI().SendMessage(msg);
				
				return;
			}
			
			station.TurnOffAlarm(action_data.m_Player.GetCachedName());
			ZMPrint("[ZenRaidAlarm] Player " + action_data.m_Player.GetCachedName() + "(" + action_data.m_Player.GetCachedID() + ") turned OFF raid station @ " + station.GetPosition());
		}
	}
}
