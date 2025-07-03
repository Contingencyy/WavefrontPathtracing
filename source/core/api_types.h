#pragma once

template<typename T>
union resource_handle_t
{
	struct
	{
		uint32_t index;
		uint32_t version;
	};

	uint64_t handle = ~0u;

	resource_handle_t() = default;

	bool is_valid() const { return handle != ~0u; }
	void invalidate() { handle = ~0u; }

	inline bool operator==(const resource_handle_t& rhs) const { return handle == rhs.handle; }
	inline bool operator!=(const resource_handle_t& rhs) const { return handle != rhs.handle; }
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

struct timer_t
{
	int64_t val;
};
