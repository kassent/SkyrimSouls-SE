#include "SKSE64_common/SKSE_version.h"
#include "SKSE64_common/BranchTrampoline.h"
#include "SKSE64_common/Relocation.h"
#include "SKSE64/PluginAPI.h"

#include <shlobj.h>
#include <memory>
#include <string>

#include "Hooks.h"
using namespace SkyrimSouls;

#define PLUGIN_VERSION	MAKE_EXE_VERSION_EX(1, 0, 2, 0)
#define PLUGIN_NAME		"SkyrimSouls"


IDebugLog						gLog;


extern "C"
{
	bool SKSEPlugin_Query(const SKSEInterface * skse, PluginInfo * info)
	{
		std::unique_ptr<char[]> sPath(new char[MAX_PATH]);
		sprintf_s(sPath.get(), MAX_PATH, "%s%s.log", "\\My Games\\Skyrim Special Edition\\SKSE\\", PLUGIN_NAME);
		gLog.OpenRelative(CSIDL_MYDOCUMENTS, sPath.get());

		_MESSAGE("%s: %08X", PLUGIN_NAME, PLUGIN_VERSION);

		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = PLUGIN_NAME;
		info->version = PLUGIN_VERSION;

		g_pluginHandle = skse->GetPluginHandle();

		plugin_info.plugin_name = PLUGIN_NAME;
		plugin_info.runtime_version = skse->runtimeVersion;

		if (skse->isEditor)
		{
			_FATALERROR("loaded in editor, marking as incompatible");
			return false;
		}
		g_messaging = (SKSEMessagingInterface *)skse->QueryInterface(kInterface_Messaging);
		if (!g_messaging)
		{
			_FATALERROR("couldn't get messaging interface");
			return false;
		}
		g_taskInterface = (SKSETaskInterface *)skse->QueryInterface(kInterface_Task);
		if (!g_taskInterface)
		{
			_FATALERROR("couldn't get task interface");
			return false;
		}
		return true;
	}

	bool SKSEPlugin_Load(const SKSEInterface * f4se)
	{
		if (!g_branchTrampoline.Create(1024 * 64))
		{
			_ERROR("couldn't create branch trampoline. this is fatal. skipping remainder of init process.");
			return false;
		}

		if (!g_localTrampoline.Create(1024 * 64, nullptr))
		{
			_ERROR("couldn't create codegen buffer. this is fatal. skipping remainder of init process.");
			return false;
		}

		SkyrimSouls::InstallHooks();

		return true;
	}
};
