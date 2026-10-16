class ZenRaidAlarmPlugin extends PluginBase
{
	protected ref array<Zen_RaidAlarmStation> m_RaidStations;
	protected ref map<string, int> m_RaidTriggers;
	protected ref map<string, ref map<string, int>> m_WebhookFailCountsByStationId;
	protected ref map<string, ref array<string>> m_WebhooksToRemoveByStationId;

	override void OnInit()
	{
		ZMPrint("[ZenRaidAlarmPlugin] :: OnInit");

		m_RaidStations = new array<Zen_RaidAlarmStation>;
		m_RaidTriggers = new map<string, int>;
		m_WebhookFailCountsByStationId = new map<string, ref map<string, int>>;
		m_WebhooksToRemoveByStationId = new map<string, ref array<string>>;

		GetZenDiscordAPI().GetOnWebhookError().Insert(OnDiscordWebhookError);
		GetZenDiscordAPI().GetOnWebhookSuccess().Insert(OnDiscordWebhookSuccess);
		GetZenDiscordAPI().GetOnWebhookTimeout().Insert(OnDiscordWebhookTimeout);
	}

	void ~ZenRaidAlarmPlugin()
	{
		if (GetZenDiscordAPI())
		{
			GetZenDiscordAPI().GetOnWebhookError().Remove(OnDiscordWebhookError);
			GetZenDiscordAPI().GetOnWebhookSuccess().Remove(OnDiscordWebhookSuccess);
			GetZenDiscordAPI().GetOnWebhookTimeout().Remove(OnDiscordWebhookTimeout);
		}
	}

    string GetRaidStationEntityId(Zen_RaidAlarmStation station)
    {
        if (!station)
            return "";

        int persistentIdA;
        int persistentIdB;
        int persistentIdC;
        int persistentIdD;

        station.GetPersistentID(persistentIdA, persistentIdB, persistentIdC, persistentIdD);

        return "RaidStation_" + persistentIdA + "_" + persistentIdB + "_" + persistentIdC + "_" + persistentIdD;
    }

	void OnDiscordWebhookError(ZenDiscordWebhookCB cb, int errorCode)
	{
		if (!cb)
			return;

		if (errorCode == ERestResultState.EREST_ERROR_APPERROR)
			return;

		if (cb.GetContextType() != "RaidAlarm")
			return;

		string stationId = cb.GetContextId();

		if (stationId == "")
			return;

		string webhook = cb.GetWebhook();

		if (webhook == "")
			return;

		RegisterWebhookFailure(stationId, webhook, errorCode);
	}

	void OnDiscordWebhookSuccess(ZenDiscordWebhookCB cb, string data, int dataSize)
	{
		if (!cb)
			return;

		if (cb.GetContextType() != "RaidAlarm")
			return;

		string stationId = cb.GetContextId();

		if (stationId == "")
			return;

		string webhook = cb.GetWebhook();

		if (webhook == "")
			return;

		ResetWebhookFailure(stationId, webhook);
	}

	void OnDiscordWebhookTimeout(ZenDiscordWebhookCB cb)
	{
		if (!cb)
			return;

		if (cb.GetContextType() != "RaidAlarm")
			return;

		string stationId = cb.GetContextId();

		if (stationId == "")
			return;

		string webhook = cb.GetWebhook();

		if (webhook == "")
			return;

		RegisterWebhookFailure(stationId, webhook, -1);
	}

	void RegisterWebhookFailure(string stationId, string webhook, int errorCode)
	{
		if (!m_WebhookFailCountsByStationId)
			return;

		if (stationId == "")
			return;

		if (webhook == "")
			return;

		if (!m_WebhookFailCountsByStationId.Contains(stationId))
			return;

		map<string, int> failCounts = m_WebhookFailCountsByStationId.Get(stationId);

		if (!failCounts)
			return;

		int failCount = 0;

		if (failCounts.Contains(webhook))
		{
			failCount = failCounts.Get(webhook);
		}

		failCount++;
		failCounts.Set(webhook, failCount);

		ZMPrint("[ZenRaidAlarmPlugin] Webhook failed stationId=" + stationId + " errorCode=" + errorCode + " failCount=" + failCount);

		if (GetZenRaidAlarmConfig().RaidRemoveWebhookFailCount >= 0 && failCount >= Math.Max(1, GetZenRaidAlarmConfig().RaidRemoveWebhookFailCount))
		{
			QueueWebhookRemoval(stationId, webhook);
			failCounts.Set(webhook, 0);
		}
	}

	void ResetWebhookFailure(string stationId, string webhook)
	{
		if (!m_WebhookFailCountsByStationId)
			return;

		if (stationId == "")
			return;

		if (webhook == "")
			return;

		if (!m_WebhookFailCountsByStationId.Contains(stationId))
			return;

		map<string, int> failCounts = m_WebhookFailCountsByStationId.Get(stationId);

		if (!failCounts)
			return;

		failCounts.Set(webhook, 0);
		if (m_WebhooksToRemoveByStationId && m_WebhooksToRemoveByStationId.Contains(stationId))
		{
			array<string> removals = m_WebhooksToRemoveByStationId.Get(stationId);
			if (removals)
				removals.RemoveItem(webhook);
		}

		ZMPrint("[ZenRaidAlarmPlugin] Webhook success stationId=" + stationId);
	}

	void QueueWebhookRemoval(string stationId, string webhook)
	{
		if (!m_WebhooksToRemoveByStationId)
			return;

		if (stationId == "")
			return;

		if (webhook == "")
			return;

		if (!m_WebhooksToRemoveByStationId.Contains(stationId))
		{
			m_WebhooksToRemoveByStationId.Set(stationId, new array<string>);
		}

		array<string> removals = m_WebhooksToRemoveByStationId.Get(stationId);

		if (!removals)
			return;

		if (removals.Find(webhook) == -1)
		{
			removals.Insert(webhook);
			ZMPrint("[ZenRaidAlarmPlugin] Queued webhook removal stationId=" + stationId);
		}
	}

	array<string> TakeQueuedWebhookRemovals(string stationId)
	{
		ref array<string> result = new array<string>;

		if (stationId == "")
			return result;

		if (!m_WebhooksToRemoveByStationId)
			return result;

		if (!m_WebhooksToRemoveByStationId.Contains(stationId))
			return result;

		array<string> removals = m_WebhooksToRemoveByStationId.Get(stationId);

		if (!removals)
			return result;

		foreach (string webhook : removals)
		{
			if (webhook != "")
			{
				result.Insert(webhook);
			}
		}

		removals.Clear();

		return result;
	}

	array<Zen_RaidAlarmStation> GetRaidStations()
	{
		return m_RaidStations;
	}

	void RegisterRaidStation(Zen_RaidAlarmStation station)
    {
        if (!station)
            return;

        if (!m_RaidStations)
            m_RaidStations = new array<Zen_RaidAlarmStation>;

        if (!m_WebhookFailCountsByStationId)
            m_WebhookFailCountsByStationId = new map<string, ref map<string, int>>;

        if (!m_WebhooksToRemoveByStationId)
            m_WebhooksToRemoveByStationId = new map<string, ref array<string>>;

        if (m_RaidStations.Find(station) != -1)
            return;

        string stationId = station.GetZenRuntimeStationId();

        if (stationId == "")
        {
            stationId = GetRaidStationEntityId(station);
            station.SetZenRuntimeStationId(stationId);
        }

        if (stationId == "")
            return;

        m_RaidStations.Insert(station);

        if (!m_WebhookFailCountsByStationId.Contains(stationId))
        {
            m_WebhookFailCountsByStationId.Set(stationId, new map<string, int>);
        }

        if (!m_WebhooksToRemoveByStationId.Contains(stationId))
        {
            m_WebhooksToRemoveByStationId.Set(stationId, new array<string>);
        }

        ZMPrint("[ZenRaidAlarmPlugin] Registered runtime ID " + stationId + " @ " + station.GetPosition());
    }

	void UnregisterRaidStation(Zen_RaidAlarmStation station, string whyMsg = "")
    {
        if (!station)
            return;

        string stationId = station.GetZenRuntimeStationId();
        vector stationPosition = station.GetPosition();

        if (m_RaidStations)
        {
            int idx = m_RaidStations.Find(station);

            if (idx != -1)
            {
                m_RaidStations.Remove(idx);
                ZMPrint("[ZenRaidAlarmPlugin] Unregistered raid station " + stationId + " @ " + stationPosition + " - reason: " + whyMsg);
            }
        }
    }

	Zen_RaidAlarmStation GetNearestRaidStation(vector pos)
	{
		if (!m_RaidStations)
			return null;

		Zen_RaidAlarmStation nearest;
		float nearestDistance = GetZenRaidAlarmConfig().RaidDetectionDistance;
		for (int i = 0; i < m_RaidStations.Count(); i++)
		{
			Zen_RaidAlarmStation station = m_RaidStations.Get(i);
			if (!station || station.IsSetForDeletion())
			{
				m_RaidStations.Remove(i);
				i--;
				continue;
			}
			float distance = vector.Distance(station.GetPosition(), pos);
			if (distance <= nearestDistance)
			{
				nearest = station;
				nearestDistance = distance;
			}
		}
		return nearest;
	}

	void AlertNearestRaidStation(vector objectPos)
	{
		Zen_RaidAlarmStation station = GetNearestRaidStation(objectPos);

		if (station)
		{
			station.TriggerRaidAlarm();
		}

		for (int i = 0; i < m_RaidTriggers.Count(); i++)
		{
			if (vector.Distance(m_RaidTriggers.GetKey(i).ToVector(), objectPos) <= GetZenRaidAlarmConfig().RaidDetectionDistance)
			{
				if (g_Game.GetTickTime() - m_RaidTriggers.GetElement(i) < 3600)
					return;
			}
		}

#ifdef GameLabs
		Zen_RaidAlarm_GameLabsDummy gameLabsDummy = Zen_RaidAlarm_GameLabsDummy.Cast(g_Game.CreateObject("Zen_RaidAlarm_GameLabsDummy", objectPos));
		ZenDiscordAPIFunctions.GameLabs_SendServerDeployed(objectPos, "RAID IN PROGRESS!");
#endif

		if (GetZenRaidAlarmConfig().TriggerAdminRaidAlert)
		{
			int playersInvolvedCount = 0;
			string playersInvolved = "";
			array<Man> onlinePlayers = new array<Man>;
			g_Game.GetPlayers(onlinePlayers);

			foreach (Man man : onlinePlayers)
			{
				PlayerBase pb = PlayerBase.Cast(man);

				if (pb != null && pb.GetIdentity() && vector.Distance(objectPos, pb.GetPosition()) <= GetZenRaidAlarmConfig().RaidDetectionDistance)
				{
					playersInvolved = playersInvolved + "[" + pb.GetCachedName() + "](http://steamcommunity.com/profiles/" + pb.GetIdentity().GetPlainId() + ")" + " (" + pb.GetIdentity().GetId() + ")\n";
					playersInvolvedCount++;
				}
			}

			string mapLink = "";

			if (GetZenCoreConfig().MapIzurviveURL == "")
			{
				mapLink = " @ " + objectPos[0] + " / " + objectPos[2];
			}
			else
			{
				mapLink = "@ [" + objectPos[0] + " / " + objectPos[2] + "](" + GetZenCoreConfig().MapIzurviveURL + "#location=" + objectPos[0] + ";" + objectPos[2] + ")";
			}

			ZenDiscordMessage msg = new ZenDiscordMessage(GetZenRaidAlarmConfig().RaidAlarmMessageTitle, ZEN_RAID_ICON);
			msg.SetTitle(GetZenRaidAlarmConfig().RaidAlarmMessageTitle);
			msg.SetMessage("A raid is occuring:\n\n" + mapLink + "\n\n" + playersInvolvedCount + " Players Involved : \n" + playersInvolved, false);
			msg.SetColor(255, 0, 0);
			msg.AddWebhooks(GetZenRaidAlarmConfig().AdminWebhooks);

			GetZenDiscordAPI().SendMessage(msg);

			m_RaidTriggers.Set(objectPos.ToString(false), g_Game.GetTickTime());
		}
	}
}