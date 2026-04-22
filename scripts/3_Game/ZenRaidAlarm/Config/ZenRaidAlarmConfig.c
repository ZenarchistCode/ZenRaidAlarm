ref ZenRaidAlarmConfig g_ZenRaidAlarmConfig;

static ZenRaidAlarmConfig GetZenRaidAlarmConfig()
{
	if (!g_ZenRaidAlarmConfig) GetZenConfigRegister().RegisterConfig(ZenRaidAlarmConfig);
	return g_ZenRaidAlarmConfig;
}

modded class ZenConfigRegister
{
	override void RegisterPreload()
	{
		super.RegisterPreload(); 
		RegisterType(ZenRaidAlarmConfig);
	}
}

class ZenRaidAlarmConfig : ZenConfigBase
{
	// -------------------------
	// CONFIG SETTINGS
	// -------------------------
	override void OnRegistered()
	{
		g_ZenRaidAlarmConfig = this;
	}

	override string    	GetCurrentVersion()   		{ return "1.29.1"; }
	override bool		ShouldLoadOnServer() 		{ return true; }
	override bool 		IsServerOnlyConfig() 		{ return true; }
	
	override bool ReadJson(string path, out string err)
	{
		return JsonFileLoader<ZenRaidAlarmConfig>.LoadFile(path, this, err);
	}

	override bool WriteJson(string path, out string err)
	{
		return JsonFileLoader<ZenRaidAlarmConfig>.SaveFile(path, this, err);
	}
	
	// -------------------------
	// CONFIG VARS
	// -------------------------
	ref array<string> AdminWebhooks;
	bool TriggerAdminRaidAlert;
	bool RaidAlarmRequiresTerritory;
	bool RaidStationInvincible;
	bool RaidRadarDishInvincible;
	int RaidRadarDishCheckRoofTimerMinutes;
	int RaidDetectionDistance;
	float RaidAlarmResetInMinutes;
	float RaidAlarmBatteryUsagePerSec;
	bool RaidPlaySound;
	bool RaidDiscordAlertRequiresRadar;
	int RaidDiscordAlertCooldownSecs;
	int RaidRemoveWebhookFailCount;
	string RaidAlarmMessageTitle;
	string RaidAlarmAttackMessage;
	string RaidAlarmWebhookUpdate;
	string RaidAlarmConnectionOnline;
	string RaidAlarmConnectionOffline;
	string RaidAlarmRequiresTerritoryText;
	string RaidAlarmTooClose;
	string RaidAlarmConnectionObstruction;
	string RaidAlarmBatteryWarning;
	bool BaseDamageTriggersRaidAlert;
	bool BaseDismantleTriggersRaidAlert;
	bool ItemDamageTriggersRaidAlert;
	ref array<string> ItemsDamagedDetect;
	ref array<string> RaidAlarmBuildTools;
	ref array<string> RaidAlarmDismantleTools;
	ref array<string> ItemsDeployedTriggerRaidAlert;
	bool RequireAnItemForIngameAlert;
	ref map<string, bool> ItemsRequiredForIngameAlert;

	override void AfterLoad()
	{
		super.AfterLoad();
		
		// Convert all arrays to lower case 
		string entry;
		int i;
		for (i = 0; i < ItemsDeployedTriggerRaidAlert.Count(); i++)
		{
			entry = ItemsDeployedTriggerRaidAlert.Get(i);
			entry.ToLower();
			ItemsDeployedTriggerRaidAlert.Set(i, entry);
		}

		for (i = 0; i < RaidAlarmBuildTools.Count(); i++)
		{
			entry = RaidAlarmBuildTools.Get(i);
			entry.ToLower();
			RaidAlarmBuildTools.Set(i, entry);
		}

		for (i = 0; i < RaidAlarmDismantleTools.Count(); i++)
		{
			entry = RaidAlarmDismantleTools.Get(i);
			entry.ToLower();
			RaidAlarmDismantleTools.Set(i, entry);
		}
		
		GetZenRaidAlarmClientSyncConfig();
		GetZenRaidAlarmClientSyncConfig().RaidAlarmBuildTools.Copy(RaidAlarmBuildTools);
		GetZenRaidAlarmClientSyncConfig().RaidAlarmDismantleTools.Copy(RaidAlarmDismantleTools);
	}

	override void SetDefaults()
	{
		AdminWebhooks = new array<string>;

		// If any of these player IDs log in it will trigger an alert to the admin discord webhook
		AdminWebhooks.Insert("https://discord.com/api/webhooks/1263661792194728040/AEWqPnfHW8h8sIJuFBKsjW1mtOHFANXtWQVh3oquwcevG0aO2CyowmP4IQuKLqOFHJno");

		ItemsDamagedDetect = new array<string>;
		ItemsDamagedDetect.Insert("BaseBuilding");
		
		// If any of these items are detected being deployed it will trigger raid alarm
		ItemsDeployedTriggerRaidAlert = new array<string>;
		ItemsDeployedTriggerRaidAlert.Insert("HDSN_BreachingChargeBase");
		ItemsDeployedTriggerRaidAlert.Insert("expansionsatchel");
		ItemsDeployedTriggerRaidAlert.Insert("plastic_explosive");

		// map = itemType, bool: bool is requires battery working & on
		ItemsRequiredForIngameAlert = new map<string, bool>;
		ItemsRequiredForIngameAlert.Insert("personalradio", true);
		ItemsRequiredForIngameAlert.Insert("baseradio", true);
		ItemsRequiredForIngameAlert.Insert("itempda", true);
		ItemsRequiredForIngameAlert.Insert("zenwatchgps", true);

		RaidAlarmBuildTools = new array<string>;
		RaidAlarmBuildTools.Insert("hands");
		RaidAlarmBuildTools.Insert("screwdriver");
		
		RaidAlarmDismantleTools = new array<string>;
		RaidAlarmDismantleTools.Insert("hands");
		RaidAlarmDismantleTools.Insert("screwdriver");

		TriggerAdminRaidAlert = true;
		RaidAlarmRequiresTerritory = false;
		RaidStationInvincible = true;
		RaidRadarDishInvincible = true;
		RaidRadarDishCheckRoofTimerMinutes = -1; // -1 = off, 0 = only check on server startup, > 0 = minutes between checks
		RaidDetectionDistance = 150;
		RaidAlarmResetInMinutes = 10;
		RaidAlarmBatteryUsagePerSec = 1.0;
		RaidPlaySound = true;
		RaidDiscordAlertRequiresRadar = false;
		RaidDiscordAlertCooldownSecs = 1;
		RaidRemoveWebhookFailCount = 5;
		RaidAlarmMessageTitle = "#STR_ZenRaidAlarmTitle";
		RaidAlarmAttackMessage = "#STR_ZenRaidAlarmBaseUnderAttack";
		RaidAlarmWebhookUpdate = "#STR_ZenRaidAlarmConfigChanged";
		RaidAlarmConnectionOnline = "#STR_ZenRaidAlarmConnectionEstablished";
		RaidAlarmConnectionOffline = "#STR_ZenRaidAlarmConnectionLost";
		RaidAlarmRequiresTerritoryText = "#STR_ZenRaidAlarmTerritoryRequired";
		RaidAlarmTooClose = "#STR_ZenRaidAlarmTooClose";
		RaidAlarmConnectionObstruction = "#STR_ZenRaidAlarmDishObstructed";
		RaidAlarmBatteryWarning = "#STR_ZenRaidAlarmBatteryCharge";
		BaseDamageTriggersRaidAlert = true;
		BaseDismantleTriggersRaidAlert = true;
		ItemDamageTriggersRaidAlert = true;
		RequireAnItemForIngameAlert = false;
	}
}