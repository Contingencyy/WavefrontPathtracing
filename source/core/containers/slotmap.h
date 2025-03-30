#pragma once

template<typename T>
struct slotmap_slot_t
{
	uint32_t next_free;
	uint32_t version;

	T value;
};

template<typename THandle, typename TValue>
struct slotmap_t
{
	uint32_t capacity;
	slotmap_slot_t<TValue>* slots;
};

namespace slotmap
{

	inline constexpr uint32_t SLOTMAP_DEFAULT_CAPACITY = 1024u;
	inline constexpr uint32_t SLOTMAP_INVALID_SLOT_INDEX = ~0u;

	template<typename THandle, typename TValue>
	void init(slotmap_t<THandle, TValue>& slotmap, memory_arena_t* arena, uint32_t capacity = SLOTMAP_DEFAULT_CAPACITY)
	{
		slotmap.capacity = capacity;
		slotmap.slots = ARENA_ALLOC_ARRAY_ZERO(arena, slotmap_slot_t<TValue>, slotmap.capacity);

		for (uint32_t i = 0; i < slotmap.capacity - 1; ++i)
		{
			slotmap.slots[i].next_free = i + 1;
			slotmap.slots[i].version = 0;
		}
	}

	template<typename THandle, typename TValue>
	void destroy(slotmap_t<THandle, TValue>& slotmap)
	{
		slotmap.capacity = 0;
		slotmap.slots = nullptr;
	}

	template<typename THandle, typename TValue>
	THandle allocate_slot(slotmap_t<THandle, TValue>& slotmap)
	{
		THandle handle = {};
		slotmap_slot_t<TValue>* sentinel = &slotmap.slots[0];

		if (sentinel->next_free != SLOTMAP_INVALID_SLOT_INDEX)
		{
			uint32_t index = sentinel->next_free;
			slotmap_slot_t<TValue>* slot = &slotmap.slots[index];

			sentinel->next_free = slot->next_free;
			slot->next_free = SLOTMAP_INVALID_SLOT_INDEX;

			handle.index = index;
			handle.version = slot->version;
		}

		return handle;
	}

	template<typename THandle, typename TValue>
	THandle add(slotmap_t<THandle, TValue>& slotmap, const TValue& value)
	{
		THandle handle = allocate_slot<THandle, TValue>(slotmap);
		ASSERT_MSG(handle.is_valid(), "Failed to insert value into slotmap because slotmap is full");

		slotmap.slots[handle.index].value = value;
		return handle;
	}

	template<typename THandle, typename TValue>
	void remove(slotmap_t<THandle, TValue>& slotmap, THandle handle)
	{
		slotmap_slot_t<TValue>* sentinel = &slotmap.slots[0];

		if (handle.is_valid())
		{
			slotmap_slot_t<TValue>* slot = &slotmap.slots[handle.index];

			// If the handle and slot version are not the same, the handle is stale and no longer valid
			if (slot->version == handle.version)
			{
				slot->version++;

				slot->next_free = sentinel->next_free;
				sentinel->next_free = handle.index;

				if constexpr (!std::is_trivially_destructible_v<TValue>)
				{
					slot->value.~TValue();
				}
			}
		}
	}

	template<typename THandle, typename TValue>
	TValue* find(slotmap_t<THandle, TValue>& slotmap, THandle handle)
	{
		TValue* value = nullptr;

		if (handle.is_valid())
		{
			slotmap_slot_t<TValue>* slot = &slotmap.slots[handle.index];
			if (slot->version == handle.version)
			{
				value = &slot->value;
			}
		}

		return value;
	}

}
