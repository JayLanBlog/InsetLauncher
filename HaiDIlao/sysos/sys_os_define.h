 #pragma once
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <functional>
#include "common/common.h"
namespace Proc{


	void LaunchDefaultAndInject();

	int InjectIntoProcess(uint32_t pid, bool wait);

	int LaunchAndInjectIntoProcess(std::string app, std::string workdir);

	void* GetFunctionAddress(void* module, const std::string& function);

}



namespace Atomic
{
	int32_t Inc32(int32_t* i);
	int32_t Dec32(int32_t* i);
	int64_t Inc64(int64_t* i);
	int64_t Dec64(int64_t* i);
	int64_t ExchAdd64(int64_t* i, int64_t a);
	int32_t CmpExch32(int32_t* dest, int32_t oldVal, int32_t newVal);
};


namespace Threading
{

	template <class data>
	class CriticalSectionTemplate
	{
	public:
		CriticalSectionTemplate();
		~CriticalSectionTemplate();
		void Lock();
		bool Trylock();
		void Unlock();

		// no copying
		CriticalSectionTemplate& operator=(const CriticalSectionTemplate& other) = delete;
		CriticalSectionTemplate(const CriticalSectionTemplate& other) = delete;

		data m_Data;
	};

	template <class data>
	class RWLockTemplate
	{
	public:
		RWLockTemplate();
		~RWLockTemplate();

		void ReadLock();
		bool TryReadlock();
		void ReadUnlock();

		void WriteLock();
		bool TryWritelock();
		void WriteUnlock();

		// no copying
		RWLockTemplate& operator=(const RWLockTemplate& other) = delete;
		RWLockTemplate(const RWLockTemplate& other) = delete;

		data m_Data;
	};
	typedef CriticalSectionTemplate<CRITICAL_SECTION> CriticalSection;
	typedef RWLockTemplate<SRWLOCK> RWLock;
}
