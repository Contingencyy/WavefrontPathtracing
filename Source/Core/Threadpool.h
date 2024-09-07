#pragma once

#include <thread>
#include <condition_variable>
#include <mutex>
#include <functional>
#include <atomic>

template<typename T>
class threadsafe_ring_buffer_t
{
public:
	static constexpr u32 THREAD_SAFE_RING_BUFFER_CAPACITY = 512;

public:
	b8 push_back(const T& job)
	{
		b8 result = false;
		m_lock.lock();
		u32 next = (m_head + 1) % THREAD_SAFE_RING_BUFFER_CAPACITY;

		if (next != m_tail)
		{
			m_jobpool[m_head] = job;
			m_head = next;
			result = true;
		}

		m_lock.unlock();
		return result;
	}

	b8 pop_front(T& out_job)
	{
		b8 result = false;
		m_lock.lock();

		if (m_tail != m_head)
		{
			out_job = m_jobpool[m_tail];
			m_tail = (m_tail + 1) % THREAD_SAFE_RING_BUFFER_CAPACITY;
			result = true;
		}

		m_lock.unlock();
		return result;
	}

private:
	T m_jobpool[THREAD_SAFE_RING_BUFFER_CAPACITY];
	u32 m_head = 0;
	u32 m_tail = 0;
	std::mutex m_lock;

};

class threadpool_t
{
public:
	struct job_dispatch_args_t
	{
		u32 job_index;
		u32 group_index;
	};

public:
	void init(memory_arena_t* arena);
	void destroy();

	void queue_job(const std::function<void()>& job_func);
	void dispatch(u32 job_count, u32 group_size, const std::function<void(job_dispatch_args_t)>& job_func);

	b8 is_busy();
	void wait_all();

private:
	void poll();

private:
	threadsafe_ring_buffer_t<std::function<void()>> m_job_ringbuffer;

	u32 m_thread_count;
	std::thread* m_threads;
	std::condition_variable m_wake_cond;
	std::mutex m_wake_mutex;

	u64 m_jobs_queued = 0;
	std::atomic<u64> m_jobs_completed;

	b8 m_exit_requested = false;

};
