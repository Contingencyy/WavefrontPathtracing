#pragma once

template<typename T>
union resource_handle_t
{
	struct
	{
		u32 index;
		u32 version;
	};

	uint64_t handle = ~0u;

	resource_handle_t() = default;

	b8 is_valid() const { return handle != ~0u; }
	void invalidate() { handle = ~0u; }

	inline b8 operator==(const resource_handle_t& rhs) const { return handle == rhs.handle; }
	inline b8 operator!=(const resource_handle_t& rhs) const { return handle != rhs.handle; }
};

#define DECLARE_HANDLE_TYPE(type) using type = resource_handle_t<struct type##__tag>

struct thread_t
{
	void* ptr;
};

struct mutex_t
{
	void* ptr;
};

struct cond_var_t
{
	void* ptr;
};
