using Gaim;

public class MPlugin : GaimPlugin
{
	public void HandleSig(object[] args)
	{
		Buddy buddy = (Buddy)args[0];
		
		Debug.debug(Debug.INFO, "mplug", "buddy " + buddy.Name + " went away\n");
	}
	
	public override void Load()
	{
		Debug.debug(Debug.INFO, "mplug", "loading...\n");
		
		/*Signal.connect(BuddyList.GetHandle(), this, "buddy-away", new Signal.Handler(HandleSig));*/
		BuddyList.OnBuddyAway.connect(this, new Signal.Handler(HandleSig));
	}
	
	public override void Unload()
	{
		Debug.debug(Debug.INFO, "mplug", "unloading...\n");
	}
	
	public override void Destroy()
	{
		Debug.debug(Debug.INFO, "mplug", "destroying...\n");
	}
	
	public override GaimPluginInfo Info()
	{
		return new GaimPluginInfo("C# Plugin", "0.1", "Test C# Plugin", "Longer Description", "Eoin Coffey", "urled");
	}
}
