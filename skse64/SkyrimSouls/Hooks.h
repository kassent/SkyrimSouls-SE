#pragma once

struct SKSETaskInterface;
struct SKSEMessagingInterface;
using PluginHandle = UInt32;

namespace SkyrimSouls
{
	extern PluginHandle				g_pluginHandle;
	extern SKSETaskInterface		* g_taskInterface;
	extern SKSEMessagingInterface	* g_messaging;

	void InstallHooks();
}