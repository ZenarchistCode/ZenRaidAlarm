class Zen_RaidAlarmStationKit extends ZenKitBase
{
	override void SetActions()
	{
		super.SetActions();

		AddAction(ActionTogglePlaceObject);
		AddAction(ActionPlaceObject);
	}
}