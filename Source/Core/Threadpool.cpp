#include "pch.h"
#include "threadpool.h"

void threadpool_t::init(memory_arena_t* arena)
{
	m_jobs_completed.store(0);

	u32 physical_core_count = std::thread::hardware_concurrency();

	m_thread_count = std::max(1u, physical_core_count);
	m_threads = ARENA_ALLOC_ARRAY(arena, std::thread, m_thread_count);

	for (u32 i = 0; i < m_thread_count; ++i)
	{
		m_threads[i] = std::thread([&] {
			std::function<void()> job_func;

			while (!m_exit_requested)
			{
				if (m_job_ringbuffer.pop_front(job_func))
				{
					job_func();
					m_jobs_completed.fetch_add(1);
				}
				else
				{
					std::unique_lock<std::mutex> lock(m_wake_mutex);
					m_wake_cond.wait(lock);
				}
			}
			});

		m_threads[i].detach();
	}
}

void threadpool_t::destroy()
{
	m_exit_requested = true;

	for (u32 i = 0; i < m_thread_count; ++i)
	{
		if (m_threads[i].joinable())
			m_threads[i].join();
	}
}

void threadpool_t::queue_job(const std::function<void()>& job_func)
{
	m_jobs_queued++;

	while (!m_job_ringbuffer.push_back(job_func))
		poll();

	m_wake_cond.notify_one();
}

void threadpool_t::dispatch(u32 job_count, u32 group_size, const std::function<void(job_dispatch_args_t)>& job_func)
{
	if (job_count == 0 || group_size == 0)
		return;

	const u32 group_count = (job_count + group_size - 1) / group_size;
	m_jobs_queued += group_count;

	for (u32 group_idx = 0; group_idx < group_count; ++group_idx)
	{
		const auto& job_group = [job_count, group_size, job_func, group_idx]() {
			const u32 job_group_offset = group_idx * group_size;
			const u32 job_group_end = std::min(job_group_offset + group_size, job_count);

			job_dispatch_args_t dispatch_args = {};
			dispatch_args.group_index = group_idx;

			for (u32 job_idx = job_group_offset; job_idx < job_group_end; ++job_idx)
			{
				dispatch_args.job_index = job_idx;
				job_func(dispatch_args);
			}
		};

		while (!m_job_ringbuffer.push_back(job_group))
			poll();

		m_wake_cond.notify_one();
	}
}

b8 threadpool_t::is_busy()
{
	return m_jobs_completed.load() < m_jobs_queued;
}

void threadpool_t::wait_all()
{
	while (is_busy())
		poll();
}

void threadpool_t::poll()
{
	m_wake_cond.notify_one();
	std::this_thread::yield();
}
