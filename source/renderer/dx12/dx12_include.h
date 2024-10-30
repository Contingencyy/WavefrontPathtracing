#pragma once

#include "d3d12.h"
#include "dxgi1_6.h"
#include "dxcompiler/inc/d3d12shader.h"
#include "dxcompiler/inc/dxcapi.h"

// Note: Windows.h needs to be included after d3d12.h, otherwise it will complain about redefinitions from the agility sdk
#include "platform/windows/windows_common.h"

#include "core/common.h"
#include "core/assertion.h"

#define DX12_VALIDATE_RESOURCE_VIEWS 1

static inline void dx_check_hr(i32 Line, const char* File, HRESULT HR)
{
	if (FAILED(HR))
		fatal_error(Line, File, "DX12 Backend", get_hr_message(HR));
}

#define DX_CHECK_HR(hr) dx_check_hr(__LINE__, __FILE__, hr)
#define DX_RELEASE_OBJECT(object) \
if ((object)) \
{ \
	ULONG ref_count = (object)->Release(); \
	while (ref_count > 0) \
	{ \
		/* Add a log warning here if it has to release more than once */ \
		ref_count = (object)->Release(); \
	} \
} \
(object) = nullptr
