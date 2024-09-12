
#include <time.h>
#include "common/common.h"
#include "sys_os_define.h"




namespace Atomic
{
	int32_t Inc32(int32_t* i)
	{
		return (int32_t)InterlockedIncrement((volatile LONG*)i);
	}

	int32_t Dec32(int32_t* i)
	{
		return (int32_t)InterlockedDecrement((volatile LONG*)i);
	}

	int64_t Inc64(int64_t* i)
	{
		return (int64_t)InterlockedIncrement64((volatile LONG64*)i);
	}

	int64_t Dec64(int64_t* i)
	{
		return (int64_t)InterlockedDecrement64((volatile LONG64*)i);
	}

	int64_t ExchAdd64(int64_t* i, int64_t a)
	{
		return (int64_t)InterlockedExchangeAdd64((volatile LONG64*)i, a);
	}

	int32_t CmpExch32(int32_t* dest, int32_t oldVal, int32_t newVal)
	{
		return (int32_t)InterlockedCompareExchange((volatile LONG*)dest, newVal, oldVal);
	}
};


namespace Threading
{

	template <>
	CriticalSection::CriticalSectionTemplate()
	{
		InitializeCriticalSection(&m_Data);
	}

	template <>
	CriticalSection::~CriticalSectionTemplate()
	{
		DeleteCriticalSection(&m_Data);
	}

	template <>
	void CriticalSection::Lock()
	{
		EnterCriticalSection(&m_Data);
	}

	template <>
	bool CriticalSection::Trylock()
	{
		return TryEnterCriticalSection(&m_Data) != FALSE;
	}

	template <>
	void CriticalSection::Unlock()
	{
		LeaveCriticalSection(&m_Data);
	}

	template <>
	RWLock::RWLockTemplate()
	{
		InitializeSRWLock(&m_Data);
	}

	template <>
	RWLock::~RWLockTemplate()
	{
	}

	template <>
	void RWLock::WriteLock()
	{
		AcquireSRWLockExclusive(&m_Data);
	}

	template <>
	bool RWLock::TryWritelock()
	{
		return TryAcquireSRWLockExclusive(&m_Data) != FALSE;
	}

	template <>
	void RWLock::WriteUnlock()
	{
		ReleaseSRWLockExclusive(&m_Data);
	}

	template <>
	void RWLock::ReadLock()
	{
		AcquireSRWLockShared(&m_Data);
	}

	template <>
	bool RWLock::TryReadlock()
	{
		return TryAcquireSRWLockShared(&m_Data) != FALSE;
	}

	template <>
	void RWLock::ReadUnlock()
	{
		ReleaseSRWLockShared(&m_Data);
	}

	struct ThreadInitData
	{
		std::function<void()> entryFunc;
	};

	static DWORD __stdcall sThreadInit(void* init)
	{
		ThreadInitData* data = (ThreadInitData*)init;

		// delete before going into entry function so lifetime is limited.
		ThreadInitData local = *data;
		delete data;

		local.entryFunc();

		return 0;
	}

}