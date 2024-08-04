#pragma once
#include "Core/Hash.h"

template<typename TKey, typename TValue>
class Hashmap
{
public:
	static constexpr u64 DEFAULT_CAPACITY = 1000;
	static constexpr TKey NODE_UNUSED = 0;

public:
	void Init(MemoryArena* Arena, u64 Capacity = DEFAULT_CAPACITY)
	{
		m_Capacity = Capacity;
		m_Nodes = ARENA_ALLOC_ARRAY_ZERO(Arena, Node, m_Capacity);

		for (u32 i = 0; i < m_Capacity; ++i)
		{
			m_Nodes[i].Key = NODE_UNUSED;
		}
	}

	Hashmap(const Hashmap& other) = delete;
	Hashmap(Hashmap&& other) = delete;
	const Hashmap& operator=(const Hashmap& other) = delete;
	Hashmap&& operator=(Hashmap&& other) = delete;

	TValue* Insert(TKey Key, TValue Value)
	{
		ASSERT(m_Size < m_Capacity);

		// Create a temporary node with our new values
		Node Temp = {};
		Temp.Key = Key;
		Temp.Value = Value;

		// Hash the key to find the corresponding index
		u32 NodeIndex = HashNodeIndex(Key);

		// If the current node is already occupied, move on to the next one
		// TODO: Implement buckets instead of moving on to the next slot
		while (m_Nodes[NodeIndex].Key != Key &&
			m_Nodes[NodeIndex].Key != NODE_UNUSED)
		{
			NodeIndex++;
			NodeIndex %= m_Capacity;
		}

		m_Nodes[NodeIndex] = Temp;
		m_Size++;

		return &m_Nodes[NodeIndex].Value;
	}

	void Remove(TKey Key)
	{
		u32 NodeIndex = HashNodeIndex(Key);
		u32 Counter = 0;

		// We need to loop here, if the object's hashed index was already occupied it was put further back
		while (m_Nodes[NodeIndex].Key != NODE_UNUSED || Counter <= m_Capacity)
		{
			if (m_Nodes[NodeIndex].Key == Key)
			{
				Node* Node = &m_Nodes[NodeIndex];

				if constexpr (!std::is_trivially_destructible_v<TValue>)
				{
					Node->Value.~TValue();
				}

				Node->Key = NODE_UNUSED;
				Node->Value = {};

				m_Size--;
				break;
			}

			Counter++;
			NodeIndex++;
			NodeIndex %= m_Capacity;
		}
	}

	TValue* Find(TKey Key)
	{
		TValue* Result = nullptr;
		u32 NodeIndex = HashNodeIndex(Key);
		u32 Counter = 0;

		while (m_Nodes[NodeIndex].Key != NODE_UNUSED || Counter <= m_Capacity)
		{
			if (m_Nodes[NodeIndex].Key == Key)
			{
				Result = &m_Nodes[NodeIndex].Value;
				break;
			}

			Counter++;
			NodeIndex++;
			NodeIndex %= m_Capacity;
		}

		return Result;
	}

	void Reset()
	{
		for (u32 i = 0; i < m_Capacity; ++i)
		{
			if (m_Nodes[i].Key != NODE_UNUSED)
			{
				if constexpr (!std::is_trivially_destructible_v<TValue>)
				{
					m_Nodes[i].Value.~TValue();
				}

				m_Nodes[i].Key = NODE_UNUSED;
			}
		}

		m_Size = 0;
	}

private:
	u32 HashNodeIndex(TKey Key)
	{
		return Hash::DJB2(&Key) % m_Capacity;
	}

private:
	struct Node
	{
		TKey Key;
		TValue Value;
	};

	Node* m_Nodes;

	u64 m_Size;
	u64 m_Capacity;

};
