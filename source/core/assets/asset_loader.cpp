#include "pch.h"
#include "asset_loader.h"

#include "renderer/renderer.h"

namespace asset_loader
{
	static void* stbi_malloc(u64 Size);
	static void* stbi_realloc(void* Ptr, u64 OldSize, u64 NewSize);
}

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC(Size) asset_loader::stbi_malloc(Size)
#define STBI_REALLOC_SIZED(Ptr, OldSize, NewSize) asset_loader::stbi_realloc(Ptr, OldSize, NewSize)
#define STBI_FREE(Ptr) // No-op, freeing is done by the arena itself
#include "stb/stb_image.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"

namespace asset_loader
{

	// Make stb_image use my arena allocator
	static memory_arena_t* s_stbi_memory_arena;

	static void* stbi_malloc(u64 size)
	{
		return ARENA_ALLOC_ZERO(s_stbi_memory_arena, size, 4);
	}

	static void* stbi_realloc(void* ptr, u64 old_size, u64 new_size)
	{
		void* ptr_new = ARENA_ALLOC_ZERO(s_stbi_memory_arena, new_size, 4);
		memcpy(ptr_new, ptr, old_size);
		return ptr_new;
	}

	texture_asset_t* load_image_hdr(memory_arena_t* arena, const char* filepath)
	{
		s_stbi_memory_arena = arena;
		texture_asset_t* asset = ARENA_ALLOC_STRUCT_ZERO(arena, texture_asset_t);

		ARENA_MEMORY_SCOPE(arena)
		{
			i32 image_width, image_height, channel_count;
			f32* ptr_image_data = stbi_loadf(filepath, &image_width, &image_height, &channel_count, STBI_rgb_alpha);

			if (!ptr_image_data)
			{
				FATAL_ERROR("asset_loader::load_image_hdr", "Failed to load HDR image: %s", filepath);
			}

			renderer::render_texture_params_t rtexture_params = {};
			rtexture_params.width = image_width;
			rtexture_params.height = image_height;
			rtexture_params.bytes_per_channel = 4;
			rtexture_params.channel_count = 4; // Forced to 4 by using STBI_rgb_alpha flag
			rtexture_params.ptr_data = reinterpret_cast<u8*>(ptr_image_data);
			rtexture_params.debug_name = ARENA_CHAR_TO_WIDE(arena, filepath).buf;

			asset->render_texture_handle = renderer::create_render_texture(rtexture_params);
		}

		return asset;
	}

	template<typename T>
	static T* cgltf_get_data_ptr(const cgltf_accessor* accessor)
	{
		cgltf_buffer_view* buffer_view = accessor->buffer_view;
		u8* ptr_base = (u8*)(buffer_view->buffer->data);
		ptr_base += buffer_view->offset;
		ptr_base += accessor->offset;

		return (T*)ptr_base;
	}

	scene_asset_t* load_gltf(memory_arena_t* arena, const char* filepath)
	{
		// Parse the GLTF file
		cgltf_options options = {};

		// Make CGLTF use our arena for allocations
		options.memory.user_data = arena;
		options.memory.alloc_func = [](void* user, cgltf_size size)
			{
				memory_arena_t* arena = (memory_arena_t*)user;
				return ARENA_ALLOC_ZERO(arena, size, 16);
			};
		// No-op, freeing is done by the arena itself
		options.memory.free_func = [](void* user, void* ptr)
			{
				(void)user; (void)ptr;
			};

		cgltf_data* ptr_data = nullptr;
		cgltf_result result = cgltf_parse_file(&options, filepath, &ptr_data);

		if (result != cgltf_result_success)
		{
			FATAL_ERROR("asset_loader::load_gltf", "Failed to load GLTF: %s", filepath);
		}

		cgltf_load_buffers(&options, ptr_data, filepath);

		u32 mesh_count = 0;
		for (u32 mesh_idx = 0; mesh_idx < ptr_data->meshes_count; ++mesh_idx)
		{
			const cgltf_mesh& mesh_gltf = ptr_data->meshes[mesh_idx];
			mesh_count += mesh_gltf.primitives_count;
		}

		scene_asset_t* asset = ARENA_ALLOC_STRUCT_ZERO(arena, scene_asset_t);
		asset->render_mesh_handles = ARENA_ALLOC_ARRAY_ZERO(arena, render_mesh_handle_t, mesh_count);

		// Use a temporary memory scope here since cpupathtracer::create_mesh is blocking, so we can do the scope inside the loop
		ARENA_MEMORY_SCOPE(arena)
		{
			for (u32 mesh_idx = 0; mesh_idx < ptr_data->meshes_count; ++mesh_idx)
			{
				const cgltf_mesh& mesh_gltf = ptr_data->meshes[mesh_idx];

				for (u32 prim_idx = 0; prim_idx < mesh_gltf.primitives_count; ++prim_idx)
				{
					const cgltf_primitive& prim_gltf = mesh_gltf.primitives[prim_idx];

					u32 index_count = prim_gltf.indices->count;
					u32 vertex_count = prim_gltf.attributes[0].data->count;

					u32* indices = ARENA_ALLOC_ARRAY(arena, u32, index_count);
					vertex_t* vertices = ARENA_ALLOC_ARRAY(arena, vertex_t, vertex_count);

					if (prim_gltf.indices->component_type == cgltf_component_type_r_32u)
					{
						memcpy(indices, cgltf_get_data_ptr<u32>(prim_gltf.indices), sizeof(u32) * prim_gltf.indices->count);
					}
					else if (prim_gltf.indices->component_type == cgltf_component_type_r_16u)
					{
						u16* ptr_src = cgltf_get_data_ptr<u16>(prim_gltf.indices);

						for (u32 index = 0; index < prim_gltf.indices->count; ++index)
						{
							indices[index] = ptr_src[index];
						}
					}

					for (u32 attr_idx = 0; attr_idx < prim_gltf.attributes_count; ++attr_idx)
					{
						const cgltf_attribute& attr_gltf = prim_gltf.attributes[attr_idx];
						ASSERT(attr_gltf.data->count == vertex_count);

						switch (attr_gltf.type)
						{
						case cgltf_attribute_type_position:
						{
							glm::vec3* ptr_src = cgltf_get_data_ptr<glm::vec3>(attr_gltf.data);

							for (u32 vert_idx = 0; vert_idx < attr_gltf.data->count; ++vert_idx)
							{
								vertices[vert_idx].position = ptr_src[vert_idx];
							}
						} break;
						case cgltf_attribute_type_normal:
						{
							glm::vec3* ptr_src = cgltf_get_data_ptr<glm::vec3>(attr_gltf.data);

							for (u32 vert_idx = 0; vert_idx < attr_gltf.data->count; ++vert_idx)
							{
								vertices[vert_idx].normal = ptr_src[vert_idx];
							}
						} break;
						}
					}

					// Create render mesh
					renderer::render_mesh_params_t rmesh_params = {};
					rmesh_params.vertex_count = vertex_count;
					rmesh_params.vertices = vertices;
					rmesh_params.index_count = index_count;
					rmesh_params.indices = indices;
					rmesh_params.debug_name = ARENA_CHAR_TO_WIDE(arena, filepath).buf;

					asset->render_mesh_handles[asset->mesh_handle_count] = renderer::create_render_mesh(rmesh_params);
					asset->mesh_handle_count++;
				}
			}
		}

		return asset;
	}

}
