#pragma once
#include "core/api_types.h"

namespace thread
{

	//thread_t create();

	bool wait_on_address(volatile void* address, void* compare_address, size_t address_size);
	void wake_on_address(void* address);
	void wake_on_address_all(void* address);

	namespace mutex
	{

		void lock(mutex_t& mutex);
		bool try_lock(mutex_t& mutex);
		void unlock(mutex_t& mutex);

	}

	namespace cond_var
	{

		void sleep(cond_var_t& cond_var, mutex_t& mutex);
		void wake_one(cond_var_t& cond_var, mutex_t& mutex);
		void wake_all(cond_var_t& cond_var, mutex_t& mutex);

	}

}
