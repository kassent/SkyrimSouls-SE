#pragma once
#include "skse64/GameMenus.h"
#include "skse64/GameReferences.h"
namespace SkyrimSouls
{
	enum MessageType
	{
		kMessage_Refresh = 0,				// 0 used after ShowAllMapMarkers
		kMessage_Open,						// 1
		kMessage_PreviouslyKnownAsClose,	// 2
		kMessage_Close,						// 3
		kMessage_Unk04,
		kMessage_Unk05,
		kMessage_Scaleform,					// 6 BSUIScaleformData
		kMessage_Message					// 7 BSUIMessageData
	};


	class UIMessage
	{
	public:

		BSFixedString		name;			// 00 - menuName or "TopMenu"
		MessageType			type;			// 08
		UInt32				pad0C;			// 0C
		IUIMessageData		* extraData;	// 10 - something with a virtual destructor
		UInt8				isPooled;		// 18
		UInt8				pad19[7];		// 19
	};


	//0x30
	class IMenu : public FxDelegateHandler
	{

	public:
		enum
		{
			kFlag_PauseGame = 0x01,
			kFlag_DoNotDeleteOnClose = 0x02,
			kFlag_ShowCursor = 0x04,
			kFlag_Unk0008 = 0x08,
			kFlag_Modal = 0x10,
			kFlag_StopDrawingWorld = 0x20,
			kFlag_Open = 0x40,
			kFlag_PreventGameLoad = 0x80,
			kFlag_Unk0100 = 0x100,
			kFlag_HideOther = 0x200,
			kFlag_Unk0400 = 0x400,
			kFlag_DoNotPreventGameSave = 0x800,
			kFlag_Unk1000 = 0x1000,
			kFlag_ItemMenu = 0x2000,
			kFlag_StopCrosshairUpdate = 0x4000,
			kFlag_Unk8000 = 0x8000,
			kFlag_Unk10000 = 0x10000,	// mouse cursor
			kFlag_Unk20000 = 0x20000,
			
			kFlag_PreventGameLoadReplacer = 0x20000000,
			kFlag_IncMenuModeCounter = 0x40000000,
			kFlag_SkyrimSoulsMenu = 0x80000000
		};

		//struct BSUIScaleformData
		//{
		//	virtual ~BSUIScaleformData() {}

		//	UInt32				unk04; // 04
		//	void*				unk08; // 08
		//};

		virtual ~IMenu();

		virtual void	Accept(CallbackProcessor * processor) {}
		virtual void	Unk_02(void);
		virtual void	Unk_03(void);
		virtual UInt32	ProcessMessage(UIMessage * data);
		virtual void	NextFrame(float arg0, UInt32 arg1);
		virtual void	Render(void);
		virtual void	Unk_07(void);
		virtual void	InitMovie(void);

		GFxMovieView	* view;			// 10 - init'd to 0, a class, virtual fn 0x228 called in dtor
		UInt8			menuDepth;		// 18 - init'd to 3 - movie clip depth ?
										//    - 0x0 0000 ContainerMenu, BarterMenu
										//    - 0x2 0010 HUD Menu
										//    - 0x3 0011 Fader Menu, StatsMenu, MapMenu, FavoritesMenu, Training Menu
										//    - 0x5 0101 Journal Menu
										//    - 0x8 1000 Mist Menu
										//    - 0x9 1001 Loading Menu, Main Menu
										//    - 0xA 1010 Credits Menu
										//    - 0xC 1100 Console
										//    - 0xD 1101 Cursor Menu
		UInt8			pad19[3];		// 19
		UInt32			flags;			// 1C - init'd to 0	- 0x01 PauseGame, 0x40 Open
										//    -	(HEX)      (NAME)             1 8421:8421 8421 8421
										//    - 0x00000803 Credits Menu        :    :    :    :   *
										//    - 0x00000803 Loading Menu        :    :*   :    :  **
										//    - 0x00004880 Mist Menu           : *  :*   :*   :    
										//    - 0x00008800 Fader Menu          :*   :*   :    :    
										//    - 0x00000004 TitleSequence Menu  :    :    :    : *
										//    - 0x00000585 Main Menu           :    : * *:*   : * *
										//    - 0x00008800 Cursor Menu         :*   :*   :    :    
										//    - 0x00018902 HUD Menu           *:*   :*  *:    :  *
										//    - 0x00000E2D Journal Menu        :    :*** :  * :** *
										//    - 0x0000818D StatsMenu           :*   :   *:*   :** *
										//    - 0x00009005 MapMenu             :*  *:    :    : * *
										//    - 0x0000CC05 TweenMenu           :**  :**  :    : * *
										//    - 0x00000905 Sleep/Wait Menu     :    :*  *:    : * *
										//    - 0x00000415 Tutorial Menu       :    : *  :   *: * *
										//    - 0x0000040D Training Menu       :    : *  :    :** *
										//    - 0x00000807 Console             :    :*   :    : ***
										//    - 0x0000648C Crafting Menu       : ** : *  :*   :**
										//    - 0x00004404 Dialogue Menu       : *  : *  :    : *
										//    - 0x0000A485 InventoryMenu       :* * : *  :*   : * *
										//    - 0x0000A48D ContainerMenu       :* * : *  :*   :** *
										//    - 0x0000A48D BarterMenu          :* * : *  :*   :** *
										//    - 0x0000A48D MagicMenu           :* * : *  :*   :** *
										//    - 0x0000A405 FavoritesMenu       :* * : *  :    : * *
										//    - 0x0000070D RaceSex Menu        :    : ***:    :** *
										//    -	(HEX)      (NAME)             1 8421:8421 8421 8421
										//    -                                  ||  |||  ||||  ||+--0001 Pause
										//    -                                  ||  |||  ||||  |+---0002 Do Not Delete On Close
										//    -                                  ||  |||  ||||  +----0004 Show Cursor Menu
										//    -                                  ||  |||  |||+-------0010 Modal, Prevent Other Menu
										//    -                                  ||  |||  ||+--------0020 Stop Drawing World
										//    -                                  ||  |||  |+---------0040 Open
										//    -                                  ||  |||  +----------0080 Prevent Game Load
										//    -                                  ||  ||+-------------0200 Hide Other Menu
										//    -                                  ||  |+--------------0400 Mouse ?
										//    -                                  ||  +---------------0800 Do Not Prevent Save Game
										//    -                                  |+------------------2000 Item Menu, Prevent Other Item Menu
										//    -                                  +-------------------4000 Stop Updating Crosshair, Disable Journal, Prevent Game Load

		/*													 A A A A A A A A B B B B B B B B C C C C C C C C D D D D D D D D
										Main Menu			 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 1 0 1 1 1 0 0 0 1 0 1		depth: 0009		context: 1
										Cursor Menu			 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 1 0 0 0 0 1 0 0 0 0 0 0		depth: 000D		context: 18
										LoadWaitSpinner		 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1 0		depth: 000A		context: 18
										Loading Menu		 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1 1		depth: 0009		context: 18
										Fader Menu			 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 1 0 0 0 0 1 0 0 0 0 1 0		depth: 0003		context: 18
										HUD Menu			 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 1 0 0 0 1 0 0 1 0 1 0 0 0 0 1 0		depth: 0002		context: 18
										Dialogue Menu		 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 1 0 0 0 1 0 0 0 1 0 0		depth: 0003		context: 1
										Training Menu		 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 1 0 0 1 1 0 1		depth: 0003		context: 18
										BarterMenu			 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 1 0 0 1 0 0 1 1 0 0 1 1 0 1		depth: 0000		context: 3
										Journal Menu		 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 1 1 1 0 1 1 0 1 1 0 1		depth: 0005		context: 12
										InventoryMenu		 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 1 0 0 1 0 0 1 1 0 0 0 1 0 1		depth: 0000		context: 18
										MagicMenu			 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 1 0 0 1 0 0 1 1 0 0 1 1 0 1		depth: 0000		context: 3
										TweenMenu			 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 1 0 0 1 1 0 0 0 1 0 0 0 1 0 1		depth: 0000		context: 1
										Console				 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 1 0 0 0 1 1 1		depth: 000C		context: 2
										Lockpicking Menu	 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 1 1 0 0 0 0 0 1		depth: 0003		context: 15
										MapMenu				 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 1 0 0 0 0 0 1 0 0 0 0 0 1		depth: 0003		context: 7
										StatsMenu			 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 1 1 1 0 0 1 1 0 1		depth: 0003		context: 8
										MessageBoxMenu		 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 1 0 1 0 1 0 1		depth: 0004		context: 1
										Mist Menu			 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 1 0 0 0 1 1 0 0 0 0 0 0		depth: 0008		context: 18
										Tutorial Menu		 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 1 0 1 0 1 0 1		depth: 000A		context: 1
										LevelUp Menu		 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 1 0 1 0 1 0 1		depth: 0003		context: 1
										ContainerMenu		 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 1 0 0 1 0 0 1 1 0 0 1 1 0 1		depth: 0000		context: 3
										Book Menu			 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 1 1 1 1 0 0 1 0 0 1		depth: 0001		context: 10
										Crafting Menu		 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 1 0 0 1 0 0 1 1 0 0 1 1 0 0		depth: 0000		context: 3
		*/
		UInt32			context;		// 20 - init'd to 0x12
		UInt32			pad24;			// 24 
		GRefCountBase	* unk18;		// 28 - holds a reference
	};
	STATIC_ASSERT(offsetof(IMenu, view) == 0x10);
	STATIC_ASSERT(sizeof(IMenu) == 0x30);

	// 018
	class MenuTableItem
	{
	public:
		BSFixedString	name;				// 000
		IMenu			* menuInstance;		// 008	0 if the menu is not currently open
		void			* menuConstructor;	// 010

		bool operator==(const MenuTableItem & rhs) const { return name == rhs.name; }
		bool operator==(const BSFixedString a_name) const { return name == a_name; }
		operator UInt64() const { return (UInt64)name.data; }

		static inline UInt32 GetHash(BSFixedString * key)
		{
			UInt32 hash;
			CalculateCRC32_64(&hash, (UInt64)key->data);
			return hash;
		}

		void Dump(void)
		{
			_MESSAGE("name: %s | instance: %016I64X | menuConstructor: %016I64X", name, menuInstance, (uintptr_t)menuConstructor - RelocationManager::s_baseAddr);
		}
	};

	// 1C8
	extern RelocPtr<SimpleLock>			globalMenuStackLock;

	class MenuManager
	{
		typedef tHashSet<MenuTableItem, BSFixedString> MenuTable;

		// 030-040
		struct Unknown3
		{
			UInt64		unk00;		// 000 - New in SE. Not init'd?

			UInt64		frequency;	// 008 

			UInt64		unk_010;	// 010 (= 0)
			UInt32		unk_018;	// 018 (= 0)
			UInt32		unk_01C;	// 01C (= 0)
			UInt32		unk_020;	// 020 (= 0)
			UInt32		unk_024;	// 024 (= 0)
			float		unk_028;	// 028 (= frequency related)

			UInt32		unk_02C;	// 02C
			UInt32		unk_030;	// 030

			UInt32		unk_034;	// 034 (= 0)
			UInt16		unk_038;	// 038
			UInt8		unk_03A;	// 03A (= 0)
			UInt8		pad[5];		// 03B
		};
		STATIC_ASSERT(sizeof(Unknown3) == 0x40);

	public:
		UInt64					unk_000;	// 000

		EventDispatcher<MenuOpenCloseEvent>		menuOpenCloseEventDispatcher;	// 008
		EventDispatcher<MenuModeChangeEvent>	menuModeChangeEventDispatcher;	// 060
		EventDispatcher<void*>					unk_064;						// 0B8 - New in 1.6.87.0 - Kinect related?

		tArray<IMenu*>			menuStack;	// 110
		MenuTable				menuTable;	// 128   (Entries ptr at 150)
		SimpleLock				menuTableLock;	// 158
		UInt32					numPauseGame;				// 160 (= 0) += 1 if (imenu->flags & 0x0001)
		UInt32					numItemMenu;				// 164 (= 0) += 1 if (imenu->flags & 0x2000)
		UInt32					numPreventGameLoad;			// 168 (= 0) += 1 if (imenu->flags & 0x0080)
		UInt32					numDoNotPreventSaveGame;	// 16C (= 0) += 1 if (imenu->flags & 0x0800)
		UInt32					numStopCrosshairUpdate;		// 170 (= 0) += 1 if (imenu->flags & 0x4000)
		UInt32					numFlag8000;				// 174 (= 0) += 1 if (imenu->flags & 0x8000)
		UInt32					numFlag20000;				// 178 (= 0)  = 1 if (imenu->flags & 0x20000)
		UInt8					numModal;					// 17C (= 0)  = 1 if (imenu->flags & 0x10)
		UInt8					pad_17D[3];	// 17D
		Unknown3				unk_180;	// 180
		bool					showMenus;	// 1C0 (= 0)
		bool					unk_1C1;	// 1C1 (= 0)
		char					pad[6];		// 1C2

	public:

		inline UInt32 GetFlagCount(UInt32 flag)
		{
			SimpleLocker locker(globalMenuStackLock);
			UInt32 result = 0;
			for (UInt32 i = 0; i < menuStack.count; ++i)
			{
				auto * pMenu = menuStack[i];
				if (pMenu && pMenu->flags & flag)
					++result;
			}
			return result;
		}

		EventDispatcher<MenuOpenCloseEvent> * MenuOpenCloseEventDispatcher()
		{
			return &menuOpenCloseEventDispatcher;
		}

		void				ShowMenus(bool show) { showMenus = show; }
		bool				IsShowingMenus() const { return showMenus; }

		typedef IMenu* (*CreatorFunc)(void);

		IMenu * GetMenu(BSFixedString * menuName);
		bool	IsFlagEnabled(BSFixedString & menuName, UInt32 flag);
		bool	IsSkyrimSoulsMenu(BSFixedString & menuName);

		inline bool	IsMenuOpen(BSFixedString & menuName)
		{
			SimpleLocker locker(&menuTableLock);
			return IsMenuOpen_Internal(menuName);
		}

		DEF_MEMBER_FN(IsMenuOpen_Internal, bool, 0x00EBD7E0, BSFixedString & menuName);
		DEF_MEMBER_FN(RegisterMenu, void, 0x00EBF050, const char * name, CreatorFunc creator);
		DEF_MEMBER_FN(GetTopMenu, void, 0xEBDBA0, IMenu *& menu, UInt8 maxMenuDepth); // - 0xE
		DEF_MEMBER_FN(ProcessMessage, void, 0xEBDCC0);
		DEF_MEMBER_FN(DrawNextFrame, void, 0xEBE8F0);
		DEF_MEMBER_FN(IsInMenuMode, bool, 0x009B8000);
	};
	STATIC_ASSERT(sizeof(MenuManager) == 0x1C8);
	STATIC_ASSERT(offsetof(MenuManager, menuTable) == 0x128);
	STATIC_ASSERT(offsetof(MenuManager, numPauseGame) == 0x160);
	STATIC_ASSERT(offsetof(MenuManager, menuTableLock) == 0x158);

	extern RelocPtr<MenuManager *>		g_menuManager;

	// B80
	class UIMessageManager
	{
	public:

		DEF_MEMBER_FN(SendUIMessage, void, 0x00165450, BSFixedString & menuName, UInt32 msgType, void * objData);

	};
	extern RelocPtr <UIMessageManager*>	g_uiMessageManager;

	extern RelocPtr <UIStringHolder *> g_uiStringHolder;


	class Main
	{
	public:
		UInt8			unk00[0xE];
		bool			isPaused;
		UInt64			unk10[(0x20 - 0x10) >> 3];
		UInt32			threadID;
	};
	STATIC_ASSERT(offsetof(Main, threadID) == 0x20);
	STATIC_ASSERT(offsetof(Main, isPaused) == 0xE);

	extern RelocPtr <Main *>			g_main;


	class MenuEventHandler : public BSIntrusiveRefCounted
	{
	public:
		virtual ~MenuEventHandler();

		virtual bool	CanProcess(InputEvent * evn);				
		virtual bool	ProcessKinect(KinectEvent * evn);				
		virtual bool	ProcessThumbstick(ThumbstickEvent * evn);		
		virtual bool	ProcessMouseMove(MouseMoveEvent * evn);
		virtual bool	ProcessButton(ButtonEvent * evn);
	};


	class PlayerCharacter : public Character
	{
	public:
		DEF_MEMBER_FN(IsCasting, bool, 0x631BB0, bool unk0);
		DEF_MEMBER_FN(InterruptCast, void, 0x631AC0, bool unk0);
		DEF_MEMBER_FN(GetDistance, float, 0x299C00, TESObjectREFR * ref, bool unk1, bool unk2); //true false
		DEF_MEMBER_FN(GetDefaultCameraPos, NiPoint3*, 0x5ED9B0, NiPoint3 &);
	};
	extern RelocPtr<PlayerCharacter*>	g_thePlayer;


	// 168
	class PlayerCamera : public TESCamera
	{
	public:
		PlayerCamera();
		virtual ~PlayerCamera();

		enum
		{
			kCameraState_FirstPerson = 0,
			kCameraState_AutoVanity,
			kCameraState_VATS,
			kCameraState_Free,
			kCameraState_IronSights,
			kCameraState_Furniture,
			kCameraState_Transition,
			kCameraState_TweenMenu,
			kCameraState_ThirdPerson1,
			kCameraState_ThirdPerson2,
			kCameraState_Horse,
			kCameraState_Bleedout,
			kCameraState_Dragon,
			kNumCameraStates
		};

		inline bool IsFirstPerson(void) const {
			return cameraState == cameraStates[kCameraState_FirstPerson];
		}
		inline bool IsThirdPerson(void) const {
			return cameraState == cameraStates[kCameraState_ThirdPerson2];
		}
		inline bool IsFreeCamera(void) const {
			return cameraState == cameraStates[kCameraState_Free];
		}
		UInt32		unk38;									// 038
		UInt32		refHandle;								// 03C
		UInt8		unk40[0xB8 - 0x40];						// 040
		TESCameraState * cameraStates[kNumCameraStates];	// 0B8
		UInt64		unk120;									// 120
		UInt64		unk128;									// 128
		UInt32		unk130;									// 130
		SimpleLock	cameraLock;								// 134
		float		worldFOV;								// 13C
		float		firstPersonFOV;							// 140
		NiPoint3	cameraPos;								// 144
		float		idleTimer;								// 150
		float		unk154;									// 154
		UInt64		unk158;									// 158
		UInt8		unk160;									// 160
		UInt8		unk161;									// 161
		UInt8		unk162;									// 162 - init'd to 1
		bool		isProcessed;							// 163
		UInt8		unk164;									// 164
		UInt8		unk165;									// 165
		UInt8		pad166[2];								// 166

		DEF_MEMBER_FN(UpdateThirdPerson, void, 0x0084D1B0, bool weaponDrawn);
		DEF_MEMBER_FN(UpdateCameraPostion, bool, 0x84A990, NiPoint3 & out);
	};
	STATIC_ASSERT(offsetof(PlayerCamera, cameraStates) == 0xB8);
	STATIC_ASSERT(offsetof(PlayerCamera, cameraPos) == 0x144);
	STATIC_ASSERT(offsetof(PlayerCamera, cameraLock) == 0x134);
	STATIC_ASSERT(offsetof(PlayerCamera, worldFOV) == 0x13C);
	STATIC_ASSERT(offsetof(PlayerCamera, refHandle) == 0x3C);
	STATIC_ASSERT(offsetof(PlayerCamera, isProcessed) == 0x163);
	//isProcessed
	extern RelocPtr<PlayerCamera*>		g_playerCamera;


	// E8 
	class InputDeviceManager : public EventDispatcher<InputEvent, InputEvent*>
	{
	public:
		UInt32			unk058;			// 058
		UInt32			pad05C;			// 05C
		BSInputDevice	* keyboard;		// 060 
		BSInputDevice	* mouse;		// 068
		BSInputDevice	* gamepad;		// 070
		BSInputDevice	* vkeyboard;	// 078	- New in SE  .?AVBSWin32VirtualKeyboardDevice@@
		UInt8			unk080;			// 080
		UInt8			unk081;			// 081
		UInt8			pad082[6];		// 082
		BSTEventSource<void *>	unk088;	// 088	- TODO: template type
		UInt8			unk0E0;			// 0E0
		UInt8			pad0E1[7];		// 0E1

		inline bool IsGamepadEnabled(void)
		{
			return (gamepad != NULL) && gamepad->IsEnabled();
		}
	};
	STATIC_ASSERT(offsetof(InputDeviceManager, gamepad) == 0x70);
	STATIC_ASSERT(sizeof(InputDeviceManager) == 0xE8);


	extern RelocPtr<InputDeviceManager*> g_inputDeviceMgr;



	struct ThreadEventHandleManager
	{
		struct HandleData {
			HANDLE	* data;
			UInt32	count;
		};

		UInt64					unk00;	// 00
		HandleData				** data;	// 08
		UInt32					count;	// 10
	};


	extern RelocPtr<ThreadEventHandleManager*> g_threadEventHandleMgr;


	class MenuHandler
	{
	public:
		DEF_MEMBER_FN(Update, void, 0x8DAA70);
	};
	/*
	int __fastcall sub_1026F0(__int64 a1)
{
  __int64 v1; // rbx@1

  v1 = a1;
  if ( dword_1EE3688 != 2 )
    sub_C02210(&g_mainHeap_1EE3200, &dword_1EE3688);
  return sub_C020A0(&g_mainHeap_1EE3200, v1, 0i64);
}
	*/
}