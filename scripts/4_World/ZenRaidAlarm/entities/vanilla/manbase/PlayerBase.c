modded class PlayerBase
{
	override void SetActions(out TInputActionMap InputActionMap)
	{
		super.SetActions(InputActionMap);

		AddAction(ActionBuildZenRaidAlarmStation, InputActionMap);
		AddAction(ActionDismantleZenRaidAlarmRadar, InputActionMap);
		AddAction(ActionDismantleZenRaidAlarmStation, InputActionMap);
	}
}