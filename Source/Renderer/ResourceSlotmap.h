#pragma once
#include "Core/Assertion.h"
#include "RendererFwd.h"

template<typename THandle, typename TResource>
class ResourceSlotmap
{
public:
	static constexpr size_t DEFAULT_CAPACITY = 1000;

private:
	static constexpr size_t INVALID_SLOT_INDEX = ~0u;

public:
	ResourceSlotmap(size_t capacity = DEFAULT_CAPACITY)
	{
		m_Slots.resize(capacity);

		for (size_t i = 0; i < capacity - 1; ++i)
		{
			m_Slots[i].nextFree = i + 1;
			m_Slots[i].version = 0;
		}
	}

	~ResourceSlotmap()
	{
	}

	ResourceSlotmap(const ResourceSlotmap& other) = delete;
	ResourceSlotmap(ResourceSlotmap&& other) = delete;
	const ResourceSlotmap& operator=(const ResourceSlotmap& other) = delete;
	ResourceSlotmap&& operator=(ResourceSlotmap&& other) = delete;

	THandle Add(const TResource& resource)
	{
		THandle handle = AllocateSlot();
		m_Slots[handle.index].resource = resource;

		return handle;
	}

	THandle Add(TResource&& resource)
	{
		THandle handle = AllocateSlot();
		m_Slots[handle.index].resource = std::move(resource);

		return handle;
	}

	void Remove(THandle handle)
	{
		if (handle.IsValid())
		{
			Slot& slot = m_Slots[handle.index];

			// If the handle and slot version are not the same, the handle is stale and no longer valid
			if (slot.version == handle.version)
			{
				slot.version++;

				slot.nextFree = m_NextFreeIndex;
				m_NextFreeIndex = handle.index;

				// If the resource is not trivially destructible, we need to call its destructor
				if constexpr (!std::is_trivially_destructible_v<TResource>)
				{
					slot.resource.~T();
				}
			}
		}
		// TODO: If the handle is invalid, maybe log a warning message here
	}

	TResource* Find(THandle handle)
	{
		TResource* resource = nullptr;

		if (handle.IsValid())
		{
			Slot& slot = m_Slots[handle.index];

			if (slot.version == handle.version)
			{
				resource = &slot.resource;
			}
		}

		return resource;
	}

private:
	THandle AllocateSlot()
	{
		if (m_NextFreeIndex == INVALID_SLOT_INDEX)
		{
			// TODO: Resize instead of crashing when slotmap runs full
			FATAL_ERROR("ResourceSlotmap::AllocateSlot", "Resource slotmap has reached its maximum capacity");
		}

		ASSERT(m_NextFreeIndex < m_Slots.size());

		THandle handle;
		Slot& slot = m_Slots[m_NextFreeIndex];

		handle.index = m_NextFreeIndex;
		handle.version = slot.version;

		m_NextFreeIndex = slot.nextFree;
		slot.nextFree = INVALID_SLOT_INDEX;

		return handle;
	}

private:
	struct Slot
	{
		uint32_t nextFree = INVALID_SLOT_INDEX;
		uint32_t version = 0;

		TResource resource;
	};

private:
	std::vector<Slot> m_Slots;
	uint32_t m_NextFreeIndex = 0;

};
