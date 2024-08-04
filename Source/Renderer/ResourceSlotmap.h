#pragma once
#include "Core/Assertion.h"
#include "RendererFwd.h"

template<typename THandle, typename TResource>
class ResourceSlotmap
{
public:
	static constexpr u32 DEFAULT_CAPACITY = 1000;

private:
	static constexpr u32 INVALID_SLOT_INDEX = ~0u;

public:
	void Init(MemoryArena* Arena, u32 Capacity = DEFAULT_CAPACITY)
	{
		m_SlotCount = Capacity;
		m_Slots = ARENA_ALLOC_ARRAY_ZERO(Arena, Slot, m_SlotCount);

		for (u32 i = 0; i < m_SlotCount - 1; ++i)
		{
			m_Slots[i].NextFree = i + 1;
			m_Slots[i].Version = 0;
		}
	}

	void Destroy()
	{
	}

	ResourceSlotmap(const ResourceSlotmap& Other) = delete;
	ResourceSlotmap(ResourceSlotmap&& Other) = delete;
	const ResourceSlotmap& operator=(const ResourceSlotmap& Other) = delete;
	ResourceSlotmap&& operator=(ResourceSlotmap&& Other) = delete;

	THandle Add(const TResource& Resource)
	{
		THandle Handle = AllocateSlot();
		ASSERT(Handle.IsValid());

		m_Slots[Handle.Index].Resource = Resource;
		return Handle;
	}

	THandle Add(TResource&& Resource)
	{
		THandle Handle = AllocateSlot();
		ASSERT(Handle.IsValid());

		m_Slots[Handle.Index].Resource = std::move(Resource);
		return Handle;
	}

	void Remove(THandle Handle)
	{
		Slot* Sentinel = &m_Slots[0];

		if (Handle.IsValid())
		{
			Slot* Slot = &m_Slots[Handle.Index];

			// If the Handle and slot Version are not the same, the Handle is stale and no longer valid
			if (Slot.Version == Handle.Version)
			{
				Slot.Version++;

				Slot.NextFree = Sentinel->NextFree;
				Sentinel->NextFree = Handle.Index;

				// If the Resource is not trivially destructible, we need to call its destructor
				if constexpr (!std::is_trivially_destructible_v<TResource>)
				{
					Slot.Resource.~T();
				}
			}
		}
		// TODO: If the Handle is invalid, maybe log a warning message here
	}

	TResource* Find(THandle Handle)
	{
		TResource* Resource = nullptr;

		if (Handle.IsValid())
		{
			Slot* Slot = &m_Slots[Handle.Index];
			if (Slot->Version == Handle.Version)
			{
				Resource = &Slot->Resource;
			}
		}

		return Resource;
	}

private:
	THandle AllocateSlot()
	{
		THandle Handle = {};
		Slot* Sentinel = &m_Slots[0];

		if (Sentinel->NextFree != INVALID_SLOT_INDEX)
		{
			u32 Index = Sentinel->NextFree;
			Slot* Slot = &m_Slots[Index];

			Sentinel->NextFree = Slot->NextFree;
			Slot->NextFree = INVALID_SLOT_INDEX;

			Handle.Index = Index;
			Handle.Version = Slot->Version;
		}

		return Handle;
	}

private:
	struct Slot
	{
		u32 NextFree = INVALID_SLOT_INDEX;
		u32 Version = 0;

		TResource Resource;
	};

private:
	u32 m_SlotCount;
	Slot* m_Slots;

};
