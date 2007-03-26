import Purple

class BooPlugin(PurplePlugin):

	def handle(*args as (object)):
		b as Buddy
		b = args[0]
		Debug.debug(Debug.INFO, "booplugin",  "Boo Plugin knows that " + b.Alias + " is away\n")
		
	override def Load():
		Debug.debug(Debug.INFO, "booplugin", "loading...\n")
		BuddyList.OnBuddyAway.connect(self, handle)
		
	override def Unload():
		Debug.debug(Debug.INFO, "booplugin", "unloading...\n")
		
	override def Destroy():
		Debug.debug(Debug.INFO, "booplugin", "destroying...\n")
		
	override def Info():
		return PurplePluginInfo("mono-boo", "Boo Plugin", "0.1", "Test Boo Plugin", "Longer Description", "Eoin Coffey", "urled")
		
