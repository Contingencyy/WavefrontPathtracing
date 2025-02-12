#pragma once
#include "core/hash.h"

template<typename TKey, typename TValue>
class hashmap
{
public:
	static constexpr uint64_t DEFAULT_CAPACITY = 1000;
	static constexpr TKey NODE_UNUSED = 0;

public:
	void init(memory_arena_t* arena, uint64_t capacity = DEFAULT_CAPACITY)
	{
		m_capacity = capacity;
		m_nodes = ARENA_ALLOC_ARRAY_ZERO(arena, node_t, m_capacity);

		for (uint32_t i = 0; i < m_capacity; ++i)
		{
			m_nodes[i].key = NODE_UNUSED;
		}
	}

	hashmap(const hashmap& other) = delete;
	hashmap(hashmap&& other) = delete;
	const hashmap& operator=(const hashmap& other) = delete;
	hashmap&& operator=(hashmap&& other) = delete;

	TValue* insert(TKey key, TValue value)
	{
		ASSERT_MSG(m_size < m_capacity, "Failed to insert pair into hashmap because the hashmap is full");

		// Create a temporary node with our new values
		node_t temp_node = {};
		temp_node.key = key;
		temp_node.value = value;

		// hash the key to find the corresponding index
		uint32_t node_index = hash_node_index(key);

		// If the current node is already occupied, move on to the next one
		// TODO: Implement buckets instead of moving on to the next slot
		while (m_nodes[node_index].key != key &&
			m_nodes[node_index].key != NODE_UNUSED)
		{
			node_index++;
			node_index %= m_capacity;
		}

		m_nodes[node_index] = temp_node;
		m_size++;

		return &m_nodes[node_index].value;
	}

	void remove(TKey key)
	{
		uint32_t node_index = hash_node_index(key);
		uint32_t counter = 0;

		// We need to loop here, if the object's hashed index was already occupied it was put further back
		while (m_nodes[node_index].key != NODE_UNUSED || counter <= m_capacity)
		{
			if (m_nodes[node_index].key == key)
			{
				node_t* node = &m_nodes[node_index];

				if constexpr (!std::is_trivially_destructible_v<TValue>)
				{
					node->value.~TValue();
				}

				node->key = NODE_UNUSED;
				node->value = {};

				m_size--;
				break;
			}

			counter++;
			node_index++;
			node_index %= m_capacity;
		}
	}

	TValue* find(TKey key)
	{
		TValue* result = nullptr;
		uint32_t node_index = hash_node_index(key);
		uint32_t counter = 0;

		while (m_nodes[node_index].key != NODE_UNUSED || counter <= m_capacity)
		{
			if (m_nodes[node_index].key == key)
			{
				result = &m_nodes[node_index].value;
				break;
			}

			counter++;
			node_index++;
			node_index %= m_capacity;
		}

		return result;
	}

	void reset()
	{
		for (uint32_t i = 0; i < m_capacity; ++i)
		{
			if (m_nodes[i].key != NODE_UNUSED)
			{
				if constexpr (!std::is_trivially_destructible_v<TValue>)
				{
					m_nodes[i].value.~TValue();
				}

				m_nodes[i].key = NODE_UNUSED;
			}
		}

		m_size = 0;
	}

private:
	uint32_t hash_node_index(TKey key)
	{
		return hash::djb2(&key) % m_capacity;
	}

private:
	struct node_t
	{
		TKey key;
		TValue value;
	};

	node_t* m_nodes;

	uint64_t m_size;
	uint64_t m_capacity;

};
