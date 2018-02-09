#pragma once

#include "skse64_common/Relocation.h"

// this has been tested to work for non-varargs functions
// varargs functions end up with 'this' passed as the last parameter (ie. probably broken)
// do NOT use with classes that have multiple inheritance

// if many member functions are to be declared, use MEMBER_FN_PREFIX to create a type with a known name
// so it doesn't need to be restated throughout the member list

// all of the weirdness with the _GetType function is because you can't declare a static const pointer
// inside the class definition. we sadly can't inline anymore because of relocation.

// RelocPtr only works at a global scope, which we can't handle or we'd be bypassing the function route altogether

#define MEMBER_FN_PREFIX(className)	\
	typedef className _MEMBER_FN_BASE_TYPE

#define DEFINE_MEMBER_FN_LONG(className, functionName, retnType, address, ...)		\
	typedef retnType (className::* _##functionName##_type)(__VA_ARGS__);			\
																					\
	inline _##functionName##_type * _##functionName##_GetPtr(void)					\
	{																				\
		static uintptr_t _address = address + RelocationManager::s_baseAddr;		\
/*_MESSAGE("%s::%s|%016I64X", typeid(className).name(), #functionName, _address - RelocationManager::s_baseAddr);*/\
		return (_##functionName##_type *)&_address;									\
	}

#define DEFINE_MEMBER_FN(functionName, retnType, address, ...)	\
	DEFINE_MEMBER_FN_LONG(_MEMBER_FN_BASE_TYPE, functionName, retnType, address, __VA_ARGS__)

#define DEFINE_STATIC_HEAP(staticAllocate, staticFree)						\
	static void * operator new(std::size_t size)							\
	{																		\
		return staticAllocate(size);										\
	}																		\
	static void * operator new(std::size_t size, const std::nothrow_t &)	\
	{																		\
		return staticAllocate(size);										\
	}																		\
	static void * operator new(std::size_t size, void * ptr)				\
	{																		\
		return ptr;															\
	}																		\
	static void operator delete(void * ptr)									\
	{																		\
		staticFree(ptr);													\
	}																		\
	static void operator delete(void * ptr, const std::nothrow_t &)			\
	{																		\
		staticFree(ptr);													\
	}																		\
	static void operator delete(void *, void *)								\
	{																		\
	}

#define CALL_MEMBER_FN(obj, fn)	\
	((*(obj)).*(*((obj)->_##fn##_GetPtr())))


#define FORCE_INLINE  __forceinline
#define DEF_MEMBER_FN(fnName, retnType, addr, ...)								\
	template <class... Params>													\
	FORCE_INLINE retnType fnName(Params&&... params) {							\
		struct empty_struct {};													\
		typedef retnType(empty_struct::*_##fnName##_type)(__VA_ARGS__);			\
		const static uintptr_t address = _##fnName##_GetFnPtr();				\
		_##fnName##_type fn = *(_##fnName##_type*)&address;						\
/*_MESSAGE("%s::%s|%016I64X", typeid(*this).name(), #fnName, address - RelocationManager::s_baseAddr);*/\
		return (reinterpret_cast<empty_struct*>(this)->*fn)(params...);			\
	}																			\
	static uintptr_t & _##fnName##_GetFnPtr()	{								\
		static uintptr_t relMem = addr + RelocationManager::s_baseAddr;			\
		return relMem;															\
	}


// this is the solution to getting a pointer-to-member-function pointer
template <typename T>
uintptr_t GetFnAddr(T src)
{
	union
	{
		uintptr_t	u;
		T			t;
	} data;

	data.t = src;

	return data.u;
}

std::string GetRuntimePath();
std::string GetRuntimeName();
const std::string & GetRuntimeDirectory();

const std::string & GetConfigPath();
std::string GetConfigOption(const char * section, const char * key);
bool GetConfigOption_UInt32(const char * section, const char * key, UInt32 * dataOut);

const std::string & GetOSInfoStr();

void * GetIATAddr(void * module, const char * searchDllName, const char * searchImportName);

const char * GetObjectClassName(void * objBase);
void DumpClass(void * theClassPtr, UInt64 nIntsToDump);
