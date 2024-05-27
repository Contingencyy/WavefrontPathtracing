#include "Pch.h"
#include "Threadpool.h"

Threadpool::Threadpool()
{
	m_JobsCompleted.store(0);

	uint32_t numCores = std::thread::hardware_concurrency();
	uint32_t numThreads = std::max(1u, numCores);

	m_Threads.reserve(numThreads);

	for (size_t i = 0; i < numThreads; ++i)
	{
		m_Threads.push_back(std::thread([&] {
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
			}));

		m_Threads[i].detach();
	}
}

Threadpool::~Threadpool()
{
	m_Exit = true;

	for (auto& thread : m_Threads)
	{
		if (thread.joinable())
			thread.join();
	}
}

void Threadpool::QueueJob(const std::function<void()>& jobFunc)
{
	m_JobsQueued++;

	while (!m_JobRingBuffer.PushBack(jobFunc))
		Poll();

	m_WakeCondition.notify_one();
}

void Threadpool::Dispatch(uint32_t numJobs, uint32_t groupSize, const std::function<void(JobDispatchArgs)>& jobFunc)
{
	if (numJobs == 0 || groupSize == 0)
		return;

	const uint32_t numGroups = (numJobs + groupSize - 1) / groupSize;
	m_JobsQueued += numGroups;

	for (size_t groupIndex = 0; groupIndex < numGroups; ++groupIndex)
	{
		const auto& jobGroup = [numJobs, groupSize, jobFunc, groupIndex]() {
			const uint32_t jobGroupOffset = groupIndex * groupSize;
			const uint32_t jobGroupEnd = std::min(jobGroupOffset + groupSize, numJobs);

			JobDispatchArgs dispatchArgs = {};
			dispatchArgs.groupIndex = groupIndex;

			for (size_t jobIndex = jobGroupOffset; jobIndex < jobGroupEnd; ++jobIndex)
			{
				dispatchArgs.jobIndex = jobIndex;
				jobFunc(dispatchArgs);
			}
		};

		while (!m_JobRingBuffer.PushBack(jobGroup))
			Poll();

		m_WakeCondition.notify_one();
	}
}

bool Threadpool::IsBusy()
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
