#include "common/ICriticalSection.h"

#include "skse64_common/SafeWrite.h"
#include "skse64_common/BranchTrampoline.h"

#include "skse64/xbyak/xbyak.h"
#include "skse64/GameMenus.h"
#include "skse64/GameThreads.h"
#include "skse64/PapyrusVM.h"
#include "skse64/GameSettings.h"
#include "skse64/NiNodes.h"
#include "skse64/Hooks_UI.h"
#include "skse64/PluginAPI.h"
#include "skse64/ObScript.h"

#include <vector>
#include <queue>
#include <dbghelp.h>

#include "Definition.h"
#include "Hooks.h"
#include "HookUtil.h"

#pragma comment ( lib, "dbghelp.lib" )

namespace SkyrimSouls
{
	bool						s_skyrimSoulsMode = false;
	volatile SInt32				s_uiDelegateCount = 0;
	ICriticalSection			s_taskQueueLock;
	std::queue<TaskDelegate*>	s_tasks;

	PluginHandle				g_pluginHandle = kPluginHandle_Invalid;
	SKSETaskInterface			* g_taskInterface = nullptr;
	SKSEMessagingInterface		* g_messaging = nullptr;
}

namespace SkyrimSouls
{
	enum
	{
		kType_Console,
		kType_Tutorial,
		kType_MessageBox,
		kType_Tween,
		kType_Inventory,
		kType_Magic,
		kType_Container,
		kType_Favorites,
		kType_Barter,
		kType_Training,
		kType_Lockpicking,
		kType_Book,
		kType_Gift,
		kType_Journal,
		kType_SleepWait,
		kType_LevelUp,
		kType_Stats,
		kType_Map,
		kType_ModManager,
		kType_Custom,
		kType_NumMenus
	};

	struct MenuSetting
	{
		std::string		menuName;
		bool			isEnabled;
	};

	struct CoreController
	{
		static UInt32								globalControlCounter; // thread-safe...
		static std::vector<MenuSetting>				globalMenuSettings;
	};

	UInt32						CoreController::globalControlCounter = 0;
	std::vector<MenuSetting>	CoreController::globalMenuSettings =
	{
		{ "Console",					true },
		{ "Tutorial Menu",				true },
		{ "MessageBoxMenu",				true },
		{ "TweenMenu",					true },
		{ "InventoryMenu",				true },
		{ "MagicMenu",					true },
		{ "ContainerMenu",				true },
		{ "FavoritesMenu",				true },
		{ "BarterMenu",					true },
		{ "Training Menu",				true },
		{ "Lockpicking Menu",			true },
		{ "Book Menu",					true },
		{ "GiftMenu",					true },
		{ "Journal Menu",				true },
		{ "Sleep/Wait Menu",			true },
		{ "LevelUp Menu",				true },
		{ "StatsMenu",					true }, 
		{ "MapMenu",					true },
		{ "Mod Manager Menu",			true },
		{ "CustomMenu",					true }
	};

	void LoadSettings()
	{
		constexpr char * configFile = ".\\Data\\SKSE\\Plugins\\SkyrimSouls.ini";
		constexpr char * settingsSection = "Settings";
		for (auto & menuSetting : CoreController::globalMenuSettings)
		{
			std::string settingName = "b" + menuSetting.menuName;
			settingName.erase(std::remove(settingName.begin(), settingName.end(), ' '));///
			settingName.erase(std::remove(settingName.begin(), settingName.end(), '/'));
			UInt32 result = GetPrivateProfileIntA(settingsSection, settingName.c_str(), -1, configFile);
			if (result != -1)
				menuSetting.isEnabled = GetPrivateProfileIntA(settingsSection, settingName.c_str(), 1, configFile) != 0;
			else
				WritePrivateProfileStringA(settingsSection, settingName.c_str(), std::to_string(menuSetting.isEnabled).c_str(), configFile);
		}
	}
}


namespace SkyrimSouls
{
	class BGSSaveLoadManager
	{
	public:
		static bool IsLoadDisabled(MenuManager * pMenuMgr)
		{
			return (pMenuMgr->numPreventGameLoad || pMenuMgr->GetFlagCount(IMenu::kFlag_PreventGameLoadReplacer));
		}

		static void InitHooks()
		{
			uintptr_t location = RELOC_RUNTIME_ADDR("76 08 32 C0 48 83 C4 20 5B C3 E8 ? ? ? ?");
			SafeWrite8(location, 0xEB);
			g_branchTrampoline.Write5Call(location + 0xA, (uintptr_t)IsLoadDisabled);
		}
	};


	class MenuManagerEx : public MenuManager
	{
	public:
		static void ChangeMenuFlags_Hook(IMenu * pInstcance)
		{
			if (pInstcance && (pInstcance->flags & IMenu::kFlag_StopCrosshairUpdate)\
				&& !(pInstcance->flags & IMenu::kFlag_SkyrimSoulsMenu))
			{
				pInstcance->flags |= IMenu::kFlag_PreventGameLoadReplacer;
			}
		}

		static void InitHooks()
		{
			static uintptr_t kHook_ChangeMenuFlags_Jmp = RELOC_RUNTIME_ADDR("FF D0 48 8B D8 48 8B 4C 24 ?");
			struct ChangeMenuFlags_Code : Xbyak::CodeGenerator
			{
				ChangeMenuFlags_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
				{
					call(rax);
					mov(rbx, rax);
					mov(rcx, rax);
					mov(rax, (uintptr_t)ChangeMenuFlags_Hook);
					call(rax);
					jmp(ptr[rip]);
					dq(kHook_ChangeMenuFlags_Jmp + 0x5);
				}
			};
			void * codeBuf = g_localTrampoline.StartAlloc();
			ChangeMenuFlags_Code code(codeBuf);
			g_localTrampoline.EndAlloc(code.getCurr());
			g_branchTrampoline.Write5Branch(kHook_ChangeMenuFlags_Jmp, (uintptr_t)codeBuf);

			uintptr_t location = RELOC_RUNTIME_ADDR("E8 ? ? ? ? 84 C0 74 58 38 1D ? ? ? ?", 0, 1, 5); //stopDrawingWorld flag check. //sub_5B0F20
			UInt8 codes[] = { 0x48, 0x31, 0xC0, 0xC3, 0x90 };//xor rax, rax; retn;
			SafeWriteBuf(location, codes, sizeof(codes));
		}
	};



	class PlayerControlsEx : public PlayerControls
	{
	public:
		using FnReceiveEvent = EventResult (__thiscall PlayerControlsEx::*)(InputEvent **, InputEventDispatcher *);
		static FnReceiveEvent	ReceiveEvent_Original;
		static bool				s_disableUserEvent;
		static SimpleLock		s_controlThreadLock;
		//1687818
		EventResult ReceiveEvent_Hook(InputEvent ** evn, InputEventDispatcher * dispatcher)
		{
			InputEvent * fakeEvent = nullptr;
			InputEvent ** input = (s_disableUserEvent) ? &fakeEvent : evn;
			return (this->*ReceiveEvent_Original)(input, dispatcher);
		}

		static void DisableUserEvent(bool disabled)
		{
			SimpleLocker locker(&s_controlThreadLock);
			s_disableUserEvent = disabled;
		}

		static void InitHooks()
		{
			uintptr_t location = RELOC_RUNTIME_ADDR("F2 0F 11 49 ? 89 69 44", -0x5A, 3, 7);
			ReceiveEvent_Original = HookUtil::SafeWrite64(location + 1 * 8, &ReceiveEvent_Hook);
			//disable fader menu.
			location = RELOC_RUNTIME_ADDR("74 51 44 8B 89 ? ? ? ?");
			SafeWrite8(location, 0xEB);
		}
	};
	bool									PlayerControlsEx::s_disableUserEvent = false;
	SimpleLock								PlayerControlsEx::s_controlThreadLock;
	PlayerControlsEx::FnReceiveEvent		PlayerControlsEx::ReceiveEvent_Original = nullptr;


	class DialogueMenu :public IMenu
	{
	public:
		using FnProcessMessage = UInt32(__thiscall DialogueMenu::*)(UIMessage*);
		static FnProcessMessage ProcessMessage_Original;

		enum
		{
			kMessage_Visible = 0x10,
			kMessage_Invisible
		};

		UInt32 ProcessMessage_Hook(UIMessage * msg)
		{
			switch (msg->type)
			{
			case kMessage_Invisible:
			{
				this->view->SetVisible(false);
				return 0;
			}
			case kMessage_Visible:
			{
				this->view->SetVisible(true);
				return 0;
			}
			default:
				break;
			}
			UInt32 result = (this->*ProcessMessage_Original)(msg);
			return result;
		}

		static void InitHooks()
		{
			//menu depth
			uintptr_t location = RELOC_RUNTIME_ADDR("C6 46 18 03 C6 05 ? ? ? ? ?");
			SafeWrite8(location + 0x3, 0x0);

			location = RELOC_RUNTIME_ADDR("48 8D 05 ? ? ? ? 48 89 06 48 8D 05 ? ? ? ? 48 89 46 30 48 8D 5E 38", 0, 3, 7);
			ProcessMessage_Original = HookUtil::SafeWrite64(location + 4 * 0x8, &ProcessMessage_Hook);
		}
	};
	DialogueMenu::FnProcessMessage	DialogueMenu::ProcessMessage_Original = nullptr;


	class StatsMenu : public IMenu,
		public MenuEventHandler
	{
	public:
		using FnProcessMessage = UInt32(__thiscall StatsMenu::*)(UIMessage*);
		//ctor 8CC400
		using FnCanProcess = bool(__thiscall StatsMenu::*)(InputEvent *);
		static FnCanProcess		CanProcess_Original;
		static FnProcessMessage ProcessMessage_Original;


		static void InitHooks()
		{
			uintptr_t location = RELOC_RUNTIME_ADDR("0F 85 ? ? ? ? 48 8D 43 ? 48 8B 4C 24 ?");
			SafeWrite32(location + 2, 0);
		}
	};
	StatsMenu::FnCanProcess				StatsMenu::CanProcess_Original = nullptr;
	StatsMenu::FnProcessMessage			StatsMenu::ProcessMessage_Original = nullptr;


	class MapMenu : public IMenu,
					public BSTEventSink<MenuOpenCloseEvent>
	{
	public:

		static void InitHooks()
		{
			uintptr_t location = RELOC_RUNTIME_ADDR("0F 85 ? ? ? ? 48 8B 05 ? ? ? ? 4C 8B 80 ? ? ? ?");
			UInt8 codes[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
			SafeWriteBuf(location, codes, sizeof(codes));
		}
	};


	class PlayerCameraEx : public PlayerCamera
	{
	public:
		using FnUpdateCameraPosition = bool(*)(PlayerCameraEx *, NiPoint3 &);
		static FnUpdateCameraPosition	UpdateCameraPosition_Original;

		static bool UpdateCameraPosition_Hook(PlayerCameraEx * camera, NiPoint3 & pos)
		{
			if ((*g_menuManager)->IsMenuOpen((*g_uiStringHolder)->mapMenu) && (*g_menuManager)->IsSkyrimSoulsMenu((*g_uiStringHolder)->mapMenu))
			{
				SimpleLocker locker(&camera->cameraLock);
				if (!camera->isProcessed)
				{
					bool result = false;
					TESObjectREFR * pRef = nullptr;
					LookupREFRByHandle(&camera->refHandle, &pRef);
					if (pRef && pRef->Is(kFormType_Character) && !camera->IsFreeCamera())
					{
						auto * pNiNode = pRef->GetNiRootNode(camera->IsFirstPerson());
						if (pNiNode != nullptr)
						{
							pos.x = camera->cameraPos.x = pNiNode->m_worldTransform.pos.x;
							pos.y = camera->cameraPos.y = pNiNode->m_worldTransform.pos.y;
							pos.z = camera->cameraPos.z = pNiNode->m_worldTransform.pos.z;
							result = camera->isProcessed = true;
						}
					}
					if (pRef != nullptr)
						pRef->handleRefObject.DecRefHandle();
					return result ? true : UpdateCameraPosition_Original(camera, pos);
				}
				else
				{
					pos.x = camera->cameraPos.x;
					pos.y = camera->cameraPos.y;
					pos.y = camera->cameraPos.y;
					return true;
				}
			}
			else
			{
				return UpdateCameraPosition_Original(camera, pos);
			}
		}

		static void InitHooks()
		{
			static uintptr_t kHook_UpdateCameraPosition_Jmp = RELOC_RUNTIME_ADDR("E8 ? ? ? ? 84 C0 74 20 F3 0F 10 05 ? ? ? ? 48 8B C3", 0, 1, 5);
			struct UpdateCameraPosition_Code : Xbyak::CodeGenerator {
				UpdateCameraPosition_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
				{
					push(rsi);
					push(rdi);
					push(r12);
					jmp(ptr[rip]);
					dq(kHook_UpdateCameraPosition_Jmp + 5);
				}
			};
			void * codeBuf = g_localTrampoline.StartAlloc();
			UpdateCameraPosition_Code code(codeBuf);
			g_localTrampoline.EndAlloc(code.getCurr());
			UpdateCameraPosition_Original = (FnUpdateCameraPosition)codeBuf;
			g_branchTrampoline.Write5Branch(kHook_UpdateCameraPosition_Jmp, (uintptr_t)UpdateCameraPosition_Hook);
		}
	};
	PlayerCameraEx::FnUpdateCameraPosition	PlayerCameraEx::UpdateCameraPosition_Original = nullptr;


	class JournalMenu : public IMenu
	{
	public:

		static void InitHooks()
		{
			uintptr_t location = RELOC_RUNTIME_ADDR("77 04 32 C0 EB 02 B0 01 48 89 5D 97");
			SafeWrite16(location, 0x9090);
		}
	};


	class FavoritesMenu : public IMenu,
		public MenuEventHandler
	{
	public:
		using FnCanProcess = bool(__thiscall FavoritesMenu::*)(InputEvent *);
		static FnCanProcess CanProcess_Original;

		static void InitHooks()
		{
			uintptr_t location = RELOC_RUNTIME_ADDR("75 1D 48 8B 03 48 8B CB FF 50 10");
			SafeWrite16(location, 0x9090);
		}
	};
	FavoritesMenu::FnCanProcess		FavoritesMenu::CanProcess_Original = nullptr;


	class FavoritesHandler : public MenuEventHandler
	{
	public:
		using FnCanProcess = bool(__thiscall FavoritesHandler::*)(InputEvent *);
		static FnCanProcess CanProcess_Original;

		bool CanProcess_Hook(InputEvent * evn)
		{
			return (s_skyrimSoulsMode) ? false : (this->*CanProcess_Original)(evn);
		}

		static void InitHooks()
		{
			uintptr_t location = RELOC_RUNTIME_ADDR("48 8D 0D ? ? ? ? 48 89 08 EB 03 48 8B C7 48 89 46 70", 0, 3, 7);
			CanProcess_Original = HookUtil::SafeWrite64(location + 1 * 0x8, &CanProcess_Hook);
		}
	};
	FavoritesHandler::FnCanProcess FavoritesHandler::CanProcess_Original = nullptr;


	class ContainerMenu : public IMenu
	{
	public:
		enum
		{
			kLootMode_Steal = 1,
			kLootMode_Pickpocket = 2,
			kLootMode_Take
		};

		using FnDrawNextFrame = void(ContainerMenu::*)(float, UInt32);
		static FnDrawNextFrame	DrawNextFrame_Original;

		static UInt32			* s_lootMode;
		static UInt32			* s_containerOwnerHandle;

		void DrawNextFrame_Hook(float unk0, UInt32 unk1)
		{
			if ((flags & IMenu::kFlag_SkyrimSoulsMenu) && s_lootMode && s_containerOwnerHandle && (*s_lootMode) == kLootMode_Pickpocket)
			{
				TESObjectREFR * pRef = nullptr;
				LookupREFRByHandle(s_containerOwnerHandle, &pRef);
				if (pRef != nullptr && pRef->Is(kFormType_Character))
				{
					auto * actor = static_cast<Actor*>(pRef);
					if (!actor->IsDead(true) && !actor->IsDisabled() && !actor->IsDeleted())
					{
						UInt32 iPickpocketDistance = (*g_gameSettingCollection)->Get("iActivatePickLength")->data.u32 + 50;
						float fCurrentDistance = (*g_thePlayer)->GetDistance(pRef, true, false);
						if (fCurrentDistance > iPickpocketDistance)
						{
							(*g_uiMessageManager)->SendUIMessage((*g_uiStringHolder)->containerMenu, kMessage_Close, nullptr);
						}
					}
				}
				if (pRef != nullptr)
				{
					pRef->handleRefObject.DecRefHandle();
				}
			}
			return (this->*DrawNextFrame_Original)(unk0, unk1);
		}

		static void InitHooks()
		{
			uintptr_t location = RELOC_RUNTIME_ADDR("76 4D 83 EF 01 74 48 44 8B CF 45 33 C0 49 8B D4");
			SafeWrite8(location, 0xEB);

			location = RELOC_RUNTIME_ADDR("33 ED 48 89 6C 24 ? 48 8D 54 24 ? 48 8D 0D ? ? ? ?", -0x24, 3, 7);
			DrawNextFrame_Original = HookUtil::SafeWrite64(location + 0x8 * 5, &DrawNextFrame_Hook);

			s_lootMode = reinterpret_cast<UInt32*>(RELOC_RUNTIME_ADDR("83 3D ? ? ? ? ? 0F 85 ? ? ? ? 4C 89 7D 77", 0, 2, 7));
			s_containerOwnerHandle = reinterpret_cast<UInt32*>(RELOC_RUNTIME_ADDR("48 8D 0D ? ? ? ? E8 ? ? ? ? 90 49 8B 0E E8 ? ? ? ?", 0, 3, 7));
		}
	};
	ContainerMenu::FnDrawNextFrame		ContainerMenu::DrawNextFrame_Original = nullptr;
	UInt32 *							ContainerMenu::s_lootMode = nullptr;
	UInt32 *							ContainerMenu::s_containerOwnerHandle = nullptr;


	class SleepWaitMenu : public IMenu
	{
	public:
		class FxDelegateArgs
		{
		public:
			GFxValue			responseID; // 00
			FxDelegateHandler	* menu;		// 18
			GFxMovieView		* movie;	// 20
			GFxValue			* args;		// 28
			UInt32				numArgs;	// 28
		};

		using FnProcessMessage = UInt32(__thiscall SleepWaitMenu::*)(UIMessage *);
		using FnCreateUIMessageData = IUIMessageData*(*)(BSFixedString&);
		using FnUICallback = void(*)(FxDelegateArgs *);


		static FnProcessMessage				ProcessMessage_Original;
		static FnUICallback					RequestSleepWait_Original;
		static FnCreateUIMessageData		CreateUIMessageData;

		enum
		{
			kMessage_ConfirmWait = 0x10,
			kMessage_RequestWait
		};

		class ConfirmWaitingDelegate : public TaskDelegate
		{
		public:
			ConfirmWaitingDelegate(UInt32 time) : m_time(time){};

			virtual void Dispose() override
			{
				delete this;
			}

			virtual void Run() override
			{
				RefHandleUIData	* pRefHandleData = (RefHandleUIData*)CreateUIMessageData((*g_uiStringHolder)->refHandleUIData);
				if (pRefHandleData)
				{
					pRefHandleData->refHandle = static_cast<UInt32>(m_time);
				}
				(*g_uiMessageManager)->SendUIMessage((*g_uiStringHolder)->sleepWaitMenu, kMessage_ConfirmWait, pRefHandleData);
			}

			static void Register(UInt32 time)
			{
				s_taskQueueLock.Enter();
				auto * pTask = new ConfirmWaitingDelegate(time);
				s_tasks.push(pTask);
				s_taskQueueLock.Leave();
			}
		private:
			UInt32					m_time;
		};


		UInt32	ProcessMessage_Hook(UIMessage * msg)
		{
			switch (msg->type)
			{
			case kMessage_RequestWait:
			{
				if (msg->extraData && !(this->flags & kFlag_IncMenuModeCounter))
				{
					auto * pRefHandleData = static_cast<RefHandleUIData*>(msg->extraData);
					(*g_menuManager)->numPauseGame += 1;
					this->flags |= kFlag_IncMenuModeCounter;
					ConfirmWaitingDelegate::Register(pRefHandleData->refHandle);
				}
				return 0;
			}
			case kMessage_ConfirmWait:
			{
				if (msg->extraData != nullptr)
				{
					auto * pRefHandleData = static_cast<RefHandleUIData*>(msg->extraData);
					FxDelegateArgs args;
					args.responseID.SetNumber(0);

					args.menu = this;
					args.movie = this->view;

					GFxValue param;
					param.SetNumber(pRefHandleData->refHandle);
					args.args = &param;
					args.numArgs = 1;

					RequestSleepWait_Original(&args);
				}
				return 0;
			}
			}
			UInt32 result = (this->*ProcessMessage_Original)(msg);
			if (msg->type == kMessage_Close && result != 1 && (this->flags & kFlag_IncMenuModeCounter))
			{
				(*g_menuManager)->numPauseGame -= 1;
				this->flags &= ~kFlag_IncMenuModeCounter;
			}
			return result;
		}


		static void RequestSleepWait_Hook(FxDelegateArgs * pargs)
		{
			SleepWaitMenu * pThisMenu = static_cast<SleepWaitMenu *>(pargs->menu);
			if (!(pThisMenu->flags & kFlag_PauseGame))
			{
				UInt32 time = static_cast<UInt32>(pargs->args->GetNumber());
				RefHandleUIData	* pRefHandleData = (RefHandleUIData*)CreateUIMessageData((*g_uiStringHolder)->refHandleUIData);
				if (pRefHandleData != nullptr)
				{
					pRefHandleData->refHandle = time;
					(*g_uiMessageManager)->SendUIMessage((*g_uiStringHolder)->sleepWaitMenu, kMessage_RequestWait, pRefHandleData);
				}
			}
			else
			{
				RequestSleepWait_Original(pargs);
			}
		}

		static void InitHooks()
		{
			uintptr_t location = RELOC_RUNTIME_ADDR("48 8D 05 ? ? ? ? 48 89 03 33 C0 89 43 30 48 89 43 38 89 43 40", 0, 3, 7);
			ProcessMessage_Original = HookUtil::SafeWrite64(location + 8 * 0x4, &ProcessMessage_Hook);

			static uintptr_t kHook_RegisterCallback_Jmp = reinterpret_cast<uintptr_t*>(location)[1] + 0x2C;
			struct RegisterCallback_Code : Xbyak::CodeGenerator
			{
				RegisterCallback_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
				{
					mov(r8, (uintptr_t)RequestSleepWait_Hook);
					jmp(ptr[rip]);
					dq(kHook_RegisterCallback_Jmp + 0x7);
				}
			};
			void * codeBuf = g_localTrampoline.StartAlloc();
			RegisterCallback_Code code(codeBuf);
			g_localTrampoline.EndAlloc(code.getCurr());
			g_branchTrampoline.Write5Branch(kHook_RegisterCallback_Jmp, (uintptr_t)codeBuf);
			SafeWrite16(kHook_RegisterCallback_Jmp + 0x5, 0x9090);

			RequestSleepWait_Original = reinterpret_cast<FnUICallback>(RELOC_RUNTIME_ADDR("48 89 5C 24 ? 57 48 83 EC 30 48 8B 59 18 48 8B F9 48 8B 0D ? ? ? ?"));
			CreateUIMessageData = reinterpret_cast<FnCreateUIMessageData>(RELOC_RUNTIME_ADDR("E8 ? ? ? ? 48 89 58 20 C7 40 ? ? ? ? ?", 0, 1, 5));
		}
	};
	SleepWaitMenu::FnProcessMessage					SleepWaitMenu::ProcessMessage_Original = nullptr;
	SleepWaitMenu::FnUICallback						SleepWaitMenu::RequestSleepWait_Original = nullptr;
	SleepWaitMenu::FnCreateUIMessageData			SleepWaitMenu::CreateUIMessageData = nullptr;


	class BookMenu : public IMenu
	{
		//ctor 0859540
		//vtable 16C5390
		//UInt64 animationGraphManagerHolder[(0x48 - 0x30) >> 3];
		//BSTEventSink<BSAnimationGraphEvent> eventsink;
	public:
		static void InitHooks()
		{
			uintptr_t location = RELOC_RUNTIME_ADDR("75 08 48 8B CB E8 ? ? ? ? 48 39 73 70 0F 84 ? ? ? ? 48 85 FF");
			SafeWrite16(location, 0x9090);
		}
	};


	class TweenMenu : public IMenu
	{
	public:
			//016D6918       .?AVTweenMenu@@
		static void InitHooks()
		{
			//no camera change when enter tween mode.
			uintptr_t location = RELOC_RUNTIME_ADDR("E8 ? ? ? ? 48 8D 0D ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? 48 8B 0D ? ? ? ?");
			UInt8 codes[] = { 0x90, 0x90, 0x90, 0x90, 0x90 };
			//SafeWriteBuf(RelocAddr<uintptr_t>(0x08D08D5), codes, sizeof(codes));
			SafeWriteBuf(location, codes, sizeof(codes));
			location = RELOC_RUNTIME_ADDR("E8 ? ? ? ? 90 48 8D 05 ? ? ? ? 48 89 44 24 ? 33 C0 48 89 46 48", 0x18);
			SafeWrite32(location + 0x3, 0xC401);
		}
	};


	class JobListManager
	{
	public:
		using FnSetEventEx		= void(*)(UInt32);
		using FnJob				= void(*)();
		using FnShadowCulling	= void(*)(UInt32, UInt32);

		struct JobNode
		{
			FnJob			job;
			const char		* name;
		};
		STATIC_ASSERT(sizeof(JobNode) == 0x10);


		class ServingThread
		{
		public:
			virtual		~ServingThread();
			virtual		UInt32	Run();
			CRITICAL_SECTION cs;
			HANDLE			createdThreadHandle;		// 30
			HANDLE			currentThreadHandle;		// 38
			UInt32			unk40;						// 40
			UInt32			currentThreadID;			// 44
			bool			isThreadCreated;			// 48
			UInt64			unk50;
			HANDLE			eventHandle1;				// 58
			HANDLE			eventHandle2;				// 60 manual reset.
			UInt32			unk68;
			UInt32			threadState;				// 6C
			bool			unk70[2];					// 70
			bool			isRunning;					// 72
		};

		struct Task
		{
			tArray<JobNode>		jobs;	// 00
		};

		static FnJob				UpdateUI_Original;
		static FnShadowCulling		WaitForShadowCulling_Original;
		static FnSetEventEx			SetEventEx;
		static bool					* isGameLoading;


		static void UpdateUI_Hook()// non-main thread.
		{
			if (!(*isGameLoading))
			{
				if (s_skyrimSoulsMode) {
					_InterlockedIncrement(&s_uiDelegateCount);
				}
				else {
					UpdateUI_Original();
				}
			}
			SetEventEx(2);
		}

		static void InitHooks()
		{
			auto ReadOffsetData = [](uintptr_t location, SInt32 relOffset, UInt32 len)->uintptr_t
			{
				SInt32 rel32 = 0;
				sig_scan_util::read_memory(location + relOffset, &rel32, sizeof(SInt32));
				return location + len + rel32;
			};

			UpdateUI_Original = HookUtil::SafeWrite64(RELOC_RUNTIME_ADDR("75 1C 48 8D 0D ? ? ? ? 48 89 4D 48", 0x6F, 3, 7) + 0x1F0, UpdateUI_Hook);

			uintptr_t location = reinterpret_cast<uintptr_t>(UpdateUI_Original);
			SetEventEx = reinterpret_cast<FnSetEventEx>(ReadOffsetData(location + 0x3B, 1, 5));
			isGameLoading = reinterpret_cast<bool *>(ReadOffsetData(location + 4, 2, 7));

			SafeWrite16(location + 0xB, 0x9090);
			UInt8 codes[] = { 0xC3, 0x90, 0x90, 0x90, 0x90 };
			SafeWriteBuf(location + 0x3B, codes, sizeof(codes));
		}
	};
	STATIC_ASSERT(sizeof(JobListManager::ServingThread) == 0x78);


	JobListManager::FnJob			JobListManager::UpdateUI_Original = nullptr;
	JobListManager::FnShadowCulling	JobListManager::WaitForShadowCulling_Original = nullptr;
	JobListManager::FnSetEventEx	JobListManager::SetEventEx = nullptr;
	bool *							JobListManager::isGameLoading = nullptr;


	class VirtualMachineEx : public SkyrimVM
	{
		//016E6D50	OFFSET: 0000	CLASS: .?AVSkyrimVM@@
		//UInt32				worldRunningTimer;					// 690
		//UInt32				gameRunningTimer;					// 694
	public:
		static bool *		menuMode1;
		static bool *		menuMode2;

		static bool IsInMenuMode_Hook()
		{
			return (*menuMode1) || (*menuMode2) || s_skyrimSoulsMode;
		}

		static void InitHooks()
		{
			static uintptr_t kHook_UpdateVirtualMachineTimer_Jmp = RELOC_RUNTIME_ADDR("01 BE ? ? ? ? 48 8B 0D ? ? ? ?");//0921CC5
			struct UpdateVirtualMachineTimer_Code : Xbyak::CodeGenerator
			{
				UpdateVirtualMachineTimer_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
				{
					Xbyak::Label skipUpdate;
					mov(rax, (uintptr_t)&s_skyrimSoulsMode);
					mov(al, ptr[rax]);
					cmp(al, 0);
					jnz(skipUpdate);
					add(ptr[rsi + 0x690], edi);
					L(skipUpdate);
					jmp(ptr[rip]);
					dq(kHook_UpdateVirtualMachineTimer_Jmp + 0x6);
				}
			};
			void * codeBuf = g_localTrampoline.StartAlloc();
			UpdateVirtualMachineTimer_Code code(codeBuf);
			g_localTrampoline.EndAlloc(code.getCurr());
			g_branchTrampoline.Write6Branch(kHook_UpdateVirtualMachineTimer_Jmp, (uintptr_t)codeBuf);

			auto ReadOffsetData = [](uintptr_t location, SInt32 relOffset, UInt32 len)->uintptr_t
			{
				SInt32 rel32 = 0;
				sig_scan_util::read_memory(location + relOffset, &rel32, sizeof(SInt32));
				return location + len + rel32;
			};
			uintptr_t location = RELOC_RUNTIME_ADDR("80 3D ? ? ? ? ? 75 0C 80 3D ? ? ? ? ?");
			menuMode1 = reinterpret_cast<bool*>(ReadOffsetData(location, 2, 7));
			menuMode2 = reinterpret_cast<bool*>(ReadOffsetData(location + 0x9, 2, 7));

			g_branchTrampoline.Write5Branch(location, (uintptr_t)IsInMenuMode_Hook);
			SafeWrite16(location + 0x5, 0x9090);
		}
	};
	bool *				VirtualMachineEx::menuMode1 = nullptr;
	bool *				VirtualMachineEx::menuMode2 = nullptr;


	class MainEx : public Main
	{
	public:
		using FnIsGameRunning = bool(*)(UInt32);

		using FnReleaseSemaphore = void(*)(BSTaskPool *);

		using FnProcessTlsIndex = void(*)(bool);

		static FnIsGameRunning		IsGameRunning_Original;

		static FnReleaseSemaphore	ReleaseSemaphore_Original;

		static FnProcessTlsIndex	ProcessTlsIndex_Original;

		static void ProcessTlsIndex_Hook(bool arg)
		{
			ProcessTlsIndex_Original(arg);
			s_skyrimSoulsMode = CoreController::globalControlCounter != 0;
		}

		static bool IsGameRunning_Hook(UInt32 state)
		{
			if (_InterlockedExchange(&s_uiDelegateCount, 0))
			{
				JobListManager::UpdateUI_Original();
			}
			return IsGameRunning_Original(state);
		}

		static void ReleaseSemaphore_Hook(BSTaskPool * pTaskPool)
		{
			s_taskQueueLock.Enter();
			while (!s_tasks.empty())
			{
				TaskDelegate * cmd = s_tasks.front();
				s_tasks.pop();
				cmd->Run();
				cmd->Dispose();
			}
			s_taskQueueLock.Leave();
			ReleaseSemaphore_Original(pTaskPool);
		}

		static void InitHooks()
		{
			 uintptr_t location = RELOC_RUNTIME_ADDR("E8 ? ? ? ? 84 C0 75 0C 48 8B 0D ? ? ? ? E8 ? ? ? ? 48 8B CB");
			 auto ReadOffsetData = [](uintptr_t location, SInt32 relOffset, UInt32 len)->uintptr_t
			 {
				 SInt32 rel32 = 0;
				 sig_scan_util::read_memory(location + relOffset, &rel32, sizeof(SInt32));
				 return location + len + rel32;
			 };
			 IsGameRunning_Original = reinterpret_cast<FnIsGameRunning>(ReadOffsetData(location, 1, 5));
			 g_branchTrampoline.Write5Call(location, (uintptr_t)IsGameRunning_Hook);

			 location = RELOC_RUNTIME_ADDR("E8 ? ? ? ? 90 48 8D 4C 24 ? E8 ? ? ? ? 48 8B 15 ? ? ? ?");
			 ReleaseSemaphore_Original = reinterpret_cast<FnReleaseSemaphore>(ReadOffsetData(location, 1, 5));

			 g_branchTrampoline.Write5Call(location, (uintptr_t)ReleaseSemaphore_Hook);

			 location = RELOC_RUNTIME_ADDR("E8 ? ? ? ? 48 8B 0D ? ? ? ? 83 B9 ? ? ? ? ? 0F 97 C0");
			 ProcessTlsIndex_Original = reinterpret_cast<FnProcessTlsIndex>(ReadOffsetData(location, 1, 5));
			 g_branchTrampoline.Write5Call(location, (uintptr_t)ProcessTlsIndex_Hook);
#ifdef TODO
			 //location = RELOC_RUNTIME_ADDR("75 0C 40 0F B6 D7");//map menu related... TODO test.
			 //SafeWrite16(location, 0x9090);
#endif // TODO

		}
	};
	MainEx::FnIsGameRunning				MainEx::IsGameRunning_Original = nullptr;
	MainEx::FnReleaseSemaphore			MainEx::ReleaseSemaphore_Original = nullptr;
	MainEx::FnProcessTlsIndex			MainEx::ProcessTlsIndex_Original = nullptr;


	class StartRemapModeEx : public GFxFunctionHandler
	{
		class RemapHandler : public BSTEventSink<InputEvent>,
							 public UIDelegate_v1
		{
		public:
			RemapHandler() : keyCode(-1) {}

			virtual ~RemapHandler() {}

			virtual EventResult ReceiveEvent(InputEvent ** evns, InputEventDispatcher * dispatcher) override
			{
				auto * inputDeviceMgr = reinterpret_cast<InputDeviceManager*>(dispatcher);
				ButtonEvent * e = (ButtonEvent*)*evns;
				if (!e || e->eventType != InputEvent::kEventType_Button)
					return kEvent_Continue;

				UInt32 deviceType = e->deviceType;

				if ((inputDeviceMgr->IsGamepadEnabled() ^ (deviceType == kDeviceType_Gamepad)) || e->flags == 0 || e->timer != 0.0)
					return kEvent_Continue;

				UInt32 keyMask = e->keyMask;

				if (deviceType == kDeviceType_Mouse)
					keyCode = InputMap::kMacro_MouseButtonOffset + keyMask;
				else if (deviceType == kDeviceType_Gamepad)
					keyCode = InputMap::GamepadMaskToKeycode(keyMask);
				else
					keyCode = keyMask;

				if (keyCode >= InputMap::kMaxMacros)
					keyCode = -1;

				inputDeviceMgr->RemoveEventSink(this);
				if (g_taskInterface)
				{
					g_taskInterface->AddUITask(this);
				}
				return kEvent_Continue;
			}

			virtual void	Run(void) override
			{
				GFxValue arg;
				arg.SetNumber(keyCode);
				scope.Invoke("EndRemapMode", NULL, &arg, 1);
				scope.CleanManaged();
			}

			virtual void	Dispose(void) override
			{
				SimpleLocker locker(&s_remapHandlerLock);
				if (s_remapHandler)
				{
					delete s_remapHandler;
					s_remapHandler = nullptr;
				}
			}
			GFxValue						scope;
			UInt32							keyCode;
		};

	public:
		using FnCreateFunction = void(*)(GFxMovieView *, GFxValue *, GFxFunctionHandler *, void *);
		static FnCreateFunction	CreateFunction_Original;
		static SimpleLock		s_remapHandlerLock;
		static RemapHandler		* s_remapHandler;

		void	Invoke_Hook(Args * args)
		{
			ASSERT(args->numArgs >= 1);
			SimpleLocker locker(&s_remapHandlerLock);
			if (!s_remapHandler && (*g_inputDeviceMgr))
			{
				s_remapHandler = new RemapHandler();
				if (s_remapHandler)
				{
					s_remapHandler->scope = args->args[0];
					(*g_inputDeviceMgr)->AddEventSink(s_remapHandler);
				}
			}
		}

		static void CreateFunction_Hook(GFxMovieView * view, GFxValue * value, GFxFunctionHandler * callback, void * refcon)
		{
			constexpr char * className = "class SKSEScaleform_StartRemapMode";
			static bool bSkipCheck = false;
			if (!bSkipCheck && strcmp(typeid(*callback).name(), className) == 0)
			{
				HookUtil::SafeWrite64(*(uintptr_t*)callback + 1 * 8, &Invoke_Hook);
				bSkipCheck = true;
			}
			CreateFunction_Original(view, value, callback, refcon);
		}

		static void InitHooks()
		{
			uintptr_t location = RELOC_RUNTIME_ADDR("48 8D 05 ? ? ? ? 4C 8B F2", 0, 3, 7);
			CreateFunction_Original = HookUtil::SafeWrite64(location + 8 * 0xF, CreateFunction_Hook);
		}
	};
	StartRemapModeEx::FnCreateFunction	StartRemapModeEx::CreateFunction_Original = nullptr;
	StartRemapModeEx::RemapHandler *	StartRemapModeEx::s_remapHandler = nullptr;
	SimpleLock							StartRemapModeEx::s_remapHandlerLock;


	class ConsoleCommand
	{
	public:
		using FnGetCommandArgs = bool(*)(const ObScriptParam *, ObScriptCommand::ScriptData *, void *, TESObjectREFR *, void *, void *, void *, ...);
		static FnGetCommandArgs GetCommandArgs;

		static bool Cmd_SetSkyrimSoulsVariable_Execute(const ObScriptParam * paramInfo, ObScriptCommand::ScriptData * scriptData, TESObjectREFR * thisObj, void * containingObj, void * scriptObj, void * locals, double& result, void * opcodeOffsetPtr)
		{
			char menuName[MAX_PATH] = {};
			UInt32 option = 0;
			GetCommandArgs(paramInfo, scriptData, opcodeOffsetPtr, thisObj, containingObj, scriptObj, locals, &menuName, &option);
			for (auto & menuSetting : CoreController::globalMenuSettings)
			{
				std::string settingName = menuSetting.menuName;
				settingName.erase(std::remove(settingName.begin(), settingName.end(), ' '));
				settingName.erase(std::remove(settingName.begin(), settingName.end(), '/'));
				if (_stricmp(settingName.c_str(), menuName) == 0)
				{
					menuSetting.isEnabled = option != 0;
					Console_Print("> SkyrimSouls::%s=%d", settingName.c_str(), option != 0);
					constexpr char * configFile = ".\\Data\\SKSE\\Plugins\\SkyrimSouls.ini";
					constexpr char * settingsSection = "Settings";
					settingName = "b" + settingName;
					WritePrivateProfileStringA(settingsSection, settingName.c_str(), std::to_string(menuSetting.isEnabled).c_str(), configFile);
				}
			}
			return true;
		}

		static void InitHooks()
		{
			GetCommandArgs = reinterpret_cast<FnGetCommandArgs>(RELOC_RUNTIME_ADDR("4C 89 4C 24 ? 48 89 4C 24 ? 55 41 56"));
			ObScriptCommand * firstConsoleCommand = reinterpret_cast<ObScriptCommand *>(RELOC_RUNTIME_ADDR("48 8D 0D ? ? ? ? 48 03 C1 C3 8D 81 ? ? ? ?", 0, 3, 7));
			for (ObScriptCommand * iter = firstConsoleCommand; iter->opcode < kObScript_NumConsoleCommands + kObScript_ConsoleOpBase; ++iter)
			{
				if (!strcmp(iter->longName, "TestCode"))
				{
					ObScriptCommand cmd = *iter;

					static ObScriptParam params[] = {
						{ "String", ObScriptParam::kType_String, 0 },
						{ "Integer", ObScriptParam::kType_Integer, 0 }
					};
					cmd.longName = "SetSkyrimSoulsVariable";
					cmd.shortName = "sssv";
					cmd.helpText = "";
					cmd.needsParent = 0;
					cmd.numParams = 2;
					cmd.params = params;
					cmd.execute = (ObScript_Execute)Cmd_SetSkyrimSoulsVariable_Execute;
					cmd.flags = 0;

					SafeWriteBuf((uintptr_t)iter, &cmd, sizeof(cmd));
					return;
				}
			}
		}
	};
	ConsoleCommand::FnGetCommandArgs		ConsoleCommand::GetCommandArgs = nullptr;


	class CrashMiniDump
	{
	public:
		static void CreateMiniDumpFile(EXCEPTION_POINTERS * pExceptionInfo)
		{
			SYSTEMTIME localTime;
			GetLocalTime(&localTime);

			char fileName[MAX_PATH] = { };
			sprintf_s(fileName, "CrashDump-%04d-%02d-%02d-%02d-%02d-%02d.dmp", localTime.wYear, localTime.wMonth, localTime.wDay,\
				localTime.wHour, localTime.wMinute, localTime.wSecond);

			HANDLE fileHandle = CreateFileA(fileName, GENERIC_READ | GENERIC_WRITE,
				0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

			if ((fileHandle != NULL) && (fileHandle != INVALID_HANDLE_VALUE))
			{
				MINIDUMP_EXCEPTION_INFORMATION mdei;

				mdei.ThreadId = GetCurrentThreadId();
				mdei.ExceptionPointers = pExceptionInfo;
				mdei.ClientPointers = FALSE;

				BOOL result = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), fileHandle, MiniDumpNormal, (pExceptionInfo != 0) ? &mdei : nullptr, 0, nullptr);
				if (result)
					MessageBoxA(NULL, "SkyrimSE.exe crashed, mini dump file has been created in the folder where SkyrimSE.exe is located.", "SkyrimSE", MB_OK);
				CloseHandle(fileHandle);
			}
		}

		static LONG WINAPI UnhandledExceptionFilter(EXCEPTION_POINTERS * pExceptionInfo)
		{
			static bool bSkip = false;
			if (!bSkip)
			{
				bSkip = true;
				CreateMiniDumpFile(pExceptionInfo);
			}
			return EXCEPTION_CONTINUE_SEARCH; 
		}

		static LPTOP_LEVEL_EXCEPTION_FILTER WINAPI SetUnhandledExceptionFilter_Hook(LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter)
		{
			uintptr_t location = reinterpret_cast<uintptr_t>(lpTopLevelExceptionFilter);
			SafeWrite8(location + 1, 0);
			return SetUnhandledExceptionFilter(UnhandledExceptionFilter);
		}

		static void InitHooks()
		{
			uintptr_t location = RELOC_RUNTIME_ADDR("FF 15 ? ? ? ? 33 C9 FF 15 ? ? ? ? E8 ? ? ? ?");
			g_branchTrampoline.Write6Call(location, (uintptr_t)SetUnhandledExceptionFilter_Hook);
		}
	};


	void RelocateRuntimeData()
	{
		RELOC_GLOBAL_VAL(LookupREFRByHandle, "E8 ? ? ? ? 90 49 8B 0E E8 ? ? ? ?", 0, 1, 5);
		RELOC_GLOBAL_VAL(CalculateCRC32_64, "48 89 5C 24 ? 0F B6 C2 48 8D 1D ? ? ? ? 4C 8B D2");
		RELOC_GLOBAL_VAL(CalculateCRC32_32, "48 89 5C 24 ? 0F B6 C2 48 8D 1D ? ? ? ? 44 8B D2");

		RELOC_MEMBER_FN_EX(SimpleLock, LockThis, "E8 ? ? ? ? 90 48 83 C3 10 41 83 C8 FF", 0, 1, 5);

		RELOC_MEMBER_FN(BSFixedString, ctor, "40 53 48 83 EC 20 45 33 DB");
		RELOC_MEMBER_FN(BSFixedString, Set, "40 53 48 83 EC 20 48 8B 01 48 8B D9 48 89 44 24 ?");
		RELOC_MEMBER_FN(BSFixedString, Release, "E8 ? ? ? ? FF C7 48 83 C5 78", 0, 1, 5);

		RELOC_MEMBER_FN(Heap, Allocate, "E8 ? ? ? ? 48 89 05 ? ? ? ? 48 85 C0 74 24", 0, 1, 5);
		RELOC_MEMBER_FN(Heap, Free, "E8 ? ? ? ? 90 48 8B 53 20 48 85 D2 74 0C", 0, 1, 5);

		RELOC_MEMBER_FN(GFxValue::ObjectInterface, SetMember, "E8 ? ? ? ? 33 D2 41 B8 ? ? ? ? 48 8D 4D F0", 0, 1, 5);
		RELOC_MEMBER_FN(GFxValue::ObjectInterface, HasMember, "E8 ? ? ? ? 84 C0 75 1C 48 8B 46 10", 0, 1, 5);
		RELOC_MEMBER_FN(GFxValue::ObjectInterface, GetMember, "E8 ? ? ? ? 48 8B 03 4C 8D 05 ? ? ? ?", 0, 1, 5);
		RELOC_MEMBER_FN(GFxValue::ObjectInterface, Invoke, "E8 ? ? ? ? 44 89 3D ? ? ? ? C6 05 ? ? ? ? ?", 0, 1, 5);
		RELOC_MEMBER_FN(GFxValue::ObjectInterface, PushBack, "40 53 48 83 EC 40 49 8B C0");
		RELOC_MEMBER_FN(GFxValue::ObjectInterface, AddManaged_Internal, "48 83 EC 28 8B 42 08 25 ? ? ? ?");
		RELOC_MEMBER_FN(GFxValue::ObjectInterface, ReleaseManaged_Internal, "40 53 48 83 EC 20 8B 42 08");

		RELOC_MEMBER_FN(ConsoleManager, VPrint, "66 83 BD ? ? ? ? ? 75 52", -0x57);

		RELOC_MEMBER_FN_EX(MenuManager, IsMenuOpen_Internal, "E8 ? ? ? ? 84 C0 74 2D 80 3D ? ? ? ? ?", 0, 1, 5);
		RELOC_MEMBER_FN_EX(MenuManager, RegisterMenu, "48 8B C4 53 56 57 48 83 EC 50 48 C7 40 ? ? ? ? ?");
		RELOC_MEMBER_FN_EX(MenuManager, GetTopMenu, "E8 ? ? ? ? 48 8B 4D 6F 80 79 18 02", 0, 1, 5);
		RELOC_MEMBER_FN_EX(MenuManager, ProcessMessage, "E8 ? ? ? ? 45 84 FF 74 0E 45 0F B6 C6", 0, 1, 5);
		RELOC_MEMBER_FN_EX(MenuManager, DrawNextFrame, "E8 ? ? ? ? E8 ? ? ? ? 48 8B C8 E8 ? ? ? ? 48 89 7C 24 ?", 0, 1, 5);
		RELOC_MEMBER_FN_EX(MenuManager, IsInMenuMode, "80 3D ? ? ? ? ? 75 0C 80 3D ? ? ? ? ?");

		RELOC_MEMBER_FN_EX(UIMessageManager, SendUIMessage, "E8 ? ? ? ? 48 8B 15 ? ? ? ? 48 81 C2 ? ? ? ? 45 33 C9 45 8D 41 0B", 0, 1, 5);

		RELOC_MEMBER_FN_EX(PlayerCharacter, IsCasting, "48 89 5C 24 ? 48 89 6C 24 ? 56 41 56 41 57 48 83 EC 20 32 DB");
		RELOC_MEMBER_FN_EX(PlayerCharacter, InterruptCast, "E8 ? ? ? ? B0 01 48 83 C4 38 C3 B2 31", 0, 1, 5);
		RELOC_MEMBER_FN_EX(PlayerCharacter, GetDistance, "E8 ? ? ? ? F3 0F 11 44 24 ? 48 8D 54 24 ?", 0, 1, 5);
		RELOC_MEMBER_FN_EX(PlayerCharacter, GetDefaultCameraPos, "E8 ? ? ? ? F3 44 0F 10 20", 0, 1, 5);

		RELOC_MEMBER_FN_EX(PlayerCamera, UpdateThirdPerson, "E8 ? ? ? ? 48 3B 1D ? ? ? ? 75 1C 48 8B 0D ? ? ? ?", 0, 1, 5);
		RELOC_MEMBER_FN_EX(PlayerCamera, UpdateCameraPostion, "E8 ? ? ? ? 40 B6 01 48 8B 87 ? ? ? ?", 0, 1, 5);


		RELOC_MEMBER_FN_EX(BaseEventDispatcher, AddEventSink_Internal, "E8 ? ? ? ? 48 8B 0D ? ? ? ? 48 83 C1 08 48 8D 96 ? ? ? ?", 0, 1, 5);
		RELOC_MEMBER_FN_EX(BaseEventDispatcher, RemoveEventSink_Internal, "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 20 33 FF 48 8D 71 10", 0x4F0, 1, 5);
		RELOC_MEMBER_FN_EX(BaseEventDispatcher, SendEvent_Internal, "4D 85 E4 74 1A 49 8B 0C 24", 0x36, 1, 5);

		RELOC_MEMBER_FN_EX(MenuHandler, Update, "E8 ? ? ? ? 48 8B 0D ? ? ? ? E8 ? ? ? ? 84 C0 74 10", 0, 1, 5);

		RELOC_GLOBAL_VAL(g_mainHeap, "48 8D 0D ? ? ? ? E8 ? ? ? ? 90 48 8B 53 20", 0, 3, 7);
		RELOC_GLOBAL_VAL(g_gameSettingCollection, "48 8B 0D ? ? ? ? 48 8B 01 4C 8B C7 49 8B D6", 0, 3, 7);
		RELOC_GLOBAL_VAL(g_thePlayer, "48 3B 3D ? ? ? ? 74 69", 0, 3, 7);
		RELOC_GLOBAL_VAL(g_menuManager, "48 8B 05 ? ? ? ? 83 B8 ? ? ? ? ? 77 6F 8B 55 B0", 0, 3, 7);
		RELOC_GLOBAL_VAL(g_main, "48 8B 0D ? ? ? ? 3B 41 20 0F 85 ? ? ? ?", 0, 3, 7);
		RELOC_GLOBAL_VAL(g_uiMessageManager, "48 8B 0D ? ? ? ? E8 ? ? ? ? EB 25", 0, 3, 7);
		RELOC_GLOBAL_VAL(g_uiStringHolder, "48 8B 0D ? ? ? ? E8 ? ? ? ? EB 25", -0x14, 3, 7);
		RELOC_GLOBAL_VAL(g_playerCamera, "48 8B 0D ? ? ? ? E8 ? ? ? ? 40 B6 01 48 8B 87 ? ? ? ?", 0, 3, 7);
		RELOC_GLOBAL_VAL(g_inputDeviceMgr, "48 8B 0D ? ? ? ? E8 ? ? ? ? 4C 8B 00 48 8D 55 D8", 0, 3, 7);
		RELOC_GLOBAL_VAL(g_console, "48 89 05 ? ? ? ? 8B 05 ? ? ? ? 0F 57 C0", 0, 3, 7);
		RELOC_GLOBAL_VAL(g_threadEventHandleMgr, "48 89 05 ? ? ? ? 41 89 1C 3E E8 ? ? ? ? B2 01", 0, 3, 7);
		RELOC_GLOBAL_VAL(globalMenuStackLock, "48 8D 05 ? ? ? ? 48 89 45 60 33 D2", 0, 3, 7);
	}

}

namespace SkyrimSouls
{
	template <UInt32 INDEX>
	class Callback
	{
	public:
		typedef IMenu * (*CallbackType)(void);

		Callback() : m_callback(nullptr) {}

		bool ReplaceFunctor()
		{
			bool result = false;
			if ((*g_menuManager) != nullptr)
			{
				const char * menuName = nullptr;
				if (INDEX < CoreController::globalMenuSettings.size())
					menuName = CoreController::globalMenuSettings[INDEX].menuName.c_str();
				SimpleLocker locker(&(*g_menuManager)->menuTableLock);
				auto & pHashSet = (*g_menuManager)->menuTable;
				auto * pItem = pHashSet.Find(&BSFixedString(menuName));
				if (pItem != nullptr)
				{
					m_callback = reinterpret_cast<CallbackType>(pItem->menuConstructor);
					pItem->menuConstructor = CreateGameMenuInstance;
					if (INDEX == kType_Console)
					{
						static BSFixedString console("Console");
						if (pItem->menuInstance && !(*g_menuManager)->IsMenuOpen(console))
						{
							if (CoreController::globalMenuSettings[kType_Console].isEnabled)
							{
								pItem->menuInstance->flags &= ~IMenu::kFlag_PauseGame;
								pItem->menuInstance->flags &= ~IMenu::kFlag_StopDrawingWorld;
								pItem->menuInstance->flags |= IMenu::kFlag_StopCrosshairUpdate;
								pItem->menuInstance->flags |= IMenu::kFlag_SkyrimSoulsMenu;
							}
							else
							{
								pItem->menuInstance->flags |= IMenu::kFlag_PauseGame;
								pItem->menuInstance->flags &= ~IMenu::kFlag_StopCrosshairUpdate;
								pItem->menuInstance->flags &= ~IMenu::kFlag_SkyrimSoulsMenu;
							}
						}
					}
					_MESSAGE("Callback<%s>::Register()", menuName);
					result = true;
				}
			}
			return result;
		}

		static Callback * GetSingleton()
		{
			static Callback singleton;
			return &singleton;
		}

		static void	Register()
		{
			GetSingleton()->ReplaceFunctor();
		}

		static IMenu * CreateGameMenuInstance()
		{
			auto callback = GetSingleton()->m_callback;
			IMenu * pResult = nullptr;
			if (callback != nullptr && (pResult = callback(), pResult)\
				&& CoreController::globalMenuSettings[INDEX].isEnabled)
			{
				pResult->flags &= ~IMenu::kFlag_PauseGame;
				pResult->flags &= ~IMenu::kFlag_StopDrawingWorld;
				pResult->flags |= (pResult->flags & IMenu::kFlag_StopCrosshairUpdate) ? IMenu::kFlag_PreventGameLoad : IMenu::kFlag_StopCrosshairUpdate;
				pResult->flags |= IMenu::kFlag_SkyrimSoulsMenu;
				pResult->menuDepth = (!pResult->menuDepth) ? 0x1 : pResult->menuDepth;
			}
			return pResult;
		}

		CallbackType				m_callback;
	};



	class GameplayControlHandler : public BSTEventSink<MenuOpenCloseEvent>
	{
	public:
		virtual ~GameplayControlHandler() { };
		virtual	EventResult ReceiveEvent(MenuOpenCloseEvent * evn, EventDispatcher<MenuOpenCloseEvent> * dispatcher) override
		{
			//_MESSAGE("%s=%d", evn->menuName.c_str(), (*g_menuManager)->IsInMenuMode());
			auto functor = [evn](const MenuSetting & setting)->bool {return (setting.menuName == evn->menuName.c_str()/* && setting.isEnabled*/) ? true : false; };
			if (std::find_if(CoreController::globalMenuSettings.begin(), CoreController::globalMenuSettings.end(), functor) != CoreController::globalMenuSettings.end())
			{
				CoreController::globalControlCounter = (*g_menuManager)->GetFlagCount(IMenu::kFlag_SkyrimSoulsMenu);
				if (CoreController::globalControlCounter)
				{
					PlayerControlsEx::DisableUserEvent(true);
					if ((*g_menuManager)->IsMenuOpen((*g_uiStringHolder)->dialogueMenu))
					{
						(*g_uiMessageManager)->SendUIMessage((*g_uiStringHolder)->dialogueMenu, DialogueMenu::kMessage_Invisible, nullptr);
					}
					if ((*g_thePlayer) && (*g_thePlayer)->actorState.IsSprinting())
					{
						(*g_thePlayer)->animGraphHolder.SendAnimationEvent("sprintStop");
					}
					if ((*g_thePlayer) && (*g_thePlayer)->IsCasting(false))
					{
						(*g_thePlayer)->InterruptCast(static_cast<bool>((~(((*g_thePlayer)->actorState.flags0C) >> 13)) & 1));
					}
				}
				else
				{
					PlayerControlsEx::DisableUserEvent(false);
					if ((*g_menuManager)->IsMenuOpen((*g_uiStringHolder)->dialogueMenu))
					{
						(*g_uiMessageManager)->SendUIMessage((*g_uiStringHolder)->dialogueMenu, DialogueMenu::kMessage_Visible, nullptr);
					}
				}
			}
			if (!evn->opening)
			{
				BSFixedString & console = (*g_uiStringHolder)->console;
				if (evn->menuName == console)
				{
					SimpleLocker locker(&(*g_menuManager)->menuTableLock);
					auto & pHashSet = (*g_menuManager)->menuTable;
					auto * pItem = pHashSet.Find(&console);
					if (pItem && pItem->menuInstance)
					{
						if (CoreController::globalMenuSettings[kType_Console].isEnabled)
						{
							pItem->menuInstance->flags &= ~IMenu::kFlag_PauseGame;
							pItem->menuInstance->flags |= IMenu::kFlag_StopCrosshairUpdate;
							pItem->menuInstance->flags |= IMenu::kFlag_SkyrimSoulsMenu;
						}
						else
						{
							pItem->menuInstance->flags |= IMenu::kFlag_PauseGame;
							pItem->menuInstance->flags &= ~IMenu::kFlag_StopCrosshairUpdate;
							pItem->menuInstance->flags &= ~IMenu::kFlag_SkyrimSoulsMenu;
						}
					}
				}
			}
			return kEvent_Continue;
		};

		static void RegisterHandler()
		{
			if ((*g_menuManager) != nullptr)
			{
				static auto * pHandler = new GameplayControlHandler();
				(*g_menuManager)->menuOpenCloseEventDispatcher.AddEventSink(pHandler);
			}
		}

		static void RegisterCoreControls()
		{
			static bool isInit = false;
			if (!isInit)
			{
				isInit = true;
				Callback<kType_Console>::Register();
				Callback<kType_Tutorial>::Register();
				Callback<kType_MessageBox>::Register();
				Callback<kType_Tween>::Register();
				Callback<kType_Inventory>::Register();
				Callback<kType_Magic>::Register();
				Callback<kType_Container>::Register();
				Callback<kType_Favorites>::Register();
				Callback<kType_Barter>::Register();
				Callback<kType_Training>::Register();
				Callback<kType_Lockpicking>::Register();
				Callback<kType_Book>::Register();
				Callback<kType_Gift>::Register();
				Callback<kType_Journal>::Register();
				Callback<kType_SleepWait>::Register();
				Callback<kType_LevelUp>::Register();
				Callback<kType_Stats>::Register();
				Callback<kType_Map>::Register();
				Callback<kType_ModManager>::Register();
				Callback<kType_Custom>::Register();
				GameplayControlHandler::RegisterHandler();
			}
		}
	};
}


namespace SkyrimSouls
{
	void SKSEMessageHandler(SKSEMessagingInterface::Message * msg)
	{
		if (msg->type == SKSEMessagingInterface::kMessage_DataLoaded)
		{
			GameplayControlHandler::RegisterCoreControls();
		}
	}

	void InstallHooks()
	{
		static bool isInit = false;
		if (!isInit)
		{
			isInit = true;
			LoadSettings();
			try
			{
				RelocateRuntimeData();
				BGSSaveLoadManager::InitHooks();
				MenuManagerEx::InitHooks();
				DialogueMenu::InitHooks();
				StatsMenu::InitHooks();
				MapMenu::InitHooks();
				PlayerCameraEx::InitHooks();
				JournalMenu::InitHooks();
				FavoritesMenu::InitHooks();
				FavoritesHandler::InitHooks();
				BookMenu::InitHooks();
				ContainerMenu::InitHooks();
				TweenMenu::InitHooks();
				SleepWaitMenu::InitHooks();
				MainEx::InitHooks();
				PlayerControlsEx::InitHooks();
				JobListManager::InitHooks();
				VirtualMachineEx::InitHooks();
				StartRemapModeEx::InitHooks();
				ConsoleCommand::InitHooks();
				CrashMiniDump::InitHooks();
			}
			catch (const no_result_exception & exception)
			{
				_MESSAGE(exception.what());
				MessageBoxA(nullptr, "Init process failed, please deactive SkyrimSouls and wait for updating.", "SkyrimSouls", MB_OK);
			}

			//00165FFC0 aBgs_logo_bik   db 'BGS_Logo.bik',0     ; DATA XREF: .data:off_1E13B78o
			//SafeWrite32(RelocAddr<uintptr_t>(0x00165FFC0), 0);

			if (g_messaging != nullptr)
				g_messaging->RegisterListener(g_pluginHandle, "SKSE", SKSEMessageHandler);
		}
	}
}