modded class MissionServer
{
	override void InvokeOnConnect(PlayerBase player, PlayerIdentity identity) 
	{
		super.InvokeOnConnect(player, identity);

		if (!identity || !player)
			return;

		GetRPCManager().SendRPC(ZEN_RPC, "RPC_ReceiveClientSyncConfig", new Param2<array<string>, array<string>>(GetZenRaidAlarmConfig().RaidAlarmBuildTools, GetZenRaidAlarmConfig().RaidAlarmDismantleTools), true, identity);
	}
}