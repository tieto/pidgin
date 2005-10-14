using System;
using System.Runtime.CompilerServices;

namespace Gaim
{
	public class BuddyList
	{
		[MethodImplAttribute(MethodImplOptions.InternalCall)]
		extern private static IntPtr _get_handle();

		private static IntPtr handle = _get_handle();
		
		public static Event OnBuddyAway = new Event(handle, "buddy-away");
		public static Event OnBuddyBack = new Event(handle, "buddy-back");
		
		public static IntPtr GetHandle()
		{
			return _get_handle();
		}		
	}
}
