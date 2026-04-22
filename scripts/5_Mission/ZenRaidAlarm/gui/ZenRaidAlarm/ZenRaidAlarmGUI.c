class ZenRaidAlarmGUI extends UIScriptedMenu
{
	static const string GUIDE_URL		= "https://youtu.be/fgJRFBte8SQ";
	static const string	LAYOUT_FILE		= "ZenRaidAlarm/data/gui/layouts/RaidAlarm/raid_alarm.layout";

	protected ref EditBoxWidget			m_BaseName;
	protected ref EditBoxWidget			m_Webhook1;
	protected ref EditBoxWidget			m_Webhook2;
	protected ref EditBoxWidget			m_Webhook3;

	protected ref ButtonWidget			m_CancelBtn;
	protected ref ButtonWidget			m_OkBtn;
	protected ref ButtonWidget			m_ClearBtn1;
	protected ref ButtonWidget			m_ClearBtn2;
	protected ref ButtonWidget			m_ClearBtn3;
	protected ref ButtonWidget			m_PasteBtn1;
	protected ref ButtonWidget			m_PasteBtn2;
	protected ref ButtonWidget			m_PasteBtn3;
	protected ref ButtonWidget			m_GuideBtn;

	protected Zen_RaidAlarmStation		m_RaidStation;

    override Widget Init()
    {
		layoutRoot		= g_Game.GetWorkspace().CreateWidgets(LAYOUT_FILE);

		m_CancelBtn		= ButtonWidget.Cast(layoutRoot.FindAnyWidget("cancel_button"));
		m_OkBtn			= ButtonWidget.Cast(layoutRoot.FindAnyWidget("ok_button"));
		m_GuideBtn		= ButtonWidget.Cast(layoutRoot.FindAnyWidget("guide_button"));

		m_BaseName		= EditBoxWidget.Cast(layoutRoot.FindAnyWidget("BaseEditBoxWidget"));
		m_Webhook1		= EditBoxWidget.Cast(layoutRoot.FindAnyWidget("WebhookEditBoxWidget0"));
		m_Webhook2		= EditBoxWidget.Cast(layoutRoot.FindAnyWidget("WebhookEditBoxWidget1"));
		m_Webhook3		= EditBoxWidget.Cast(layoutRoot.FindAnyWidget("WebhookEditBoxWidget2"));

		m_ClearBtn1		= ButtonWidget.Cast(layoutRoot.FindAnyWidget("ClearButtonWidget0"));
		m_ClearBtn2		= ButtonWidget.Cast(layoutRoot.FindAnyWidget("ClearButtonWidget1"));
		m_ClearBtn3		= ButtonWidget.Cast(layoutRoot.FindAnyWidget("ClearButtonWidget2"));

		m_PasteBtn1		= ButtonWidget.Cast(layoutRoot.FindAnyWidget("PasteButtonWidget0"));
		m_PasteBtn2		= ButtonWidget.Cast(layoutRoot.FindAnyWidget("PasteButtonWidget1"));
		m_PasteBtn3		= ButtonWidget.Cast(layoutRoot.FindAnyWidget("PasteButtonWidget2"));

        return layoutRoot;
    }

	// Called when server has sync'd required scav data to client
	void OnReceivedServerData(string hook1, string hook2, string hook3, notnull Zen_RaidAlarmStation station, string baseName) 
	{
		m_Webhook1.SetText(hook1);
		m_Webhook2.SetText(hook2);
		m_Webhook3.SetText(hook3);
		m_BaseName.SetText(baseName);
		m_RaidStation = station;
	}

	bool ValidateWebhook(string text)
	{
		if (!text.Contains(ZenDiscordAPI.DISCORD_API))
			return false;

		return true;
	}

	override void Update(float timeslice)
	{
		super.Update(timeslice);

		if (KeyState(KeyCode.KC_ESCAPE) == 1)
			Close();
	}

	override void OnShow()
	{
		super.OnShow();

		g_Game.GetInput().ChangeGameFocus(1);
		SetFocus(layoutRoot);
		
		PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
		if (player) 
			player.GetInputController().SetDisabled(true);
		
		g_Game.GetUIManager().ShowCursor(true);
	}

	override void OnHide()
	{
		super.OnHide();

		g_Game.GetInput().ResetGameFocus();
		g_Game.GetUIManager().ShowCursor(false);

		PlayerBase player = PlayerBase.Cast(g_Game.GetPlayer());
		if (player) 
			player.GetInputController().SetDisabled(false);
	}

	override bool OnChange(Widget w, int x, int y, bool finished)
	{
		super.OnChange(w, x, y, finished);

		/*
		if (w == m_Webhook1)
		{
			string webhook1 = m_Webhook1.GetText();
			if (!ValidateWebhook(webhook1))
			{
				m_Webhook1.SetText("");
			}
		}

		if (w == m_Webhook2)
		{
			string webhook2 = m_Webhook2.GetText();	
			if (!ValidateWebhook(webhook2))
			{
				m_Webhook2.SetText("");
			}
		}

		if (w == m_Webhook3)
		{
			string webhook3 = m_Webhook3.GetText();
			if (!ValidateWebhook(webhook3))
			{
				m_Webhook3.SetText("");
			}
		}
		*/

		if (w == m_BaseName)
		{
			string baseName = m_BaseName.GetText();
			if (baseName.Length() >= 14)
				m_BaseName.SetText(baseName.Substring(0, baseName.Length() - 1));
		}

		return true;
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		string webhook;

		if (button == MouseState.LEFT)
		{
			if (w == m_CancelBtn)
			{
				Close();
				return true;
			}

			if (w == m_OkBtn)
			{
				webhook = m_Webhook1.GetText();
				if (!ValidateWebhook(webhook))
				{
					m_Webhook1.SetText("");
				}

				webhook = m_Webhook2.GetText();	
				if (!ValidateWebhook(webhook))
				{
					m_Webhook2.SetText("");
				}

				webhook = m_Webhook3.GetText();
				if (!ValidateWebhook(webhook))
				{
					m_Webhook3.SetText("");
				}

				Close();
				SendWebhooks();
				return true;
			}

			if (w == m_GuideBtn)
			{
				g_Game.OpenURL(GUIDE_URL);
				return true;
			}

			if (w == m_ClearBtn1)
			{
				m_Webhook1.SetText("");
				return true;
			}

			if (w == m_ClearBtn2)
			{
				m_Webhook2.SetText("");
				return true;
			}

			if (w == m_ClearBtn3)
			{
				m_Webhook3.SetText("");
				return true;
			}

			if (w == m_PasteBtn1)
			{
				g_Game.CopyFromClipboard(webhook);

				if (ValidateWebhook(webhook))
					m_Webhook1.SetText(webhook);

				return true;
			}

			if (w == m_PasteBtn2)
			{
				g_Game.CopyFromClipboard(webhook);

				if (ValidateWebhook(webhook))
					m_Webhook2.SetText(webhook);

				return true;
			}

			if (w == m_PasteBtn3)
			{
				g_Game.CopyFromClipboard(webhook);
				
				if (ValidateWebhook(webhook))
					m_Webhook3.SetText(webhook);

				return true;
			}
		}
		
		return super.OnClick(w, x, y, button);
	}

	// Remove any duplicates and send webhooks
	void SendWebhooks()
	{
		array<string> webhooks = new array<string>;
		webhooks.Insert(m_Webhook1.GetText());

		if (webhooks.Find(m_Webhook2.GetText()) != -1)
			webhooks.Insert("");
		else 
			webhooks.Insert(m_Webhook2.GetText());

		if (webhooks.Find(m_Webhook3.GetText()) != -1)
			webhooks.Insert("");
		else
			webhooks.Insert(m_Webhook3.GetText());

		GetRPCManager().SendRPC("ZenMod_RPC", "RPC_ReceiveZenRaidAlarmClientWebhooks", new Param3<array<string>, Zen_RaidAlarmStation, string>(webhooks, m_RaidStation, m_BaseName.GetText()), true, NULL);
	}
}
