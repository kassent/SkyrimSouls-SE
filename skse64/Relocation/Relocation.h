#pragma once
#include "common/IPrefix.h"

class RelocationManager
{
public:
	RelocationManager();

	static uintptr_t	s_baseAddr;
};

// use this for addresses that represent pointers to a type T
template <typename T>
class RelocPtr
{
public:
	RelocPtr(uintptr_t offset)
		:m_offset(offset + RelocationManager::s_baseAddr)
	{
		//
	}

	operator T *() const
	{
		//_MESSAGE(__FUNCTION__);
		return GetPtr();
	}

	T * operator->() const
	{
		//_MESSAGE(__FUNCTION__);
		return GetPtr();
	}

	T * GetPtr() const
	{
		return reinterpret_cast <T *>(m_offset);
	}

	const T * GetConst() const
	{
		return reinterpret_cast <T *>(m_offset);
	}

	uintptr_t GetUIntPtr() const
	{
		return m_offset;
	}

	RelocPtr & operator=(RelocPtr & rhs)
	{
		m_offset = rhs.m_offset;
	}

	RelocPtr & operator=(T * rhs)
	{
		m_offset = reinterpret_cast<uintptr_t>(rhs);
		//_MESSAGE("%s=%08X", __FUNCTION__, m_offset);
		return (*this);
	}

	RelocPtr & operator=(uintptr_t rhs)
	{
		m_offset = rhs + RelocationManager::s_baseAddr;
		//_MESSAGE("%s=%08X", __FUNCTION__, m_offset);
		return (*this);
	}
private:
	uintptr_t	m_offset;

	// hide
	RelocPtr();
	RelocPtr(RelocPtr & rhs);
};

// use this for direct addresses to types T. needed to avoid ambiguity
template <typename T>
class RelocAddr
{
public:
	RelocAddr(uintptr_t offset)
		:m_offset(reinterpret_cast <BlockConversionType *>(offset + RelocationManager::s_baseAddr))
	{
		//
	}

	operator T()
	{
		//_MESSAGE(__FUNCTION__);
		return reinterpret_cast <T>(m_offset);
	}

	uintptr_t GetUIntPtr() const
	{
		return reinterpret_cast <uintptr_t>(m_offset);
	}

	RelocAddr& operator=(T rhs)
	{
		m_offset = reinterpret_cast <BlockConversionType *>(reinterpret_cast<uintptr_t>(rhs) - RelocationManager::s_baseAddr);
		//_MESSAGE("%s=%08X", __FUNCTION__, m_offset);
		return (*this);
	}
private:
	// apparently you can't reinterpret_cast from a type to the same type
	// that's kind of stupid and makes it impossible to use this for uintptr_ts if I use the same type
	// so we make a new type by storing the data in a pointer to this useless struct
	// at least this is hidden by a wrapper
	struct BlockConversionType { };
	BlockConversionType * m_offset;

	// hide
	RelocAddr();
	RelocAddr(RelocAddr & rhs);
	//RelocAddr & operator=(RelocAddr & rhs);
};
