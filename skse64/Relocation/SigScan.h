#pragma once
#include <unordered_map>
#include <memory>
#include <string>
#include <sstream>
#include <exception> 
#include <functional>

#include "skse64_common/skse_version.h"
#include "Relocation/Pattern.h"
#include "Relocation/Relocation.h"


struct sig_plugin_info
{
	uint32_t		runtime_version = CURRENT_RELEASE_RUNTIME;
	const char		* plugin_name = nullptr;
};

///Fill this struct in F4SEPlugin_Query
extern sig_plugin_info	plugin_info; 

///To use RELOC_MEMBER_FN macro, first of all, you need to replace DEFINE_MEMBER_FN_LONG macro in skse64_common/Utilities.h with the following codes.
/*
#define DEFINE_MEMBER_FN_LONG(className, functionName, retnType, address, ...)		\
typedef retnType (className::* _##functionName##_type)(__VA_ARGS__);			\
\
inline _##functionName##_type * _##functionName##_GetPtr(void)					\
{																				\
static uintptr_t _address = address + RelocationManager::s_baseAddr;		\
return (_##functionName##_type *)&_address;									\
}
*/

#define RELOC_MEMBER_FN(className, fnName, ...)																						\
	decltype(&className::_##fnName##_GetPtr)   _className##_##fnName = &className::_##fnName##_GetPtr;								\
	*((*((uintptr_t *(**)())(&_className##_##fnName)))()) = sig_scan_processor(#className"::"#fnName, __VA_ARGS__).GetUIntPtr();


#define RELOC_MEMBER_FN_EX(className, fnName, ...)																					\
	className##::_##fnName##_GetFnPtr() = sig_scan_processor(#className"::"#fnName, __VA_ARGS__).GetUIntPtr(); 


#define RELOC_GLOBAL_VAL(globalObj, ...)																							\
	globalObj = sig_scan_processor(#globalObj, __VA_ARGS__).GetOffset(); 


#define RELOC_RUNTIME_ADDR(...)																										\
	sig_scan_processor(nullptr, __VA_ARGS__).GetUIntPtr()



class no_result_exception : public std::exception
{
public:
	virtual char const * what() const override;
};



struct sig_scan_util
{
	static bool read_memory(uintptr_t addr, void* data, size_t len)
	{
		UInt32 oldProtect;
		if (VirtualProtect((void *)addr, len, PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			memcpy(data, (void*)addr, len);
			if (VirtualProtect((void *)addr, len, oldProtect, &oldProtect))
				return true;
		}
		return false;
	}
};



class sig_scan_timer
{
public:
	sig_scan_timer()
	{
		QueryPerformanceCounter(&countStart);
		QueryPerformanceFrequency(&frequency);
	}
	~sig_scan_timer()
	{
		QueryPerformanceCounter(&countEnd);
		_MESSAGE(">> sigscan time elapsed: %llu ms...", (countEnd.QuadPart - countStart.QuadPart) / (frequency.QuadPart / 1000));
	}

private:
	LARGE_INTEGER countStart, countEnd, frequency;
};



class sig_scan_processor
{
public:
	using version = uint32_t;
	using relative_address = uint64_t;
	using runtime_address = uint64_t;
	using version_map = std::unordered_map<version, relative_address>;

	sig_scan_processor() = delete;

	sig_scan_processor(const sig_scan_processor& rhs) = delete;

	sig_scan_processor(sig_scan_processor&& rhs) = delete;

	sig_scan_processor(const char * iniName, version_map versionData, const char * sig = nullptr, int relOffset = 0, int dataOffset = 0, int instructionLen = 0) : m_result(0)
	{
		std::unique_ptr<char[]> sResult(new char[MAX_PATH]);
		static std::string fileName = ".\\Data\\SKSE\\Plugins\\" + std::string(plugin_info.plugin_name) + ".mem";
		static std::string sectionName = (sprintf_s(sResult.get(), MAX_PATH, "%08X", plugin_info.runtime_version), sResult.get());
		std::string		settingName;
		if (iniName)
		{
			settingName = iniName;
		}
		else if (sig)
		{
			std::hash<std::string> hash_fn;
			size_t hash = hash_fn(sig + std::string("|") + std::to_string(relOffset << 0x10 | (dataOffset & 0xFF) << 0x8 | instructionLen & 0xFF));
			settingName = std::string("SIG_") + (sprintf_s(sResult.get(), MAX_PATH, "%016I64X", hash), sResult.get()); //only sig...
		}
		if (versionData.count(plugin_info.runtime_version))
		{
			m_result = versionData[plugin_info.runtime_version] + RelocationManager::s_baseAddr;
		}
		else /*if (iniName && plugin_info.plugin_name)*/
		{
			uint32_t relMem = 0;
			if (plugin_info.plugin_name && (relMem = GetPrivateProfileIntA(sectionName.c_str(), settingName.c_str(), 0, fileName.c_str()), relMem))
			{
				m_result = relMem + RelocationManager::s_baseAddr;
			}
			else if (sig != nullptr)
			{
				uintptr_t scan_result = (uintptr_t)Utility::pattern(sig).count(1).get(0).get<void>(0);
				if (!scan_result)
				{
					throw no_result_exception();
				}
				scan_result += relOffset;
				if (dataOffset)
				{
					int32_t rel32 = 0;
					sig_scan_util::read_memory(scan_result + dataOffset, &rel32, sizeof(int32_t));
					scan_result += instructionLen + rel32;
				}
				m_result = scan_result;
			}
		}
		if (!m_result)
		{
			throw no_result_exception();
		}
		if (plugin_info.plugin_name && settingName.size())
		{
			sprintf_s(sResult.get(), MAX_PATH, "0x%08X", GetOffset());
			WritePrivateProfileStringA(sectionName.c_str(), settingName.c_str(), sResult.get(), fileName.c_str());
		}
	}

	sig_scan_processor(const char * iniName, const char * sig = nullptr, int relOffset = 0, int dataOffset = 0, int instructionLen = 0) \
		: sig_scan_processor(iniName, { {} }, sig, relOffset, dataOffset, instructionLen) {  }

	sig_scan_processor(version_map versionData, const char * sig = nullptr, int relOffset = 0, int dataOffset = 0, int instructionLen = 0) \
		: sig_scan_processor(nullptr, versionData, sig, relOffset, dataOffset, instructionLen) {  }

	sig_scan_processor(const char * sig = nullptr, int relOffset = 0, int dataOffset = 0, int instructionLen = 0) \
		: sig_scan_processor(nullptr, { {} }, sig, relOffset, dataOffset, instructionLen) {  }

	sig_scan_processor(const char * iniName, relative_address relMem, const char * sig = nullptr, int relOffset = 0, int dataOffset = 0, int instructionLen = 0) \
		: sig_scan_processor(iniName, { { CURRENT_RELEASE_RUNTIME, relMem } }, sig, relOffset, dataOffset, instructionLen) {  }

	sig_scan_processor(relative_address relMem, const char * sig = nullptr, int relOffset = 0, int dataOffset = 0, int instructionLen = 0) \
		: sig_scan_processor(nullptr, { { CURRENT_RELEASE_RUNTIME, relMem } }, sig, relOffset, dataOffset, instructionLen) {  }

	uintptr_t GetUIntPtr() const
	{
		return m_result;
	}

	uint32_t GetOffset() const
	{
		return (m_result) ? static_cast<uint32_t>(m_result - RelocationManager::s_baseAddr) : 0;
	}
private:
	uintptr_t									    m_result;
};
