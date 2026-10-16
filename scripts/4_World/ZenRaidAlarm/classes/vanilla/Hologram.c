modded class Hologram
{
	override EntityAI PlaceEntity(EntityAI entity_for_placing)
	{
		EntityAI superEntityAI = super.PlaceEntity(entity_for_placing);

		if (superEntityAI == NULL)
		{
			return NULL;
		}

		#ifdef SERVER
		//! RAID ALARM
		string itemType = entity_for_placing.GetType();
		itemType.ToLower();

		// Check if this item is on our raid config
		foreach (string s : GetZenRaidAlarmConfig().ItemsDeployedTriggerRaidAlert)
		{
			s.ToLower();

			if (itemType == s || entity_for_placing.IsKindOf(s))
			{
				ZenRaidAlarmPlugin plugin = ZenRaidAlarmPlugin.Cast(GetPlugin(ZenRaidAlarmPlugin));
				if (plugin)
				{
					plugin.AlertNearestRaidStation(entity_for_placing.GetPosition());
				}

				break;
			}
		}

		#endif
		return superEntityAI;
	}
}