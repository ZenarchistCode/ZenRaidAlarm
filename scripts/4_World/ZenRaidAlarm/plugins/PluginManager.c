modded class PluginManager
{
	override void Init()
	{
		super.Init();

		RegisterPlugin("ZenRaidAlarmPlugin", false, true);
	}
}