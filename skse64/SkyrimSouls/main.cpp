#include "SKSE64_common/SKSE_version.h"
#include "SKSE64_common/BranchTrampoline.h"
#include "SKSE64_common/Relocation.h"
#include "SKSE64/PluginAPI.h"

#include <shlobj.h>
#include <memory>
#include <string>

#include "Hooks.h"
using namespace SkyrimSouls;

#define PLUGIN_VERSION	MAKE_EXE_VERSION_EX(1, 0, 0, 1)
#define PLUGIN_NAME		"SkyrimSouls"


IDebugLog						gLog;

#ifdef DEBUG
#pragma warning (push)
#pragma warning (disable : 4200)
struct TypeHierarchy
{
	struct Node
	{
		UInt32	    type;  //32bit RTTIType*
		UInt32		depth;
		UInt32		offset;
	};

	UInt32		memPtr;
	UInt32		unk04;
	UInt32		length;
	UInt32		nodes;     //32bit Node**
};


struct RTTIType
{
	void	 ** vtbl;
	UInt64	data;
	char	name[0];
};

struct RTTILocator
{
	UInt32		sig, offset, cdOffset;
	UInt32		typeDesc;
	UInt32		herarchyDesc;
	UInt32		thisLocator;
};
#pragma warning (pop)


const RelocAddr<uintptr_t>	type_info_vtbl = 0x188B5A0;

const RelocAddr<uintptr_t>  rdata_begin = 0x1522000;
const RelocAddr<uintptr_t>  rdata_end = 0x1DD5000;

const RelocAddr<uintptr_t>  data_begin = 0x1DD5000;
const RelocAddr<uintptr_t>  data_end = 0x34BD000;

const RelocAddr<uintptr_t>  text_begin = 0x1000;
const RelocAddr<uintptr_t>  text_end = 0x1522000;



void SearchVTable(uintptr_t typeinfo_addr)
{
	for (uintptr_t i = rdata_begin.GetUIntPtr(); i < rdata_end.GetUIntPtr(); i += sizeof(int))
	{
		static uintptr_t iOffset = (uintptr_t)GetModuleHandle(nullptr);
		if ((*reinterpret_cast<UInt32*>(i) + iOffset) == typeinfo_addr)
		{
			const RTTILocator * rtti = reinterpret_cast<RTTILocator*>(i - 0x0C);

			if (rtti->sig != 1 || rtti->cdOffset != 0) continue;

			uintptr_t location = reinterpret_cast<uintptr_t>(rtti);
			for (uintptr_t i = rdata_begin.GetUIntPtr(); i < rdata_end.GetUIntPtr(); i += sizeof(uintptr_t))
			{
				uintptr_t * p = reinterpret_cast<uintptr_t*>(i);
				if (*p == location)
				{
					uintptr_t * vtbl = reinterpret_cast<uintptr_t*>(p + 1);
					if (text_begin.GetUIntPtr() <= vtbl[0] && vtbl[0] < text_end.GetUIntPtr())
					{
						_MESSAGE("VTBL: %08X	OFFSET: %04X	CLASS: %s", (uintptr_t)vtbl - iOffset, rtti->offset, (char*)(typeinfo_addr + 0x10));
						TypeHierarchy* pHierarchy = reinterpret_cast<TypeHierarchy *>(rtti->herarchyDesc + iOffset);
						UInt32 * pInt = reinterpret_cast<UInt32*>(pHierarchy->nodes + iOffset);

						for (size_t i = 0; i < pHierarchy->length; ++i)
						{
							auto pNode = reinterpret_cast<TypeHierarchy::Node*>(pInt[i] + iOffset);
							RTTIType* pRTTI = reinterpret_cast<RTTIType*>(pNode->type + iOffset);
							_MESSAGE("		>> DEPTH: %d	OFFSET: %04X	CLASS: %s", pNode->depth, pNode->offset, pRTTI->name);
						}
					}
				}
			}
			_MESSAGE(" ");
		}
	}
};

#include "skse64/PapyrusVM.h"
#include "skse64/PapyrusNativeFunctions.h"
#include "HookUtil.h"
//VTBL: 018625A0	OFFSET: 0000	CLASS: .?AVVirtualMachine@Internal@BSScript@@
class VirtualMachineEx
{
public:
	using FnRegisterFunction = void(__thiscall VirtualMachineEx::*)(IFunction * fn);
	static FnRegisterFunction	fnRegisterFunction;

	void RegisterFunction_Hook(IFunction * fn)
	{
		uintptr_t callback = *(uintptr_t*)((uintptr_t)fn + 0x50);
		_MESSAGE("<%s> %s (%016I64X) callback=%08X", fn->GetClassName()->c_str(), fn->GetName()->c_str(), (uintptr_t)fn, callback - (uintptr_t)GetModuleHandle(NULL));
		_MESSAGE("");
		(this->*fnRegisterFunction)(fn);
	}

	static void InitHook()
	{
		//Dump all papyrus functions.
		fnRegisterFunction = HookUtil::SafeWrite64(RelocAddr<uintptr_t>(0x018625A0).GetUIntPtr() + 8 * 0x18, &RegisterFunction_Hook);//V1.10 .?AVVirtualMachine@Internal@BSScript@@
	}
};
VirtualMachineEx::FnRegisterFunction VirtualMachineEx::fnRegisterFunction = nullptr;



#include "skse64/GameEvents.h"
#include "Definition.h"
#include <vector>
namespace SkyrimSouls
{
	std::vector<std::string>  processedMenus;

	class MenuOpenCloseEventHandler : public BSTEventSink <MenuOpenCloseEvent>
	{
	public:
		virtual	EventResult ReceiveEvent(MenuOpenCloseEvent * evn, EventDispatcher<MenuOpenCloseEvent> * dispatcher) override
		{
			if (evn->opening && (*g_menuManager) != nullptr)
			{
				IMenu * pMenu = (*g_menuManager)->GetMenu(&evn->menuName);
				if (pMenu != nullptr && std::find(processedMenus.begin(), processedMenus.end(), std::string(evn->menuName.c_str())) == processedMenus.end())
				{
					processedMenus.push_back(evn->menuName.c_str());
					std::string flags;
					UInt32 mask = 0x80000000;
					while (mask != 0)
					{
						flags += (pMenu->flags & mask) ? " 1" : " 0";
						mask >>= 1;
					}
					_MESSAGE("%s		%s		depth: %04X		context: %d", evn->menuName.c_str(), flags.c_str(), pMenu->menuDepth, pMenu->context);
				}
			}
			return kEvent_Continue;
		}

		static void Register()
		{
			if ((*g_menuManager) != nullptr)
			{
				(*g_menuManager)->menuTable.Dump();
				static auto * pHandler = new MenuOpenCloseEventHandler();
				(*g_menuManager)->menuOpenCloseEventDispatcher.AddEventSink(pHandler);
			}
		}
	};
}

#endif



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

		if (skse->runtimeVersion != CURRENT_RELEASE_RUNTIME)
		{
			_FATALERROR("unsupported game version....");
			return false;
		}

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

#ifdef DEBUG
		//for (uintptr_t i = data_begin.GetUIntPtr(); i < data_end.GetUIntPtr(); i += sizeof(uintptr_t))
		//{
		//	uintptr_t * pointer = reinterpret_cast<uintptr_t*>(i);
		//	if (*pointer == type_info_vtbl.GetUIntPtr())
		//	{
		//		SearchVTable(i);
		//	}
		//}

		//VTBL: 018625A0	OFFSET: 0000	CLASS: .?AVVirtualMachine@Internal@BSScript@@
		//VirtualMachineEx::InitHook();
#endif
		return true;
	}
};
