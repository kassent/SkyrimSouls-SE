#include "Definition.h"

namespace SkyrimSouls
{
	RelocPtr<PlayerCharacter*>	g_thePlayer(0x02F4CE68);

	RelocPtr<MenuManager *>		g_menuManager(0x01EE4AA0);

	RelocPtr<UIMessageManager*>	g_uiMessageManager(0x01EE69F0);

	RelocPtr<UIStringHolder *>	g_uiStringHolder(0x01EE69F8);

	RelocPtr<SimpleLock>		globalMenuStackLock(0x30577B8);

	RelocPtr <Main *>			g_main(0x2F4CC20);

	RelocPtr<PlayerCamera*>		g_playerCamera(0x02EEB938);

	RelocPtr<InputDeviceManager*> g_inputDeviceMgr(0x02F4B728);

	RelocPtr<ThreadEventHandleManager*> g_threadEventHandleMgr(0x2F4C5F0);
}

namespace SkyrimSouls
{
	IMenu * MenuManager::GetMenu(BSFixedString * menuName)
	{
		if (!menuName->data)
			return NULL;
		SimpleLocker locker(&menuTableLock);
		MenuTableItem * item = menuTable.Find(menuName);

		if (!item)
			return NULL;

		IMenu * menu = item->menuInstance;
		if (!menu)
			return NULL;

		return menu;
	}

	bool	MenuManager::IsFlagEnabled(BSFixedString & menuName, UInt32 flag)
	{
		if (!menuName.data)
			return false;
		SimpleLocker locker(&menuTableLock);
		MenuTableItem * item = menuTable.Find(&menuName);
		if (!item)
			return false;

		IMenu * menu = item->menuInstance;
		if (!menu)
			return false;

		return (menu->flags & flag) != 0;
	}

	bool	MenuManager::IsSkyrimSoulsMenu(BSFixedString & menuName)
	{
		return IsFlagEnabled(menuName, IMenu::kFlag_SkyrimSoulsMenu);
	}
}




