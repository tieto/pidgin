namespace Gaim
{
	public class GaimPluginInfo
	{
		private string name;
		private string version;
		private string summary;
		private string description;
		private string author;
		private string homepage;
		
		public GaimPluginInfo(string name, string version, string summary, string description, string author, string homepage)
		{
			this.name = name;
			this.version = version;
			this.summary = summary;
			this.description = description;
			this.author = author;
			this.homepage = homepage;	
		}
		
		public string Name { get { return name; } }
		public string Version { get { return version; } }
		public string Summary { get { return summary; } }
		public string Description { get { return description; } }
		public string Author { get { return author; } }
		public string Homepage { get { return homepage; } }
	}
	
	abstract public class GaimPlugin
	{	
		public abstract void Load();
		public abstract void Unload();
		public abstract void Destroy();
		
		public abstract GaimPluginInfo Info();
	}
}
