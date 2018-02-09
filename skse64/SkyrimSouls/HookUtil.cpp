#include "HookUtil.h"

namespace HookUtil
{

	template<class Ty>
	static inline Ty SafeWrite_Impl(uintptr_t addr, Ty data)
	{
		DWORD	oldProtect = 0;
		Ty		oldVal = 0;

		if (VirtualProtect((void *)addr, sizeof(Ty), PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			Ty *p = (Ty*)addr;
			oldVal = *p;
			*p = data;
			VirtualProtect((void *)addr, sizeof(Ty), oldProtect, &oldProtect);
		}

		return oldVal;
	}

	uintptr_t SafeWrite64(uintptr_t addr, uintptr_t data)
	{
		return SafeWrite_Impl(addr, data);
	}
}