#pragma once

#include <thread>
#include <condition_variable>
#include <mutex>
#include <functional>
#include <atomic>

template<typename T>
class ThreadSafeRingBuffer
{
public:
	static constexpr uint32_t THREAD_SAFE_RING_BUFFER_CAPACITY = 512;

public:
	bool PushBack(const T& job)
	{
		bool result = false;
		m_Lock.lock();
		size_t next = (m_Head + 1) % THREAD_SAFE_RING_BUFFER_CAPACITY;

		if (next != m_Tail)
		{
			m_JobPool[m_Head] = job;
			m_Head = next;
			result = true;
		}

		m_Lock.unlock();
		return result;
	}

	bool PopFront(T& job)
	{
		bool result = false;
		m_Lock.lock();

		if (m_Tail != m_Head)
		{
			job = m_JobPool[m_Tail];
			m_Tail = (m_Tail + 1) % THREAD_SAFE_RING_BUFFER_CAPACITY;
			result = true;
		}

		m_Lock.unlock();
		return result;
	}

private:
	std::array<T, THREAD_SAFE_RING_BUFFER_CAPACITY> m_JobPool;
	size_t m_Head = 0;
	size_t m_Tail = 0;
	std::mutex m_Lock;

};

class Threadpool
{
public:
	struct JobDispatchArgs
	{
		uint32_t jobIndex;
		uint32_t groupIndex;
	};

public:
	Threadpool();
	~Threadpool();

	void QueueJob(const std::function<void()>& jobFunc);
	void Dispatch(uint32_t numJobs, uint32_t groupSize, const std::function<void(JobDispatchArgs)>& jobFunc);

	bool IsBusy();
	void WaitAll();

private:
	void Poll();

private:
	ThreadSafeRingBuffer<std::function<void()>> m_JobRingBuffer;

	std::vector<std::thread> m_Threads;
	std::condition_variable m_WakeCondition;
	std::mutex m_WakeMutex;

	uint64_t m_JobsQueued = 0;
	std::atomic<uint64_t> m_JobsCompleted;

	bool m_Exit = false;

};
