#include "Pch.h"
#include "Threadpool.h"

void Threadpool::Init(MemoryArena* Arena)
{
	m_JobsCompleted.store(0);

	u32 CoreCount = std::thread::hardware_concurrency();

	m_ThreadCount = std::max(1u, CoreCount);
	m_Threads = ARENA_ALLOC_ARRAY(Arena, std::thread, m_ThreadCount);

	for (u32 i = 0; i < m_ThreadCount; ++i)
	{
		m_Threads[i] = std::thread([&] {
			std::function<void()> jobFunc;

			while (!m_Exit)
			{
				if (m_JobRingBuffer.PopFront(jobFunc))
				{
					jobFunc();
					m_JobsCompleted.fetch_add(1);
				}
				else
				{
					std::unique_lock<std::mutex> lock(m_WakeMutex);
					m_WakeCondition.wait(lock);
				}
			}
			});

		m_Threads[i].detach();
	}
}

void Threadpool::Destroy()
{
	m_Exit = true;

	for (u32 i = 0; i < m_ThreadCount; ++i)
	{
		if (m_Threads[i].joinable())
			m_Threads[i].join();
	}
}

void Threadpool::QueueJob(const std::function<void()>& JobFunc)
{
	m_JobsQueued++;

	while (!m_JobRingBuffer.PushBack(JobFunc))
		Poll();

	m_WakeCondition.notify_one();
}

void Threadpool::Dispatch(u32 JobCount, u32 GroupSize, const std::function<void(JobDispatchArgs)>& JobFunc)
{
	if (JobCount == 0 || GroupSize == 0)
		return;

	const u32 GroupCount = (JobCount + GroupSize - 1) / GroupSize;
	m_JobsQueued += GroupCount;

	for (u32 GroupIdx = 0; GroupIdx < GroupCount; ++GroupIdx)
	{
		const auto& JobGroup = [JobCount, GroupSize, JobFunc, GroupIdx]() {
			const u32 JobGroupOffset = GroupIdx * GroupSize;
			const u32 JobGroupEnd = std::min(JobGroupOffset + GroupSize, JobCount);

			JobDispatchArgs dispatchArgs = {};
			dispatchArgs.GroupIndex = GroupIdx;

			for (u32 jobIndex = JobGroupOffset; jobIndex < JobGroupEnd; ++jobIndex)
			{
				dispatchArgs.JobIndex = jobIndex;
				JobFunc(dispatchArgs);
			}
		};

		while (!m_JobRingBuffer.PushBack(JobGroup))
			Poll();

		m_WakeCondition.notify_one();
	}
}

b8 Threadpool::IsBusy()
{
	return m_JobsCompleted.load() < m_JobsQueued;
}

void Threadpool::WaitAll()
{
	while (IsBusy())
		Poll();
}

void Threadpool::Poll()
{
	m_WakeCondition.notify_one();
	std::this_thread::yield();
}
