#pragma once

namespace HookUtil
{
	uintptr_t SafeWrite64(uintptr_t addr, uintptr_t data);

	template <class Ty, class TRet, class... TArg>
	inline auto SafeWrite64(uintptr_t jumpSrc, TRet(Ty::*fn)(TArg...)) -> decltype(fn)
	{
		typedef decltype(fn) Fn;
		union
		{
			uintptr_t	u64;
			Fn			fn;
		} data;

		data.fn = fn;

		data.u64 = SafeWrite64(jumpSrc, data.u64);
		return data.fn;
	}

	template <class TRet, class... TArg>
	inline auto SafeWrite64(uintptr_t jumpSrc, TRet(*fn)(TArg...)) -> decltype(fn)
	{
		typedef decltype(fn) Fn;
		return (Fn)SafeWrite64(jumpSrc, (uintptr_t)fn);
	}
}