#include "pch.h"
#include "asset_loader.h"

#include "renderer/renderer.h"
#include "renderer/shaders/shared.hlsl.h"
#include "core/logger.h"

namespace asset_loader
{
	static void* stbi_malloc(uint64_t size);
	static void* stbi_realloc(void* ptr, uint64_t old_size, uint64_t new_size);
}

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC(size) asset_loader::stbi_malloc(size)
#define STBI_REALLOC_SIZED(ptr, old_size, new_size) asset_loader::stbi_realloc(ptr, old_size, new_size)
#define STBI_FREE(ptr) // No-op, freeing is done by the arena itself
#include "stb/stb_image.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"

namespace asset_loader
{

	// Make stb_image use my arena allocator
	static memory_arena_t* s_stbi_memory_arena;

	static void* stbi_malloc(uint64_t size)
	{
		return ARENA_ALLOC_ZERO(*s_stbi_memory_arena, size, 4);
	}

	static void* stbi_realloc(void* ptr, uint64_t old_size, uint64_t new_size)
	{
		void* ptr_new = ARENA_ALLOC_ZERO(*s_stbi_memory_arena, new_size, 4);
		memcpy(ptr_new, ptr, old_size);
		return ptr_new;
	}

	struct image_load_result_t
	{
		uint32_t width;
		uint32_t height;
		uint32_t channel_count;
		uint32_t bytes_per_channel;
		uint8_t* data_ptr;
	};

	static image_load_result_t load_image_from_file(memory_arena_t& arena, const char* filepath, bool load_hdr)
	{
		image_load_result_t ret = {};
		s_stbi_memory_arena = &arena;

		int32_t image_width, image_height, channel_count;
		uint8_t* image_data_ptr;

		if (load_hdr)
		{
			image_data_ptr = (uint8_t*)stbi_loadf(filepath, &image_width, &image_height, &channel_count, STBI_rgb_alpha);
		}
		else
		{
			image_data_ptr = stbi_load(filepath, &image_width, &image_height, &channel_count, STBI_rgb_alpha);
		}
		
		if (!image_data_ptr)
		{
			FATAL_ERROR("asset_loader::load_image_from_file", "Failed to load image: %s", filepath);
		}

		ret.width = image_width;
		ret.height = image_height;
		ret.channel_count = 4;
		ret.bytes_per_channel = load_hdr ? 4 : 1;
		ret.data_ptr = image_data_ptr;

		return ret;
	}

	texture_asset_t* load_texture_rgba(memory_arena_t& arena, const char* filepath, TEXTURE_FORMAT format)
	{
		// TODO: Same code as below, maybe detect based on file extension whether to load HDR or not
		texture_asset_t* asset = ARENA_ALLOC_STRUCT_ZERO(arena, texture_asset_t);

		ARENA_MEMORY_SCOPE(arena)
		{
			image_load_result_t loaded_image = load_image_from_file(arena, filepath, false);

			renderer::render_texture_params_t rtexture_params = {};
			rtexture_params.width = loaded_image.width;
			rtexture_params.height = loaded_image.height;
			rtexture_params.bytes_per_channel = loaded_image.bytes_per_channel;
			rtexture_params.channel_count = loaded_image.channel_count;
			rtexture_params.format = format;
			rtexture_params.ptr_data = (uint8_t*)loaded_image.data_ptr;
			rtexture_params.debug_name = ARENA_CHAR_TO_WIDE(arena, filepath);

			asset->render_texture_handle = renderer::create_render_texture(rtexture_params);
		}

		return asset;
	}

	texture_asset_t* load_texture_hdr(memory_arena_t& arena, const char* filepath, TEXTURE_FORMAT format)
	{
		texture_asset_t* asset = ARENA_ALLOC_STRUCT_ZERO(arena, texture_asset_t);

		ARENA_MEMORY_SCOPE(arena)
		{
			image_load_result_t loaded_image = load_image_from_file(arena, filepath, true);

			renderer::render_texture_params_t rtexture_params = {};
			rtexture_params.width = loaded_image.width;
			rtexture_params.height = loaded_image.height;
			rtexture_params.bytes_per_channel = loaded_image.bytes_per_channel;
			rtexture_params.channel_count = loaded_image.channel_count;
			rtexture_params.format = format;
			rtexture_params.ptr_data = (uint8_t*)loaded_image.data_ptr;
			rtexture_params.debug_name = ARENA_CHAR_TO_WIDE(arena, filepath);

			asset->render_texture_handle = renderer::create_render_texture(rtexture_params);
		}

		return asset;
	}

	template<typename T>
	static T* cgltf_get_data_ptr(const cgltf_accessor* accessor)
	{
		cgltf_buffer_view* buffer_view = accessor->buffer_view;
		uint8_t* ptr_base = (uint8_t*)(buffer_view->buffer->data);
		ptr_base += buffer_view->offset;
		ptr_base += accessor->offset;

		return (T*)ptr_base;
	}

	struct gltf_load_result_t
	{
		cgltf_data* data;
	};

	static gltf_load_result_t gltf_load_from_file(memory_arena_t& arena, const char* filepath)
	{
		gltf_load_result_t ret = {};

		// Parse the GLTF file
		cgltf_options options = {};

		// Make CGLTF use our arena for allocations
		options.memory.user_data = &arena;
		options.memory.alloc_func = [](void* user, cgltf_size size)
			{
				memory_arena_t* arena = (memory_arena_t*)user;
				return ARENA_ALLOC_ZERO(*arena, size, 16);
			};
		// No-op, freeing is done by the arena itself
		options.memory.free_func = [](void* user, void* ptr)
			{
				(void)user; (void)ptr;
			};

		cgltf_result result = cgltf_parse_file(&options, filepath, &ret.data);

		if (result != cgltf_result_success)
		{
			FATAL_ERROR("asset_loader::gltf_load_from_file", "Failed to load GLTF: %s", filepath);
		}

		cgltf_load_buffers(&options, ret.data, filepath);
		return ret;
	}

	static char* gltf_get_path(memory_arena_t& arena, const char* filepath, const char* uri)
	{
		char* ret = ARENA_ALLOC_ARRAY_ZERO(arena, char, strlen(filepath) + strlen(uri));

		cgltf_combine_paths(ret, filepath, uri);
		cgltf_decode_uri(ret + strlen(ret) - strlen(uri));

		return ret;
	}

	template<typename T>
	static uint32_t gltf_get_index(const T* arr, const T* elem)
	{
		return static_cast<uint32_t>(elem - arr);
	}

	static texture_asset_t gltf_load_texture(memory_arena_t& arena, const char* filepath, const cgltf_image& gltf_image, TEXTURE_FORMAT format)
	{
		const char* image_filepath = gltf_get_path(arena, filepath, gltf_image.uri);
		return *load_texture_rgba(arena, image_filepath, format);
	}

	struct gltf_load_materials_result_t
	{
		material_asset_t* material_assets;
		uint32_t material_asset_count;
	};

	static gltf_load_materials_result_t gltf_load_materials(memory_arena_t& arena, const char* filepath, gltf_load_result_t& loaded_gltf)
	{
		// GLTF 2.0 Spec states that:
		// - Base color textures are encoded in SRGB
		// - Normal textures are encoded in linear
		// - Metallic roughness textures are encoded in linear
		// - Emissive textures are encoded in SRGB
		gltf_load_materials_result_t ret = {};
		ret.material_assets = ARENA_ALLOC_ARRAY_ZERO(arena, material_asset_t, loaded_gltf.data->materials_count);

		for (uint32_t mat_idx = 0; mat_idx < loaded_gltf.data->materials_count; ++mat_idx)
		{
			cgltf_material& gltf_material = loaded_gltf.data->materials[mat_idx];
			material_asset_t& asset = ret.material_assets[mat_idx];

			if (gltf_material.has_pbr_metallic_roughness)
			{
				asset.base_color_factor = *(glm::vec4*)gltf_material.pbr_metallic_roughness.base_color_factor;
				asset.metallic_factor = gltf_material.pbr_metallic_roughness.metallic_factor;
				asset.roughness_factor = gltf_material.pbr_metallic_roughness.roughness_factor;
				asset.emissive_factor = *(glm::vec3*)gltf_material.emissive_factor;
				asset.emissive_strength = gltf_material.emissive_strength.emissive_strength;

				ARENA_MEMORY_SCOPE(arena)
				{
					if (gltf_material.pbr_metallic_roughness.base_color_texture.texture)
					{
						cgltf_image* gltf_image = gltf_material.pbr_metallic_roughness.base_color_texture.texture->image;
						if (gltf_image)
						{
							asset.base_color_texture = gltf_load_texture(arena, filepath, *gltf_image, TEXTURE_FORMAT_RGBA8_SRGB);
						}
					}

					if (gltf_material.normal_texture.texture)
					{
						cgltf_image* gltf_image = gltf_material.normal_texture.texture->image;
						if (gltf_image)
						{
							asset.normal_texture = gltf_load_texture(arena, filepath, *gltf_image, TEXTURE_FORMAT_RGBA8);
						}
					}

					if (gltf_material.pbr_metallic_roughness.metallic_roughness_texture.texture)
					{
						cgltf_image* gltf_image = gltf_material.pbr_metallic_roughness.metallic_roughness_texture.texture->image;
						if (gltf_image)
						{
							asset.metallic_roughness_texture = gltf_load_texture(arena, filepath, *gltf_image, TEXTURE_FORMAT_RGBA8);
						}
					}

					if (gltf_material.has_emissive_strength)
					{
						cgltf_image* gltf_image = gltf_material.emissive_texture.texture->image;
						if (gltf_image)
						{
							asset.emissive_texture = gltf_load_texture(arena, filepath, *gltf_image, TEXTURE_FORMAT_RGBA8_SRGB);
						}
					}
				}
			}
			else
			{
				// If the material has no metallic roughness properties, pick sensible defaults and log warning
				LOG_WARN("Loaded a material for GLTF %s that has no PBR metallic roughness properties", filepath);

				asset.base_color_factor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
				asset.metallic_factor = 1.0f;
				asset.roughness_factor = 1.0f;
				asset.emissive_factor = glm::vec3(0.0f, 0.0f, 0.0f);
				asset.emissive_strength = 0.0f;
				asset.base_color_texture.render_texture_handle.handle = ~0u;
				asset.normal_texture.render_texture_handle.handle = ~0u;
				asset.metallic_roughness_texture.render_texture_handle.handle = ~0u;
				asset.emissive_texture.render_texture_handle.handle = ~0u;
			}

			++ret.material_asset_count;
		}

		return ret;
	}

	struct gltf_load_meshes_result_t
	{
		mesh_asset_t* mesh_assets;
		uint32_t mesh_asset_count;
	};

	static gltf_load_meshes_result_t gltf_load_meshes(memory_arena_t& arena, const char* filepath, gltf_load_result_t& loaded_gltf)
	{
		gltf_load_meshes_result_t ret = {};

		uint32_t mesh_count = 0;
		for (uint32_t mesh_idx = 0; mesh_idx < loaded_gltf.data->meshes_count; ++mesh_idx)
		{
			const cgltf_mesh& mesh_gltf = loaded_gltf.data->meshes[mesh_idx];
			mesh_count += mesh_gltf.primitives_count;
		}

		ret.mesh_assets = ARENA_ALLOC_ARRAY_ZERO(arena, mesh_asset_t, mesh_count);

		ARENA_MEMORY_SCOPE(arena)
		{
			for (uint32_t mesh_idx = 0; mesh_idx < loaded_gltf.data->meshes_count; ++mesh_idx)
			{
				const cgltf_mesh& mesh_gltf = loaded_gltf.data->meshes[mesh_idx];

				for (uint32_t prim_idx = 0; prim_idx < mesh_gltf.primitives_count; ++prim_idx)
				{
					const cgltf_primitive& prim_gltf = mesh_gltf.primitives[prim_idx];

					uint32_t index_count = prim_gltf.indices->count;
					uint32_t vertex_count = prim_gltf.attributes[0].data->count;

					uint32_t* indices = ARENA_ALLOC_ARRAY(arena, uint32_t, index_count);
					vertex_t* vertices = ARENA_ALLOC_ARRAY(arena, vertex_t, vertex_count);

					if (prim_gltf.indices->component_type == cgltf_component_type_r_32u)
					{
						memcpy(indices, cgltf_get_data_ptr<uint32_t>(prim_gltf.indices), sizeof(uint32_t) * prim_gltf.indices->count);
					}
					else if (prim_gltf.indices->component_type == cgltf_component_type_r_16u)
					{
						uint16_t* ptr_src = cgltf_get_data_ptr<uint16_t>(prim_gltf.indices);

						for (uint32_t index = 0; index < prim_gltf.indices->count; ++index)
						{
							indices[index] = ptr_src[index];
						}
					}

					for (uint32_t attr_idx = 0; attr_idx < prim_gltf.attributes_count; ++attr_idx)
					{
						const cgltf_attribute& attr_gltf = prim_gltf.attributes[attr_idx];
						ASSERT(attr_gltf.data->count == vertex_count);

						switch (attr_gltf.type)
						{
						case cgltf_attribute_type_position:
						{
							glm::vec3* ptr_src = cgltf_get_data_ptr<glm::vec3>(attr_gltf.data);

							for (uint32_t vert_idx = 0; vert_idx < attr_gltf.data->count; ++vert_idx)
							{
								vertices[vert_idx].position = ptr_src[vert_idx];
							}
						} break;
						case cgltf_attribute_type_normal:
						{
							glm::vec3* ptr_src = cgltf_get_data_ptr<glm::vec3>(attr_gltf.data);

							for (uint32_t vert_idx = 0; vert_idx < attr_gltf.data->count; ++vert_idx)
							{
								vertices[vert_idx].normal = ptr_src[vert_idx];
							}
						} break;
						case cgltf_attribute_type_texcoord:
						{
							glm::vec2* ptr_src = cgltf_get_data_ptr<glm::vec2>(attr_gltf.data);

							for (uint32_t vert_idx = 0; vert_idx < attr_gltf.data->count; ++vert_idx)
							{
								vertices[vert_idx].tex_coord = ptr_src[vert_idx];
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
					rmesh_params.debug_name = ARENA_CHAR_TO_WIDE(arena, filepath);

					ret.mesh_assets[ret.mesh_asset_count].render_mesh_handle = renderer::create_render_mesh(rmesh_params);
					++ret.mesh_asset_count;
				}
			}
		}

		return ret;
	}

	struct gltf_load_nodes_result_t
	{
		uint32_t* root_node_indices;
		uint32_t root_node_count;
		scene_node_t* nodes;
		uint32_t node_count;
	};
	
	static glm::mat4 gltf_node_get_transform(const cgltf_node& node)
	{
		glm::mat4 transform = glm::identity<glm::mat4>();

		if (node.has_matrix)
		{
			memcpy(&transform[0][0], &node.matrix[0], sizeof(glm::mat4));
			return transform;
		}

		glm::vec3 translation(0.0);
		glm::quat rotation(0.0, 0.0, 0.0, 0.0);
		glm::vec3 scale(1.0);

		if (node.has_translation)
		{
			translation.x = node.translation[0];
			translation.y = node.translation[1];
			translation.z = node.translation[2];
		}
		if (node.has_rotation)
		{
			rotation.x = node.rotation[0];
			rotation.y = node.rotation[1];
			rotation.z = node.rotation[2];
			rotation.w = node.rotation[3];
		}
		if (node.has_scale)
		{
			scale.x = node.scale[0];
			scale.y = node.scale[1];
			scale.z = node.scale[2];
		}

		transform = glm::translate(transform, translation);
		transform = transform * glm::mat4_cast(rotation);
		transform = glm::scale(transform, scale);

		return transform;
	}

	static gltf_load_nodes_result_t gltf_load_nodes(memory_arena_t& arena, gltf_load_result_t& loaded_gltf)
	{
		gltf_load_nodes_result_t ret = {};

		uint32_t root_node_count = 0;
		uint32_t node_count = loaded_gltf.data->nodes_count;

		for (uint32_t node_idx = 0; node_idx < loaded_gltf.data->nodes_count; ++node_idx)
		{
			cgltf_node& gltf_node = loaded_gltf.data->nodes[node_idx];

			if (!gltf_node.parent)
			{
				++root_node_count;
			}
		}

		ret.root_node_indices = ARENA_ALLOC_ARRAY_ZERO(arena, uint32_t, root_node_count);
		ret.nodes = ARENA_ALLOC_ARRAY_ZERO(arena, scene_node_t, node_count);

		for (uint32_t node_idx = 0; node_idx < loaded_gltf.data->nodes_count; ++node_idx)
		{
			cgltf_node& gltf_node = loaded_gltf.data->nodes[node_idx];
			ASSERT(gltf_node.mesh);

			scene_node_t& scene_node = ret.nodes[node_idx];
			scene_node.transform = gltf_node_get_transform(gltf_node);
			scene_node.children = ARENA_ALLOC_ARRAY_ZERO(arena, uint32_t, gltf_node.children_count);
			scene_node.child_count = gltf_node.children_count;
			scene_node.mesh_indices = ARENA_ALLOC_ARRAY_ZERO(arena, uint32_t, gltf_node.mesh->primitives_count);
			scene_node.material_indices = ARENA_ALLOC_ARRAY_ZERO(arena, uint32_t, gltf_node.mesh->primitives_count);
			scene_node.mesh_count = gltf_node.mesh->primitives_count;

			if (!gltf_node.parent)
			{
				ret.root_node_indices[ret.root_node_count] = node_idx;
				++ret.root_node_count;
			}

			for (uint32_t i = 0; i < gltf_node.mesh->primitives_count; ++i)
			{
				scene_node.mesh_indices[i] = gltf_get_index<cgltf_primitive>(gltf_node.mesh->primitives, &gltf_node.mesh->primitives[i]);
				scene_node.material_indices[i] = gltf_get_index<cgltf_material>(loaded_gltf.data->materials, gltf_node.mesh->primitives[i].material);
			}

			++ret.node_count;
		}

		return ret;
	}

	scene_asset_t* load_gltf(memory_arena_t& arena, const char* filepath)
	{
		scene_asset_t* asset = ARENA_ALLOC_STRUCT_ZERO(arena, scene_asset_t);

		ARENA_SCRATCH_SCOPE()
		{
			gltf_load_result_t loaded_gltf = gltf_load_from_file(arena_scratch, filepath);
			gltf_load_materials_result_t loaded_materials = gltf_load_materials(arena_scratch, filepath, loaded_gltf);
			gltf_load_meshes_result_t loaded_meshes = gltf_load_meshes(arena_scratch, filepath, loaded_gltf);
			gltf_load_nodes_result_t loaded_nodes = gltf_load_nodes(arena, loaded_gltf);

			asset->material_assets = ARENA_ALLOC_ARRAY_ZERO(arena, material_asset_t, loaded_materials.material_asset_count);
			memcpy(asset->material_assets, loaded_materials.material_assets, sizeof(material_asset_t) * loaded_materials.material_asset_count);
			asset->material_asset_count = loaded_materials.material_asset_count;
			asset->mesh_assets = ARENA_ALLOC_ARRAY_ZERO(arena, mesh_asset_t, loaded_meshes.mesh_asset_count);
			memcpy(asset->mesh_assets, loaded_meshes.mesh_assets, sizeof(mesh_asset_t) * loaded_meshes.mesh_asset_count);
			asset->mesh_asset_count = loaded_meshes.mesh_asset_count;

			asset->root_node_indices = loaded_nodes.root_node_indices;
			asset->root_node_count = loaded_nodes.root_node_count;
			asset->nodes = loaded_nodes.nodes;
			asset->node_count = loaded_nodes.node_count;
		}

		return asset;
	}

}
