using Gaim;

public class GetBuddyBack : GaimPlugin
{
	public void HandleSig(object[] args)
	{
		Buddy buddy = (Buddy)args[0];
		
		Debug.debug(Debug.INFO, "buddyback", "buddy " + buddy.Name + " is back!\n");
	}
	
	public override void Load()
	{
		Debug.debug(Debug.INFO, "buddyback", "loading...\n");
		
		/*Signal.connect(BuddyList.GetHandle(), this, "buddy-back", new Signal.Handler(HandleSig));*/
		/*BuddyList.OnBuddyBack.connect(this, new Signal.Handler(HandleSig));*/
	}
	
	public override void Unload()
	{
	}
	
	public override void Destroy()
	{
	}
	
	public override GaimPluginInfo Info()
	{
		return new GaimPluginInfo("C# Get Buddy Back", "0.1", "Prints when a Buddy returns", "Longer Description", "Eoin Coffey", "urled");
	}
}
