modded class MissionBase
{
	//! SHARED 
	void MissionBase()
	{
		#ifdef SERVER
		// SERVER RECEIVE RPC

		//! RAID ALARM
        GetRPCManager().AddRPC(ZEN_RPC, "RPC_ReceiveZenRaidAlarmClientWebhooks", this, SingeplayerExecutionType.Server);
		#else
		// CLIENT RECEIVE RPC

		//! RAID ALARM
		GetRPCManager().AddRPC(ZEN_RPC, "RPC_ReceiveZenRaidAlarmServerWebhooks", this, SingeplayerExecutionType.Client);
        GetRPCManager().AddRPC(ZEN_RPC, "RPC_ReceiveClientSyncConfig", this, SingeplayerExecutionType.Client);
		#endif
	}

    // Client -> server :: receive webhooks on server
	void RPC_ReceiveZenRaidAlarmClientWebhooks(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type == CallType.Server && g_Game.IsDedicatedServer())
        {
            Param3<array<string>, Zen_RaidAlarmStation, string> data;
            if (!ctx.Read(data))
            {
                Error("Error receiving data - RPC_ReceiveZenRaidAlarmClientWebhooks");
                return;
            }

            if (data.param1.Count() != 3)
            {
                Error("Error sync'ing client-side RaidStation data to server - RPC_ReceiveZenRaidAlarmClientWebhooks - count=" + data.param1.Count());
                return;
            }

            if (!data.param2)
            {
                Error("Error sync'ing client-side RaidStation data to server - RPC_ReceiveZenRaidAlarmClientWebhooks");
                return;
            }

            if (sender && data.param1 && data.param2)
            {
				string playerName = "Survivor";
				PlayerBase pb = PlayerBase.Cast(sender.GetPlayer());
				if (pb)
				{
					playerName = pb.GetCachedName();
				}
				
                data.param2.SetWebhooks(data.param1, playerName);
                data.param2.SetBaseName(data.param3);
            }
        }
    }

    // Server -> client :: receive webhooks on client
    void RPC_ReceiveZenRaidAlarmServerWebhooks(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
        if (type == CallType.Client && g_Game.IsClient())
        {
            Param3<array<string>, Zen_RaidAlarmStation, string> data;
            if (!ctx.Read(data))
            {
                Error("Error sync'ing server-side data to client - RPC_ReceiveZenRaidAlarmServerWebhooks");
                return;
            }

            if (data.param1.Count() != 3)
            {
                Error("Error sync'ing server-side webhook data to client - RPC_ReceiveZenRaidAlarmServerWebhooks - count=" + data.param1.Count());
                return;
            }

            if (!data.param2)
            {
                Error("Error sync'ing server-side Raid Station data to client - RPC_ReceiveZenRaidAlarmServerWebhooks");
                return;
            }

            if (g_Game.GetUIManager() != NULL)
            {
                ZenRaidAlarmGUI gui = ZenRaidAlarmGUI.Cast(g_Game.GetUIManager().EnterScriptedMenu(ZenMenusDiscordAPI.RAID_ALARM_GUI, NULL));
                if (gui)
                    gui.OnReceivedServerData(data.param1.Get(0), data.param1.Get(1), data.param1.Get(2), data.param2, data.param3);
            }
        }
    }

    // Server -> client :: receive config on client
    void RPC_ReceiveClientSyncConfig(CallType type, ParamsReadContext ctx, PlayerIdentity sender, Object target)
    {
	    Param2<array<string>, array<string>> data;
	    if (!ctx.Read(data))
	    {
	        Error("Error sync'ing server-side data to client - RPC_ReceiveClientSyncConfig");
	        return;
	    }
	
	    GetZenRaidAlarmClientSyncConfig().RaidAlarmBuildTools.Clear();
	    GetZenRaidAlarmClientSyncConfig().RaidAlarmDismantleTools.Clear();
	
	    int i;
	
	    for (i = 0; i < data.param1.Count(); i++)
	    {
	        GetZenRaidAlarmClientSyncConfig().RaidAlarmBuildTools.Insert(data.param1.Get(i));
	    }
	
	    for (i = 0; i < data.param2.Count(); i++)
	    {
	        GetZenRaidAlarmClientSyncConfig().RaidAlarmDismantleTools.Insert(data.param2.Get(i));
	    }
    }

	override UIScriptedMenu CreateScriptedMenu(int id)
    {
        UIScriptedMenu menu = super.CreateScriptedMenu(id);

        if (!menu)
        {
            switch (id)
            {
				//! RAID ALARM
                case ZenMenusDiscordAPI.RAID_ALARM_GUI:
                {
                    menu = new ZenRaidAlarmGUI;
                    break;
                }
            }
        }

        return menu;
    }
}