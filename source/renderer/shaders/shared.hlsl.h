#pragma once

#if !defined(__HLSL_VERSION)
#define int i32
#define uint u32
#define float f32
#define float2 glm::vec2
#define float3 glm::vec3
#define float4 glm::vec4
#define float3x3 glm::mat3
#define float4x4 glm::mat4
#endif

struct view_shader_data_t
{
    float4x4 world_to_view; // View matrix
    float4x4 view_to_world; // Inverse view matrix
    float4x4 view_to_clip;  // Projection matrix
    float4x4 clip_to_view;  // Inverse projection matrix

    float2 render_dim; // Render dimensions/resolution in pixels
};

#if !defined(__HLSL_VERSION)
#undef int
#undef uint
#undef float
#undef float2
#undef float3
#undef float4
#undef float3x3
#undef float4x4
#endif
