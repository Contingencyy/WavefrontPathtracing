#pragma once

template<typename value_type>
struct slotmap_slot_t
{
	uint32_t next_free;
	uint32_t version;

	value_type value;
};

template<typename handle_type, typename value_type>
struct slotmap_t
{
	static_assert(std::is_trivially_destructible_v<handle_type>, "Slotmap implementation does not support non-trivially destructible handle types");
	
	uint32_t capacity;
	slotmap_slot_t<value_type>* slots;
};

namespace slotmap
{
	
	inline constexpr uint32_t SLOTMAP_DEFAULT_CAPACITY = 1024u;
	inline constexpr uint32_t SLOTMAP_INVALID_SLOT_INDEX = ~0u;

	template<typename handle_type, typename value_type>
	void init(slotmap_t<handle_type, value_type>& slotmap, memory_arena_t* arena, uint32_t capacity = SLOTMAP_DEFAULT_CAPACITY)
	{
		slotmap.capacity = capacity;
		slotmap.slots = ARENA_ALLOC_ARRAY_ZERO(arena, slotmap_slot_t<value_type>, slotmap.capacity);

		for (uint32_t i = 0; i < slotmap.capacity - 1; ++i)
		{
			slotmap.slots[i].next_free = i + 1;
			slotmap.slots[i].version = 0;
		}
	}

	template<typename handle_type, typename value_type>
	void destroy(slotmap_t<handle_type, value_type>& slotmap)
	{
		if constexpr (!std::is_trivially_destructible_v<value_type>)
		{
			for (uint32_t i = 0; i < slotmap.capacity; ++i)
			{
				slotmap.slots[i].~value_type();
			}
		}
		
		slotmap.capacity = 0;
		slotmap.slots = nullptr;
	}

	template<typename handle_type, typename value_type>
	handle_type allocate_slot(slotmap_t<handle_type, value_type>& slotmap)
	{
		handle_type handle = {};
		slotmap_slot_t<value_type>* sentinel = &slotmap.slots[0];

		if (sentinel->next_free != SLOTMAP_INVALID_SLOT_INDEX)
		{
			uint32_t index = sentinel->next_free;
			slotmap_slot_t<value_type>* slot = &slotmap.slots[index];

			sentinel->next_free = slot->next_free;
			slot->next_free = SLOTMAP_INVALID_SLOT_INDEX;

			handle.index = index;
			handle.version = slot->version;
		}

		return handle;
	}

	template<typename handle_type, typename value_type>
	handle_type add(slotmap_t<handle_type, value_type>& slotmap, const value_type& value)
	{
		handle_type handle = allocate_slot<handle_type, value_type>(slotmap);
		ASSERT_MSG(handle.is_valid(), "Failed to insert value into slotmap because slotmap is full");

		slotmap.slots[handle.index].value = value;
		return handle;
	}

	template<typename handle_type, typename value_type>
	void remove(slotmap_t<handle_type, value_type>& slotmap, handle_type handle)
	{
		slotmap_slot_t<value_type>* sentinel = &slotmap.slots[0];

		if (handle.is_valid())
		{
			slotmap_slot_t<value_type>* slot = &slotmap.slots[handle.index];

			// If the handle and slot version are not the same, the handle is stale and no longer valid
			if (slot->version == handle.version)
			{
				slot->version++;

				slot->next_free = sentinel->next_free;
				sentinel->next_free = handle.index;

				if constexpr (!std::is_trivially_destructible_v<value_type>)
				{
					slot->value.~value_type();
				}
			}
		}
	}

	template<typename handle_type, typename value_type>
	value_type* find(slotmap_t<handle_type, value_type>& slotmap, handle_type handle)
	{
		value_type* value = nullptr;

		if (handle.is_valid())
		{
			slotmap_slot_t<value_type>* slot = &slotmap.slots[handle.index];
			if (slot->version == handle.version)
			{
				value = &slot->value;
			}
		}

		return value;
	}

}
