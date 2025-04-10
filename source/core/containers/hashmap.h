#pragma once
#include "core/hash.h"

template<typename key_type, typename value_type>
struct hashmap_node_t
{
	key_type key;
	value_type value;
	
	bool used;
};

// TODO: Custom Key copy function to avoid problems with e.g. strings as keys
// TODO: Custom hashing function
// TODO: Custom compare function
// TODO: Hashmap resizing
	// TODO: Hashmap resizing should resize to double capcity, then next greater prime number from that to reduce collisions that hash to powers of 2
// TODO: Hashmap iterate by making a linked list
// TODO: Hashmap iterate by passing in an array of keys to loop over, which is useful for the timestamps here
template<typename key_type, typename value_type>//, typename compare_func, typename hash_func>
struct hashmap_t
{
	static_assert(std::is_trivially_destructible_v<key_type>, "Hashmap implementation does not support non-trivially destructible key types");
	
	uint64_t size;
	uint64_t capacity;
	
	hashmap_node_t<key_type, value_type>* nodes;
	//key_type (*copy_key_func)(const key_type&);
};

template<typename key_type, typename value_type>
struct hashmap_iter_t
{
	const key_type* key;
	value_type* value;
	
	hashmap_t<key_type, value_type>* hashmap;
	uint32_t index;
};

namespace hashmap
{

	inline constexpr uint32_t HASHMAP_DEFAULT_CAPACITY = 256u;
	
	template<typename key_type, typename value_type>
	uint32_t hash_node_index(hashmap_t<key_type, value_type>& hashmap, key_type key)
	{
		return hash::djb2(&key) % hashmap.capacity;
	}

	template<typename key_type, typename value_type>
	hashmap_node_t<key_type, value_type>* find_next_free_node(hashmap_t<key_type, value_type>& hashmap, const key_type& key)
	{
		uint32_t node_index = hash_node_index<key_type, value_type>(hashmap, key);
		hashmap_node_t<key_type, value_type>* node = &hashmap.nodes[node_index];
		
		// If the current node is already occupied, move on to the next one
		// TODO: Implement buckets instead of moving on to the next slot
		while (node->used)
		{
			++node_index;
			node_index %= hashmap.capacity;
			node = &hashmap.nodes[node_index];
		}

		return node;
	}
	
	template<typename key_type, typename value_type>
	void init(hashmap_t<key_type, value_type>& hashmap, memory_arena_t& arena, uint64_t capacity = HASHMAP_DEFAULT_CAPACITY)
	{
		hashmap.capacity = capacity;
		hashmap.size = 0;
		hashmap.nodes = (hashmap_node_t<key_type, value_type>*)ARENA_ALLOC_ZERO(arena,
			sizeof(hashmap_node_t<key_type, value_type>) * hashmap.capacity,
			alignof(hashmap_node_t<key_type, value_type>));

		/*for (uint32_t i = 0; i < hashmap.capacity; ++i)
		{
			hashmap.nodes[i].used = false;
		}*/
	}
	
	template<typename key_type, typename value_type>
	void destroy(hashmap_t<key_type, value_type>& hashmap)
	{
		for (uint32_t i = 0; i < hashmap.capacity; ++i)
		{
			if constexpr (!std::is_trivially_destructible_v<value_type>)
			{
				hashmap.nodes[i].~value_type();
			}
			hashmap.nodes[i].used = false;
		}
		
		hashmap.capacity = 0;
		hashmap.size = 0;
		hashmap.nodes = nullptr;
	}

	template<typename key_type, typename value_type>
	value_type* insert(hashmap_t<key_type, value_type>& hashmap, const key_type& key, const value_type& value)
	{
		ASSERT_MSG(hashmap.size < hashmap.capacity, "Failed to insert into hashmap because the hashmap is full");
		hashmap_node_t<key_type, value_type>* node = find_next_free_node<key_type, value_type>(hashmap, key);

		node->key = key;
		node->value = value;
		node->used = true;
		
		++hashmap.size;
		return &node->value;
	}

	template<typename key_type, typename value_type, typename... arg_types>
	value_type* emplace(hashmap_t<key_type, value_type>& hashmap, const key_type& key, arg_types&&... args)
	{
		ASSERT_MSG(hashmap.size < hashmap.capacity, "Failed to insert into hashmap because the hashmap is full");
		hashmap_node_t<key_type, value_type>* node = find_next_free_node<key_type, value_type>(hashmap, key);

		node->key = key;
		new (&node->value) value_type{ std::forward<arg_types>(args)... }; 
		node->used = true;
		
		++hashmap.size;
		return &node->value;
	}

	template<typename key_type, typename value_type>
	value_type* emplace_zeroed(hashmap_t<key_type, value_type>& hashmap, const key_type& key)
	{
		ASSERT_MSG(hashmap.size < hashmap.capacity, "Failed to insert into hashmap because the hashmap is full");
		hashmap_node_t<key_type, value_type>* node = find_next_free_node<key_type, value_type>(hashmap, key);

		node->key = key;
		new (&node->value) value_type{}; 
		node->used = true;
		
		++hashmap.size;
		return &node->value;
	}

	template<typename key_type, typename value_type>
	void remove(hashmap_t<key_type, value_type>& hashmap, const key_type& key)
	{
		uint32_t node_index = hash_node_index<key_type, value_type>(hashmap, key);
		uint32_t counter = 0;

		// We need to loop here, if the object's hashed index was already occupied it was put further back
		while (hashmap.nodes[node_index].used || counter <= hashmap.capacity)
		{
			if (hashmap.nodes[node_index].key == key)
			{
				hashmap_node_t<key_type, value_type>* node = &hashmap.nodes[node_index];

				if constexpr (!std::is_trivially_destructible_v<value_type>)
				{
					node->value.~value_type();
				}

				node->key = {};
				node->value = {};
				node->used = false;

				hashmap.size--;
				break;
			}

			counter++;
			node_index++;
			node_index %= hashmap.capacity;
		}
	}

	template<typename key_type, typename value_type>
	value_type* find(hashmap_t<key_type, value_type>& hashmap, const key_type& key)
	{
		value_type* result = nullptr;
		uint32_t node_index = hash_node_index<key_type, value_type>(hashmap, key);
		uint32_t counter = 0;

		while (hashmap.nodes[node_index].used || counter <= hashmap.capacity)
		{
			if (hashmap.nodes[node_index].key == key)
			{
				result = &hashmap.nodes[node_index].value;
				break;
			}

			counter++;
			node_index++;
			node_index %= hashmap.capacity;
		}

		return result;
	}

	template<typename key_type, typename value_type>
	void clear(hashmap_t<key_type, value_type>& hashmap)
	{
		for (uint32_t i = 0; i < hashmap.capacity; ++i)
		{
			if constexpr (!std::is_trivially_destructible_v<value_type>)
			{
				hashmap.nodes[i].~value_type();
			}
			hashmap.nodes[i].used = false;
		}

		hashmap.size = 0;
	}

	template<typename key_type, typename value_type>
	hashmap_iter_t<key_type, value_type> get_iter(hashmap_t<key_type, value_type>& hashmap)
	{
		hashmap_iter_t<key_type, value_type> iter = {};
		iter.hashmap = &hashmap;
		iter.index = 0;
		
		return iter;
	}

	template<typename key_type, typename value_type>
	bool advance_iter(hashmap_iter_t<key_type, value_type>& iter)
	{
		while (iter.index < iter.hashmap->capacity)
		{
			uint32_t i = iter.index;
			++iter.index;
			if (iter.hashmap->nodes[i].used)
			{
				hashmap_node_t<key_type, value_type>& node = iter.hashmap->nodes[i];
				iter.key = &node.key;
				iter.value = &node.value;
				return true;
			}
		}

		return false;
	}
	
}
