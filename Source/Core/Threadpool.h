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
	static constexpr u32 THREAD_SAFE_RING_BUFFER_CAPACITY = 512;

public:
	b8 PushBack(const T& Job)
	{
		b8 bResult = false;
		m_Lock.lock();
		u32 Next = (m_Head + 1) % THREAD_SAFE_RING_BUFFER_CAPACITY;

		if (Next != m_Tail)
		{
			m_JobPool[m_Head] = Job;
			m_Head = Next;
			bResult = true;
		}

		m_Lock.unlock();
		return bResult;
	}

	b8 PopFront(T& Job)
	{
		b8 bResult = false;
		m_Lock.lock();

		if (m_Tail != m_Head)
		{
			Job = m_JobPool[m_Tail];
			m_Tail = (m_Tail + 1) % THREAD_SAFE_RING_BUFFER_CAPACITY;
			bResult = true;
		}

		m_Lock.unlock();
		return bResult;
	}

private:
	T m_JobPool[THREAD_SAFE_RING_BUFFER_CAPACITY];
	u32 m_Head = 0;
	u32 m_Tail = 0;
	std::mutex m_Lock;

};

class Threadpool
{
public:
	struct JobDispatchArgs
	{
		u32 JobIndex;
		u32 GroupIndex;
	};

public:
	void Init(MemoryArena* Arena);
	void Destroy();

	void QueueJob(const std::function<void()>& JobFunc);
	void Dispatch(u32 JobCount, u32 GroupSize, const std::function<void(JobDispatchArgs)>& JobFunc);

	b8 IsBusy();
	void WaitAll();

private:
	void Poll();

private:
	ThreadSafeRingBuffer<std::function<void()>> m_JobRingBuffer;

	std::thread* m_Threads;
	u32 m_ThreadCount;
	std::condition_variable m_WakeCondition;
	std::mutex m_WakeMutex;

	u64 m_JobsQueued = 0;
	std::atomic<u64> m_JobsCompleted;

	b8 m_Exit = false;

};
