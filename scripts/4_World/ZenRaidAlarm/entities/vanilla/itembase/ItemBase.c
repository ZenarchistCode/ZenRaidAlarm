modded class ItemBase 
{
	//! RAID ALERT - Item alarm (eg. RA Basebuilding)
	override bool EEOnDamageCalculated(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
	{
		if (!super.EEOnDamageCalculated(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef))
			return false;

		CheckZenRaidTrigger();
		
		return true;
	}
	
	override void EEKilled(Object killer)
	{
		super.EEKilled(killer);
		
		CheckZenRaidTrigger();
	}
	
	void CheckZenRaidTrigger()
	{
		if (g_Game.IsClient())
			return;
		
		#ifdef ZenModPack
		if (!ZenModEnabled("ZenRaidAlarm"))
			return;
		#endif
		
		if (!GetZenRaidAlarmConfig().ItemDamageTriggersRaidAlert)
			return;
		
		string thisType = GetType();
		thisType.ToLower();
		
		foreach (string itemCfg : GetZenRaidAlarmConfig().ItemsDamagedDetect)
		{
			itemCfg.ToLower();
			if (thisType.Contains(itemCfg) || this.IsKindOf(itemCfg))
			{
				TriggerZenRaidAlarm();
				break;
			}
		}
	}
	
	void TriggerZenRaidAlarm()
	{
		#ifdef SERVER

		#ifdef ZenModPack
		if (!ZenModEnabled("ZenRaidAlarm"))
			return;
		#endif

		ZenRaidAlarmPlugin plugin = ZenRaidAlarmPlugin.Cast(GetPlugin(ZenRaidAlarmPlugin));

		if (plugin)
			plugin.AlertNearestRaidStation(GetPosition());
		#endif
	}
}