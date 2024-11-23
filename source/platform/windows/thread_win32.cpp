#include "pch.h"
#include "windows_common.h"

namespace thread
{

	//static DWORD WINAPI thread_proc_win32(void* params)
	//{
	//	// TODO: Do something more useful
	//	return 0;
	//}

	//thread_t create()
	//{
	//	thread_t thread = {};
	//	thread.ptr = CreateThread(NULL, 0, thread_proc_win32, nullptr, 0, nullptr);

	//	return thread;
	//}

	bool wait_on_address(volatile void* address, void* compare_address, size_t address_size)
	{
		return WaitOnAddress(address, compare_address, address_size, INFINITE);
	}

	void wake_on_address(void* address)
	{
		WakeByAddressSingle(address);
	}

	void wake_on_address_all(void* address)
	{
		WakeByAddressAll(address);
	}

	namespace mutex
	{

		void lock(mutex_t& mutex)
		{
			SRWLOCK* mutex_win32 = (SRWLOCK*)&mutex;
			AcquireSRWLockExclusive(mutex_win32);
		}

		bool try_lock(mutex_t& mutex)
		{
			SRWLOCK* mutex_win32 = (SRWLOCK*)&mutex;
			return TryAcquireSRWLockExclusive(mutex_win32);
		}

		void unlock(mutex_t& mutex)
		{
			SRWLOCK* mutex_win32 = (SRWLOCK*)&mutex;
			ReleaseSRWLockExclusive(mutex_win32);
		}

	}

	namespace cond_var
	{

		void sleep(cond_var_t& cond_var, mutex_t& mutex)
		{
			CONDITION_VARIABLE* cond_var_win32 = (CONDITION_VARIABLE*)&cond_var;
			SRWLOCK* mutex_win32 = (SRWLOCK*)&mutex;
			SleepConditionVariableSRW(cond_var_win32, mutex_win32, INFINITE, 0);
		}

		void wake_one(cond_var_t& cond_var, mutex_t& mutex)
		{
			CONDITION_VARIABLE* cond_var_win32 = (CONDITION_VARIABLE*)&cond_var;
			WakeConditionVariable(cond_var_win32);
		}

		void wake_all(cond_var_t& cond_var, mutex_t& mutex)
		{
			CONDITION_VARIABLE* cond_var_win32 = (CONDITION_VARIABLE*)&cond_var;
			WakeAllConditionVariable(cond_var_win32);
		}

	}

}
