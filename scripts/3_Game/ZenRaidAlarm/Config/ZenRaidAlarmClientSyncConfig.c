ref ZenRaidAlarmClientSyncConfig g_ZenRaidAlarmClientConfig;

static ZenRaidAlarmClientSyncConfig GetZenRaidAlarmClientSyncConfig()
{
	if (!g_ZenRaidAlarmClientConfig)
		g_ZenRaidAlarmClientConfig = new ZenRaidAlarmClientSyncConfig();
	
	return g_ZenRaidAlarmClientConfig;
}

class ZenRaidAlarmClientSyncConfig
{
	ref array<string> RaidAlarmBuildTools;
	ref array<string> RaidAlarmDismantleTools;
	
	void ZenRaidAlarmClientSyncConfig()
	{
		RaidAlarmBuildTools = new array<string>;
		RaidAlarmDismantleTools = new array<string>;
	}
}