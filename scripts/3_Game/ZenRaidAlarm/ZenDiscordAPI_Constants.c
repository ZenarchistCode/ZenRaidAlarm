enum ZenMenusDiscordAPI
{
	RAID_ALARM_GUI = 43761233
};

class ZenRaidAlarmWebhookContext
{
	string StationId;

	void ZenRaidAlarmWebhookContext(string stationId)
	{
		StationId = stationId;
	}
}

static const string ZEN_RAID_ICON = "https://i.imgur.com/dRcPIht.png";