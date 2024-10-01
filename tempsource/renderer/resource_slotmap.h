#pragma once
#include "core/assertion.h"
#include "renderer_fwd.h"

template<typename THandle, typename TResource>
class resource_slotmap_t
{
public:
	static constexpr u32 DEFAULT_CAPACITY = 1000;

private:
	static constexpr u32 INVALID_SLOT_INDEX = ~0u;

public:
	void init(memory_arena_t* arena, u32 capacity = DEFAULT_CAPACITY)
	{
		m_slot_count = capacity;
		m_slots = ARENA_ALLOC_ARRAY_ZERO(arena, slot_t, m_slot_count);

		for (u32 i = 0; i < m_slot_count - 1; ++i)
		{
			m_slots[i].next_free = i + 1;
			m_slots[i].version = 0;
		}
	}

	void destroy()
	{
	}

	resource_slotmap_t(const resource_slotmap_t& other) = delete;
	resource_slotmap_t(resource_slotmap_t&& other) = delete;
	const resource_slotmap_t& operator=(const resource_slotmap_t& other) = delete;
	resource_slotmap_t&& operator=(resource_slotmap_t&& other) = delete;

	THandle add(const TResource& resource)
	{
		THandle handle = allocate_slot();
		ASSERT(handle.is_valid());

		m_slots[handle.index].resource = resource;
		return handle;
	}

	THandle add(TResource&& resource)
	{
		THandle handle = allocate_slot();
		ASSERT(handle.is_valid());

		m_slots[handle.index].resource = std::move(resource);
		return handle;
	}

	void Remove(THandle handle)
	{
		slot_t* sentinel = &m_slots[0];

		if (handle.is_valid())
		{
			slot_t* slot = &m_slots[handle.index];

			// If the handle and slot version are not the same, the handle is stale and no longer valid
			if (slot.version == handle.version)
			{
				slot.version++;

				slot.next_free = sentinel->next_free;
				sentinel->next_free = handle.index;

				// If the resource is not trivially destructible, we need to call its destructor
				if constexpr (!std::is_trivially_destructible_v<TResource>)
				{
					slot.resource.~T();
				}
			}
		}
		// TODO: If the handle is invalid, maybe log a warning message here
	}

	TResource* find(THandle handle)
	{
		TResource* resource = nullptr;

		if (handle.is_valid())
		{
			slot_t* slot = &m_slots[handle.index];
			if (slot->version == handle.version)
			{
				resource = &slot->resource;
			}
		}

		return resource;
	}

private:
	THandle allocate_slot()
	{
		THandle handle = {};
		slot_t* sentinel = &m_slots[0];

		if (sentinel->next_free != INVALID_SLOT_INDEX)
		{
			u32 index = sentinel->next_free;
			slot_t* slot = &m_slots[index];

			sentinel->next_free = slot->next_free;
			slot->next_free = INVALID_SLOT_INDEX;

			handle.index = index;
			handle.version = slot->version;
		}

		return handle;
	}

private:
	struct slot_t
	{
		u32 next_free = INVALID_SLOT_INDEX;
		u32 version = 0;

		TResource resource;
	};

private:
	u32 m_slot_count;
	slot_t* m_slots;

};
