#include "pch.h"
#include "asset_loader.h"
#include "dds.h"

#include "core/fileio/fileio.h"

#include "renderer/renderer.h"
#include "renderer/shaders/shared.hlsl.h"

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

#include "ufbx/ufbx.h"

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

	enum TEXTURE_FILE_TYPE
	{
		TEXTURE_FILE_TYPE_PNG,
		TEXTURE_FILE_TYPE_HDR,
		TEXTURE_FILE_TYPE_DDS,
		TEXTURE_FILE_TYPE_UNKNOWN
	};

	static TEXTURE_FILE_TYPE get_texture_file_type(const char* filepath)
	{
		const char* extension_delim = strrchr(filepath, '.');
		if (!extension_delim || extension_delim == filepath)
		{
			return TEXTURE_FILE_TYPE_UNKNOWN;
		}

		const char* extension = extension_delim + 1;
		if (strcmp(extension_delim, ".png") == 0)
		{
			return TEXTURE_FILE_TYPE_PNG;
		}
		if (strcmp(extension_delim, ".hdr") == 0)
		{
			return TEXTURE_FILE_TYPE_HDR;
		}
		if (strcmp(extension_delim, ".dds") == 0)
		{
			return TEXTURE_FILE_TYPE_DDS;
		}
		
		return TEXTURE_FILE_TYPE_UNKNOWN;
	}

	struct image_load_result_t
	{
		TEXTURE_FORMAT format;
		int32_t width;
		int32_t height;
		int32_t bits_per_pixel;
		uint8_t* data_ptr;
		bool loaded;
	};

	static texture_asset_t load_texture_png(memory_arena_t& arena, memory_arena_t& arena_scratch, const char* filepath, bool srgb)
	{
		texture_asset_t ret = {};
		ret.render_texture_handle.handle = INVALID_HANDLE;
		
		// -------------------------------------------------------------------------------------------------------------
		// Load the file
		image_load_result_t loaded_image = {};
		{
			s_stbi_memory_arena = &arena;

			int32_t channels;
			loaded_image.data_ptr = stbi_load(filepath, &loaded_image.width, &loaded_image.height, &channels, STBI_rgb_alpha);
			loaded_image.loaded = loaded_image.data_ptr;
			loaded_image.bits_per_pixel = 32;
			loaded_image.format = srgb ? TEXTURE_FORMAT_RGBA8_SRGB : TEXTURE_FORMAT_RGBA8;
		}

		// -------------------------------------------------------------------------------------------------------------
		// Upload texture to the GPU
		if (!loaded_image.loaded)
		{
			LOG_ERR("Assets", "Failed to load image: %s", filepath);
			return ret;
		}
		
		{
			renderer::render_texture_params_t rtexture_params = {};
			rtexture_params.width = loaded_image.width;
			rtexture_params.height = loaded_image.height;
			rtexture_params.bits_per_pixel = loaded_image.bits_per_pixel;
			rtexture_params.format = loaded_image.format;
			rtexture_params.ptr_data = loaded_image.data_ptr;
			rtexture_params.debug_name = ARENA_CHAR_TO_WIDE(arena, filepath);

			ret.render_texture_handle = renderer::create_render_texture(rtexture_params);
		}

		return ret;
	}

	static texture_asset_t load_texture_hdr(memory_arena_t& arena, memory_arena_t& arena_scratch, const char* filepath, bool srgb)
	{
		texture_asset_t ret = {};
		ret.render_texture_handle.handle = INVALID_HANDLE;
		
		// -------------------------------------------------------------------------------------------------------------
		// Load the file
		image_load_result_t loaded_image = {};
		{
			s_stbi_memory_arena = &arena;
			
			int32_t channels;
			loaded_image.data_ptr = (uint8_t*)stbi_loadf(filepath, &loaded_image.width, &loaded_image.height, &channels, STBI_rgb_alpha);
			loaded_image.loaded = loaded_image.data_ptr;
			loaded_image.bits_per_pixel = 128;
			loaded_image.format = TEXTURE_FORMAT_RGBA32_FLOAT;
		}

		// -------------------------------------------------------------------------------------------------------------
		// Upload texture to the GPU
		if (!loaded_image.loaded)
		{
			LOG_ERR("Assets", "Failed to load image: %s", filepath);
			return ret;
		}
		
		{
			renderer::render_texture_params_t rtexture_params = {};
			rtexture_params.width = loaded_image.width;
			rtexture_params.height = loaded_image.height;
			rtexture_params.bits_per_pixel = loaded_image.bits_per_pixel;
			rtexture_params.format = loaded_image.format;
			rtexture_params.ptr_data = loaded_image.data_ptr;
			rtexture_params.debug_name = ARENA_CHAR_TO_WIDE(arena, filepath);

			ret.render_texture_handle = renderer::create_render_texture(rtexture_params);
		}

		return ret;
	}

	static texture_asset_t load_texture_dds(memory_arena_t& arena, memory_arena_t& arena_scratch, const char* filepath, bool srgb)
	{
		texture_asset_t ret = {};
		ret.render_texture_handle.handle = INVALID_HANDLE;

		// -------------------------------------------------------------------------------------------------------------
		// Load the file
		image_load_result_t loaded_image = {};
		{
			fileio::read_file_result_t loaded_file = fileio::read_file(arena_scratch, filepath);
			if (!loaded_file.loaded)
			{
				LOG_ERR("Assets", "Failed to read file: %s", filepath);
				return ret;
			}
			
			uint32_t dds_file_start = 0x20534444;
			if (memcmp(loaded_file.data, &dds_file_start, 4) != 0)
			{
				LOG_ERR("Assets", "Invalid DDS file, file did not start with \"DDS \": %s", filepath);
				return ret;
			}
			
			DDS_HEADER* header = (DDS_HEADER*)(loaded_file.data + 4);
			if (header->size != sizeof(DDS_HEADER) ||
				header->ddspf.size != sizeof(DDS_PIXELFORMAT))
			{
				LOG_ERR("Assets", "DDS header wrong or possibly corrupted: %s");
				return ret;
			}

			DDS_HEADER_DXT10* header_dxt10 = nullptr;
			if (header->ddspf.flags & DDS_FOURCC &&
				header->ddspf.fourCC == MAKEFOURCC('D', 'X', '1', '0'))
			{
				if (loaded_file.size < sizeof(uint32_t) + sizeof(DDS_HEADER) + sizeof(DDS_HEADER_DXT10))
				{
					LOG_ERR("Assets", "DDS file has DXT10 extension, but file is too small");
					return ret;
				}
				
				header_dxt10 = (DDS_HEADER_DXT10*)(header + 1);
			}

			DXGI_FORMAT dxgi_format;
			if (header_dxt10)
			{
				dxgi_format = header_dxt10->dxgiFormat;
			}
			else
			{
				dxgi_format = GetDXGIFormat(&header->ddspf);
			}

			TEXTURE_FORMAT texture_format;
			switch (dxgi_format)
			{
			case DXGI_FORMAT_BC1_UNORM:      texture_format = TEXTURE_FORMAT_BC1;      break;
			case DXGI_FORMAT_BC1_UNORM_SRGB: texture_format = TEXTURE_FORMAT_BC1_SRGB; break;
			case DXGI_FORMAT_BC2_UNORM:      texture_format = TEXTURE_FORMAT_BC2;      break;
			case DXGI_FORMAT_BC2_UNORM_SRGB: texture_format = TEXTURE_FORMAT_BC2_SRGB; break;
			case DXGI_FORMAT_BC3_UNORM:      texture_format = TEXTURE_FORMAT_BC3;      break;
			case DXGI_FORMAT_BC3_UNORM_SRGB: texture_format = TEXTURE_FORMAT_BC3_SRGB; break;
			case DXGI_FORMAT_BC4_UNORM:      texture_format = TEXTURE_FORMAT_BC4;      break;
			case DXGI_FORMAT_BC5_UNORM:      texture_format = TEXTURE_FORMAT_BC5;      break;
			case DXGI_FORMAT_BC7_UNORM:      texture_format = TEXTURE_FORMAT_BC7;      break;
			case DXGI_FORMAT_BC7_UNORM_SRGB: texture_format = TEXTURE_FORMAT_BC7_SRGB; break;
			default:
				{
					LOG_ERR("Assets", "DDS file has an unsupported format");
					return ret;
				} break;
			}

			size_t bits_per_pixel = DDSBitsPerPixel(dxgi_format);
			if (bits_per_pixel == 0)
			{
				LOG_ERR("Assets", "DDS file has 0 bits per pixel");
				return ret;
			}

			if (header_dxt10)
			{
				if (header_dxt10->resourceDimension != 3) // 3 = D3D12_RESOURCE_DIMENSION_TEXTURE2D
				{
					LOG_ERR("Assets", "DDS file is not a texture 2D, only texture 2Ds are currently supported");
					return ret;
				}
			}
			else
			{
				if (header->flags & DDS_HEADER_FLAGS_VOLUME ||
					header->caps2 & DDS_CUBEMAP)
				{
					LOG_ERR("Assets", "DDS file is not a texture 2D, only texture 2Ds are currently supported");
					return ret;
				}
			}

			if (header_dxt10)
			{
				if (header_dxt10->arraySize == 0)
				{
					LOG_ERR("Assets", "DDS array size is 0");
					return ret;
				}

				if (header_dxt10->arraySize > 1)
				{
					LOG_WARN("Assets", "DDS contains more than one texture, only first one will be used");
					return ret;
				}
			}

			if (!IS_POW2(header->width) || !IS_POW2(header->height))
			{
				LOG_ERR("Assets", "DDS resolution is invalid, has to be a power of 2");
				return ret;
			}

			if (header->mipMapCount > 16)
			{
				LOG_ERR("Assets", "DDS has more than 16 mips");
				return ret;
			}

			uint32_t expected_mip_count = log2_u32(MIN(header->width, header->height));
			if (header->mipMapCount > expected_mip_count)
			{
				LOG_ERR("Assets", "DDS has more mips than the resolution would suggest");
				return ret;
			}

			loaded_image.format = texture_format;
			loaded_image.width = (int32_t)header->width;
			loaded_image.height = (int32_t)header->height;
			loaded_image.bits_per_pixel = (int32_t)bits_per_pixel;
			loaded_image.data_ptr = loaded_file.data;
			loaded_image.loaded = true;
		}

		// -------------------------------------------------------------------------------------------------------------
		// Upload texture to the GPU
		if (!loaded_image.loaded)
		{
			LOG_ERR("Assets", "Failed to load image: %s", filepath);
			return ret;
		}

		{
			renderer::render_texture_params_t rtexture_params = {};
			rtexture_params.width = loaded_image.width;
			rtexture_params.height = loaded_image.height;
			rtexture_params.bits_per_pixel = loaded_image.bits_per_pixel;
			rtexture_params.format = loaded_image.format;
			rtexture_params.ptr_data = loaded_image.data_ptr;
			rtexture_params.debug_name = ARENA_CHAR_TO_WIDE(arena, filepath);

			ret.render_texture_handle = renderer::create_render_texture(rtexture_params);
		}
		
		return ret;
	}

	static texture_asset_t load_texture_from_file(memory_arena_t& arena, memory_arena_t& arena_scratch, const char* filepath, bool srgb)
	{
		texture_asset_t ret = {};
		TEXTURE_FILE_TYPE source_format = get_texture_file_type(filepath);

		switch (source_format)
		{
		case TEXTURE_FILE_TYPE_PNG:		ret = load_texture_png(arena, arena_scratch, filepath, srgb); break;
		case TEXTURE_FILE_TYPE_HDR:		ret = load_texture_hdr(arena, arena_scratch, filepath, srgb); break;
		case TEXTURE_FILE_TYPE_DDS:		ret = load_texture_dds(arena, arena_scratch, filepath, srgb); break;
		case TEXTURE_FILE_TYPE_UNKNOWN:	LOG_ERR("Assets", "Tried to load texture with unknown file type: %s", filepath); break;
		}

		return ret;
	}

	texture_asset_t* load_texture(memory_arena_t& arena, const char* filepath, bool srgb)
	{
		texture_asset_t* asset = ARENA_ALLOC_STRUCT_ZERO(arena, texture_asset_t);
		ARENA_SCRATCH_SCOPE()
		{
			*asset = load_texture_from_file(arena, arena_scratch, filepath, srgb);
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

	static texture_asset_t gltf_load_texture(memory_arena_t& arena, const char* filepath, const cgltf_image& gltf_image, bool srgb)
	{
		const char* image_filepath = gltf_get_path(arena, filepath, gltf_image.uri);
		return *load_texture(arena, image_filepath, srgb);
	}

	static texture_asset_t fbx_load_texture(memory_arena_t& arena, const ufbx_texture& fbx_texture, bool srgb)
	{
		return *load_texture(arena, fbx_texture.filename.data, srgb);
	}
	
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

	enum SCENE_MESH_FILE_TYPE : uint8_t
	{
		SCENE_MESH_FILE_TYPE_GLTF,
		SCENE_MESH_FILE_TYPE_FBX,
		SCENE_MESH_FILE_TYPE_UNKNOWN = 255
	};

	static SCENE_MESH_FILE_TYPE get_scene_mesh_file_type(const char* filepath)
	{
		const char* extension_delim = strrchr(filepath, '.');
		if (!extension_delim || extension_delim == filepath)
		{
			return SCENE_MESH_FILE_TYPE_UNKNOWN;
		}

		const char* extension = extension_delim + 1;
		if (strcmp(extension_delim, ".gltf") == 0)
		{
			return SCENE_MESH_FILE_TYPE_GLTF;
		}
		if (strcmp(extension_delim, ".fbx") == 0)
		{
			return SCENE_MESH_FILE_TYPE_FBX;
		}
		
		return SCENE_MESH_FILE_TYPE_UNKNOWN;
	}

	static scene_asset_t load_scene_gltf(memory_arena_t& arena, memory_arena_t& arena_scratch, const char* filepath)
	{
		scene_asset_t ret = {};

		// -------------------------------------------------------------------------------------------------------------
		// Load GLTF file
		cgltf_data* loaded_gltf = nullptr;
		{
			// Parse the GLTF file
			cgltf_options options = {};

			// Make CGLTF use our arena for allocations
			options.memory.user_data = &arena_scratch;
			options.memory.alloc_func = [](void* user, cgltf_size size)
			{
				memory_arena_t* arena = (memory_arena_t*)user;
				return ARENA_ALLOC_ZERO(*arena, size, 16);
			};
			// No-op, freeing is done by the arena automatically
			options.memory.free_func = [](void* user, void* ptr)
			{
				(void)user; (void)ptr;
			};

			cgltf_result result = cgltf_parse_file(&options, filepath, &loaded_gltf);

			if (result != cgltf_result_success)
			{
				FATAL_ERROR("Assets", "Failed to load GLTF: %s", filepath);
			}

			cgltf_load_buffers(&options, loaded_gltf, filepath);
		}

		// -------------------------------------------------------------------------------------------------------------
		// Parse materials/textures and upload to GPU
		{
			// GLTF 2.0 Spec states that:
			// - Base color textures are encoded in SRGB
			// - Normal textures are encoded in linear
			// - Metallic roughness textures are encoded in linear
			// - Emissive textures are encoded in SRGB
			ret.material_assets = ARENA_ALLOC_ARRAY_ZERO(arena, material_asset_t, loaded_gltf->materials_count);

			for (uint32_t mat_idx = 0; mat_idx < loaded_gltf->materials_count; ++mat_idx)
			{
				cgltf_material& gltf_material = loaded_gltf->materials[mat_idx];
				material_asset_t& asset = ret.material_assets[mat_idx];

				asset.base_color_factor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
				asset.metallic_factor = 1.0f;
				asset.roughness_factor = 1.0f;
				asset.emissive_factor = glm::vec3(0.0f, 0.0f, 0.0f);
				asset.emissive_strength = 0.0f;
				asset.base_color_texture.render_texture_handle.handle = INVALID_HANDLE;
				asset.normal_texture.render_texture_handle.handle = INVALID_HANDLE;
				asset.metallic_roughness_texture.render_texture_handle.handle = INVALID_HANDLE;
				asset.emissive_texture.render_texture_handle.handle = INVALID_HANDLE;

				if (gltf_material.has_pbr_metallic_roughness)
				{
					asset.base_color_factor = *(glm::vec4*)gltf_material.pbr_metallic_roughness.base_color_factor;
					asset.metallic_factor = gltf_material.pbr_metallic_roughness.metallic_factor;
					asset.roughness_factor = gltf_material.pbr_metallic_roughness.roughness_factor;
					asset.emissive_factor = *(glm::vec3*)gltf_material.emissive_factor;
					asset.emissive_strength = gltf_material.emissive_strength.emissive_strength;

					if (gltf_material.pbr_metallic_roughness.base_color_texture.texture)
					{
						cgltf_image* gltf_image = gltf_material.pbr_metallic_roughness.base_color_texture.texture->image;
						if (gltf_image)
						{
							asset.base_color_texture = gltf_load_texture(arena_scratch, filepath, *gltf_image, true);
						}
					}

					if (gltf_material.normal_texture.texture)
					{
						cgltf_image* gltf_image = gltf_material.normal_texture.texture->image;
						if (gltf_image)
						{
							asset.normal_texture = gltf_load_texture(arena_scratch, filepath, *gltf_image, false);
						}
					}

					if (gltf_material.pbr_metallic_roughness.metallic_roughness_texture.texture)
					{
						cgltf_image* gltf_image = gltf_material.pbr_metallic_roughness.metallic_roughness_texture.texture->image;
						if (gltf_image)
						{
							asset.metallic_roughness_texture = gltf_load_texture(arena_scratch, filepath, *gltf_image, false);
						}
					}

					if (gltf_material.emissive_texture.texture)
					{
						cgltf_image* gltf_image = gltf_material.emissive_texture.texture->image;
						if (gltf_image)
						{
							asset.emissive_texture = gltf_load_texture(arena_scratch, filepath, *gltf_image, true);
						}
					}
				}

				++ret.material_asset_count;
			}
		}

		// -------------------------------------------------------------------------------------------------------------
		// Parse meshes and upload to GPU
		{
			uint32_t mesh_count = 0;
			for (uint32_t mesh_idx = 0; mesh_idx < loaded_gltf->meshes_count; ++mesh_idx)
			{
				const cgltf_mesh& mesh_gltf = loaded_gltf->meshes[mesh_idx];
				mesh_count += mesh_gltf.primitives_count;
			}

			ret.mesh_assets = ARENA_ALLOC_ARRAY_ZERO(arena, mesh_asset_t, mesh_count);

			for (uint32_t mesh_idx = 0; mesh_idx < loaded_gltf->meshes_count; ++mesh_idx)
			{
				const cgltf_mesh& mesh_gltf = loaded_gltf->meshes[mesh_idx];

				for (uint32_t prim_idx = 0; prim_idx < mesh_gltf.primitives_count; ++prim_idx)
				{
					const cgltf_primitive& prim_gltf = mesh_gltf.primitives[prim_idx];

					uint32_t index_count = prim_gltf.indices->count;
					uint32_t vertex_count = prim_gltf.attributes[0].data->count;

					uint32_t* indices = ARENA_ALLOC_ARRAY(arena_scratch, uint32_t, index_count);
					vertex_t* vertices = ARENA_ALLOC_ARRAY(arena_scratch, vertex_t, vertex_count);
					
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

					uint32_t attr_indices[4] = { UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX };

					for (uint32_t attr_idx = 0; attr_idx < prim_gltf.attributes_count; ++attr_idx)
					{
						const cgltf_attribute& attr_gltf = prim_gltf.attributes[attr_idx];
						ASSERT(attr_gltf.data->count == vertex_count);

						switch (attr_gltf.type)
						{
						case cgltf_attribute_type_position: attr_indices[0] = attr_idx; break;
						case cgltf_attribute_type_normal:	attr_indices[1] = attr_idx; break;
						case cgltf_attribute_type_tangent:	attr_indices[2] = attr_idx; break;
						case cgltf_attribute_type_texcoord: attr_indices[3] = attr_idx; break;
						}
					}

					ASSERT_MSG(attr_indices[0] != UINT32_MAX, "GLTF %s is missing vertex attribute position", filepath);
					ASSERT_MSG(attr_indices[1] != UINT32_MAX, "GLTF %s is missing vertex attribute normal", filepath);
					//ASSERT_MSG(attr_indices[2] != UINT32_MAX, "GLTF %s is missing vertex attribute tangent", filepath);
					ASSERT_MSG(attr_indices[3] != UINT32_MAX, "GLTF %s is missing vertex attribute tex_coord", filepath);

					for (uint32_t vert_idx = 0; vert_idx < vertex_count; ++vert_idx)
					{
						glm::vec3* ptr_pos = cgltf_get_data_ptr<glm::vec3>(prim_gltf.attributes[attr_indices[0]].data);
						glm::vec3* ptr_normal = cgltf_get_data_ptr<glm::vec3>(prim_gltf.attributes[attr_indices[1]].data);
						//glm::vec4* ptr_tangent = cgltf_get_data_ptr<glm::vec4>(prim_gltf.attributes[attr_indices[2]].data);
						glm::vec2* ptr_tex_coord = cgltf_get_data_ptr<glm::vec2>(prim_gltf.attributes[attr_indices[3]].data);

						vertices[vert_idx].position = ptr_pos[vert_idx];
						vertices[vert_idx].normal = ptr_normal[vert_idx];
						//vertices[vert_idx].tangent = ptr_tangent[vert_idx];
						vertices[vert_idx].tex_coord = ptr_tex_coord[vert_idx];
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

		// -------------------------------------------------------------------------------------------------------------
		// Parse nodes and node hierarchy
		{
			uint32_t root_node_count = 0;
			uint32_t node_count = 0;

			for (uint32_t node_idx = 0; node_idx < loaded_gltf->nodes_count; ++node_idx)
			{
				cgltf_node& gltf_node = loaded_gltf->nodes[node_idx];

				if (gltf_node.mesh)
				{
					++node_count;
				}
			}

			ret.nodes = ARENA_ALLOC_ARRAY_ZERO(arena, scene_node_t, node_count);

			for (uint32_t node_idx = 0; node_idx < loaded_gltf->nodes_count; ++node_idx)
			{
				cgltf_node& gltf_node = loaded_gltf->nodes[node_idx];
				if (!gltf_node.mesh)
					continue;

				scene_node_t& scene_node = ret.nodes[node_idx];
				cgltf_float gltf_node_to_world[16];
				cgltf_node_transform_world(&gltf_node, gltf_node_to_world);
				scene_node.node_to_world = *(glm::mat4*)gltf_node_to_world;
				scene_node.mesh_indices = ARENA_ALLOC_ARRAY_ZERO(arena, uint32_t, gltf_node.mesh->primitives_count);
				scene_node.material_indices = ARENA_ALLOC_ARRAY_ZERO(arena, uint32_t, gltf_node.mesh->primitives_count);
				scene_node.mesh_count = gltf_node.mesh->primitives_count;

				scene_node.children = ARENA_ALLOC_ARRAY_ZERO(arena, uint32_t, gltf_node.children_count);
				scene_node.child_count = gltf_node.children_count;
				for (uint32_t i = 0; i < gltf_node.children_count; ++i)
				{
					scene_node.children[i] = cgltf_node_index(loaded_gltf, &loaded_gltf->nodes[node_idx]);
				}

				for (uint32_t i = 0; i < gltf_node.mesh->primitives_count; ++i)
				{
					scene_node.mesh_indices[i] = gltf_get_index<cgltf_primitive>(gltf_node.mesh->primitives, &gltf_node.mesh->primitives[i]);
					scene_node.material_indices[i] = gltf_get_index<cgltf_material>(loaded_gltf->materials, gltf_node.mesh->primitives[i].material);
				}

				++ret.node_count;
			}
		}

		return ret;
	}

	static scene_asset_t load_scene_fbx(memory_arena_t& arena, memory_arena_t& arena_scratch, const char* filepath)
	{
		scene_asset_t ret = {};

		// -------------------------------------------------------------------------------------------------------------
		// Load FBX file
		ufbx_scene* loaded_fbx = nullptr;
		{
			ufbx_allocator_opts temp_alloc = {};
			temp_alloc.allocator.user = &arena_scratch;
			temp_alloc.allocator.alloc_fn = [](void* user, size_t size)
				{
					memory_arena_t* arena = (memory_arena_t*)user;
					return ARENA_ALLOC_ZERO(*arena, size, 8);
				};
			temp_alloc.allocator.realloc_fn = [](void* user, void* old_ptr, size_t old_size, size_t new_size)
				{
					memory_arena_t* arena = (memory_arena_t*)user;
					void* ptr_new = ARENA_ALLOC_ZERO(*arena, new_size, 8);
					memcpy(ptr_new, old_ptr, old_size);
					return ptr_new;
				};
			temp_alloc.allocator.free_fn = [](void* user, void* ptr, size_t size)
				{
					(void)user; (void)ptr; (void)size;
				};
			temp_alloc.allocator.free_allocator_fn = [](void* user)
				{
					(void)user;
				};
			temp_alloc.memory_limit = SIZE_MAX;
			temp_alloc.allocation_limit = SIZE_MAX;
			temp_alloc.huge_threshold = MB(1);
			temp_alloc.max_chunk_size = MB(16);

			ufbx_allocator_opts result_alloc = {};
			result_alloc.allocator.user = &arena_scratch;
			result_alloc.allocator.alloc_fn = [](void* user, size_t size)
				{
					memory_arena_t* arena = (memory_arena_t*)user;
					return ARENA_ALLOC_ZERO(*arena, size, 8);
				};
			result_alloc.allocator.realloc_fn = [](void* user, void* old_ptr, size_t old_size, size_t new_size)
				{
					memory_arena_t* arena = (memory_arena_t*)user;
					void* ptr_new = ARENA_ALLOC_ZERO(*arena, new_size, 8);
					memcpy(ptr_new, old_ptr, old_size);
					return ptr_new;
				};
			result_alloc.allocator.free_fn = [](void* user, void* ptr, size_t size)
				{
					(void)user; (void)ptr; (void)size;
				};
			result_alloc.allocator.free_allocator_fn = [](void* user)
				{
					(void)user;
				};
			result_alloc.memory_limit = SIZE_MAX;
			result_alloc.allocation_limit = SIZE_MAX;
			result_alloc.huge_threshold = MB(1);
			result_alloc.max_chunk_size = MB(16);

			ufbx_load_opts opts = {};
			opts.target_unit_meters = 1.0f;
			//opts.load_external_files = true;
			//opts.ignore_missing_external_files = true;
			opts.generate_missing_normals = true;
			/*opts.normalize_normals = true;
			opts.normalize_tangents = true;*/
			opts.target_axes = {
				.right = UFBX_COORDINATE_AXIS_POSITIVE_X,
				.up = UFBX_COORDINATE_AXIS_POSITIVE_Y,
				.front = UFBX_COORDINATE_AXIS_POSITIVE_Z
			};
			opts.temp_allocator = temp_alloc;
			opts.result_allocator = result_alloc;

			ufbx_error error = {};
			loaded_fbx = ufbx_load_file(filepath, &opts, &error);
			if (!loaded_fbx || error.type != UFBX_ERROR_NONE)
			{
				FATAL_ERROR("Assets", "Failed to load FBX: %s", filepath);
			}
		}

		// -------------------------------------------------------------------------------------------------------------
		// Parse materials/textures and upload to GPU
		{
			ret.material_assets = ARENA_ALLOC_ARRAY_ZERO(arena, material_asset_t, loaded_fbx->materials.count);
			for (uint32_t mat_idx = 0; mat_idx < loaded_fbx->materials.count; ++mat_idx)
			{
				ufbx_material* fbx_material = loaded_fbx->materials[mat_idx];
				material_asset_t& asset = ret.material_assets[mat_idx];

				asset.base_color_factor = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
				asset.metallic_factor = 0.0f;
				asset.roughness_factor = 0.5f;
				asset.emissive_factor = glm::vec3(0.0f, 0.0f, 0.0f);
				asset.emissive_strength = 0.0f;
				asset.base_color_texture.render_texture_handle.handle = INVALID_HANDLE;
				asset.normal_texture.render_texture_handle.handle = INVALID_HANDLE;
				asset.metallic_roughness_texture.render_texture_handle.handle = INVALID_HANDLE;
				asset.emissive_texture.render_texture_handle.handle = INVALID_HANDLE;

				//if (fbx_material->features.pbr.enabled)
				{
					asset.base_color_factor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
					asset.metallic_factor = 1.0f;
					asset.roughness_factor = 1.0f;
					asset.emissive_factor = glm::vec3(0.0f, 0.0f, 0.0f);
					asset.emissive_strength = 0.0f;

					if (fbx_material->pbr.base_color.texture_enabled)
					{
						ufbx_texture* fbx_texture = fbx_material->pbr.base_color.texture;
						if (fbx_texture)
						{
							asset.base_color_texture = fbx_load_texture(arena_scratch, *fbx_texture, true);
						}
					}

					if (fbx_material->pbr.normal_map.texture_enabled)
					{
						ufbx_texture* fbx_texture = fbx_material->pbr.normal_map.texture;
						if (fbx_texture)
						{
							asset.normal_texture = fbx_load_texture(arena_scratch, *fbx_texture, false);
						}
					}

					if (fbx_material->pbr.metalness.texture_enabled || fbx_material->pbr.roughness.texture_enabled)
					{
						ufbx_texture* fbx_texture_metalness = fbx_material->pbr.metalness.texture;
						ufbx_texture* fbx_texture_roughness = fbx_material->pbr.roughness.texture;
						if (fbx_texture_metalness && fbx_texture_roughness && fbx_texture_metalness == fbx_texture_roughness)
						{
							asset.metallic_roughness_texture = fbx_load_texture(arena_scratch, *fbx_texture_metalness, false);
						}
					}
					else if (fbx_material->pbr.specular_color.texture_enabled)
					{
						ufbx_texture* fbx_texture_specular = fbx_material->pbr.specular_color.texture;
						if (fbx_texture_specular)
						{
							asset.metallic_roughness_texture = fbx_load_texture(arena_scratch, *fbx_texture_specular, false);
						}
					}

					if (fbx_material->pbr.emission_color.texture_enabled)
					{
						ufbx_texture* fbx_texture = fbx_material->pbr.emission_color.texture;
						if (fbx_texture)
						{
							asset.emissive_texture = fbx_load_texture(arena_scratch, *fbx_texture, true);
						}
					}
				}

				++ret.material_asset_count;
			}
		}

		// -------------------------------------------------------------------------------------------------------------
		// Parse meshes and upload to GPU
		{
			uint32_t mesh_count = 0;
			uint32_t max_triangles = 0;

			for (uint32_t mesh_idx = 0; mesh_idx < loaded_fbx->meshes.count; ++mesh_idx)
			{
				const ufbx_mesh* fbx_mesh = loaded_fbx->meshes.data[mesh_idx];
				if (fbx_mesh->instances.count == 0 || fbx_mesh->material_parts.count == 0) continue;

				for (uint32_t part_idx = 0; part_idx < fbx_mesh->material_parts.count; ++part_idx)
				{
					const ufbx_mesh_part& fbx_mesh_part = fbx_mesh->material_parts.data[part_idx];
					if (fbx_mesh_part.num_triangles == 0) continue;

					++mesh_count;
					max_triangles = MAX(max_triangles, fbx_mesh_part.num_triangles);
				}
			}

			ret.mesh_assets = ARENA_ALLOC_ARRAY_ZERO(arena, mesh_asset_t, mesh_count);

			for (uint32_t mesh_idx = 0; mesh_idx < loaded_fbx->meshes.count; ++mesh_idx)
			{
				const ufbx_mesh* fbx_mesh = loaded_fbx->meshes.data[mesh_idx];
				if (fbx_mesh->instances.count == 0 || fbx_mesh->material_parts.count == 0) continue;

				ASSERT_MSG(fbx_mesh->vertex_position.exists, "FBX %s is missing vertex attribute position", filepath);
				ASSERT_MSG(fbx_mesh->vertex_normal.exists, "FBX %s is missing vertex attribute normal", filepath);
				//ASSERT_MSG(fbx_mesh->vertex_tangent.exists, "FBX %s is missing vertex attribute tangent", filepath);
				ASSERT_MSG(fbx_mesh->vertex_uv.exists, "FBX %s is missing vertex attribute tex_coord", filepath);

				uint32_t tri_index_count = fbx_mesh->max_face_triangles * 3;
				uint32_t* tri_indices = ARENA_ALLOC_ARRAY_ZERO(arena_scratch, uint32_t, tri_index_count);
				vertex_t* vertices = ARENA_ALLOC_ARRAY_ZERO(arena_scratch, vertex_t, max_triangles * 3);
				uint32_t* indices = ARENA_ALLOC_ARRAY_ZERO(arena_scratch, uint32_t, max_triangles * 3);
				
				for (uint32_t part_idx = 0; part_idx < fbx_mesh->material_parts.count; ++part_idx)
				{
					const ufbx_mesh_part& fbx_mesh_part = fbx_mesh->material_parts.data[part_idx];
					if (fbx_mesh_part.num_triangles == 0) continue;

					uint32_t index_count = 0;

					for (uint32_t face_idx = 0; face_idx < fbx_mesh_part.num_faces; ++face_idx)
					{
						const ufbx_face& fbx_face = fbx_mesh->faces.data[fbx_mesh_part.face_indices.data[face_idx]];
						uint32_t triangle_count = ufbx_triangulate_face(tri_indices, tri_index_count, fbx_mesh, fbx_face);

						for (uint32_t vert_idx = 0; vert_idx < triangle_count * 3; ++vert_idx)
						{
							uint32_t tri_idx = tri_indices[vert_idx];
							
							ufbx_vec3 src_pos = ufbx_get_vertex_vec3(&fbx_mesh->vertex_position, tri_idx);
							ufbx_vec3 src_normal = ufbx_get_vertex_vec3(&fbx_mesh->vertex_normal, tri_idx);
							//ufbx_vec3 src_tangent = ufbx_get_vertex_vec3(&fbx_mesh->vertex_tangent, tri_idx);
							ufbx_vec2 src_tex_coord = ufbx_get_vertex_vec2(&fbx_mesh->vertex_uv, tri_idx);
							
							vertices[index_count].position = glm::vec3(src_pos.x, src_pos.y, src_pos.z);
							vertices[index_count].normal = glm::vec3(src_normal.x, src_normal.y, src_normal.z);
							//vertices[index_count].tangent = glm::vec4(src_tangent.x, src_tangent.y, src_tangent.z, sign);
							vertices[index_count].tex_coord = glm::vec2(src_tex_coord.x, -src_tex_coord.y);

							index_count++;
						}
					}

					ufbx_vertex_stream vertex_stream = {};
					vertex_stream.data = vertices;
					vertex_stream.vertex_count = index_count;
					vertex_stream.vertex_size = sizeof(vertex_t);
					
					uint32_t vertex_count = ufbx_generate_indices(&vertex_stream, 1, indices, index_count, NULL, NULL);

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
		
		// -------------------------------------------------------------------------------------------------------------
		// Parse nodes and node hierarchy
		{
			uint32_t node_count = 0;
			for (uint32_t mesh_idx = 0; mesh_idx < loaded_fbx->meshes.count; ++mesh_idx)
			{
				const ufbx_mesh* fbx_mesh = loaded_fbx->meshes.data[mesh_idx];
				if (fbx_mesh->instances.count == 0 || fbx_mesh->material_parts.count == 0) continue;

				for (uint32_t instance_idx = 0; instance_idx < fbx_mesh->instances.count; ++instance_idx)
				{
					const ufbx_node* fbx_node = fbx_mesh->instances.data[instance_idx];
					if (!fbx_node->visible) continue;
				
					++node_count;
				}
			}
			
			ret.nodes = ARENA_ALLOC_ARRAY_ZERO(arena, scene_node_t, node_count);
			
			uint32_t mesh_part_offset = 0;
			uint32_t node_offset = 0;
			for (uint32_t mesh_idx = 0; mesh_idx < loaded_fbx->meshes.count; ++mesh_idx)
			{
				const ufbx_mesh* fbx_mesh = loaded_fbx->meshes.data[mesh_idx];
				if (fbx_mesh->instances.count == 0 || fbx_mesh->material_parts.count == 0) continue;

				for (uint32_t instance_idx = 0; instance_idx < fbx_mesh->instances.count; ++instance_idx)
				{
					const ufbx_node* fbx_node = fbx_mesh->instances.data[instance_idx];
					if (!fbx_node->visible) continue;

					scene_node_t& scene_node = ret.nodes[node_offset];
					scene_node.node_to_world = glm::mat4(
						fbx_node->node_to_world.m00, fbx_node->node_to_world.m10, fbx_node->node_to_world.m20, 0.0,
						fbx_node->node_to_world.m01, fbx_node->node_to_world.m11, fbx_node->node_to_world.m21, 0.0,
						fbx_node->node_to_world.m02, fbx_node->node_to_world.m12, fbx_node->node_to_world.m22, 0.0,
						fbx_node->node_to_world.m03, fbx_node->node_to_world.m13, fbx_node->node_to_world.m23, 1.0
					);
					scene_node.mesh_indices = ARENA_ALLOC_ARRAY(arena, uint32_t, fbx_mesh->material_parts.count);
					scene_node.material_indices = ARENA_ALLOC_ARRAY(arena, uint32_t, fbx_mesh->material_parts.count);
					scene_node.mesh_count = fbx_mesh->material_parts.count;

					scene_node.children = ARENA_ALLOC_ARRAY(arena, uint32_t, fbx_node->children.count);
					scene_node.child_count = fbx_node->children.count;
					for (uint32_t i = 0; i < fbx_node->children.count; ++i)
					{
						scene_node.children[i] = fbx_node->children.data[i]->typed_id;
					}

					for (uint32_t i = 0; i < fbx_mesh->material_parts.count; ++i)
					{
						scene_node.mesh_indices[i] = mesh_part_offset;
						scene_node.material_indices[i] = fbx_mesh->materials[i]->typed_id;

						++mesh_part_offset;
					}

					++node_offset;
					++ret.node_count;
				}
			}
		}

		ufbx_free_scene(loaded_fbx);
		return ret;
	}

	static scene_asset_t load_scene_from_file(memory_arena_t& arena, memory_arena_t& arena_scratch, const char* filepath)
	{
		scene_asset_t ret = {};
		SCENE_MESH_FILE_TYPE source_format = get_scene_mesh_file_type(filepath);

		switch (source_format)
		{
			case SCENE_MESH_FILE_TYPE_GLTF:		ret = load_scene_gltf(arena, arena_scratch, filepath); break;
			case SCENE_MESH_FILE_TYPE_FBX:		ret = load_scene_fbx(arena, arena_scratch, filepath); break;
			case SCENE_MESH_FILE_TYPE_UNKNOWN:	LOG_ERR("Assets", "Tried to load scene asset with unknown file type: %s", filepath); break;
		}

		return ret;
	}

	scene_asset_t* load_scene(memory_arena_t& arena, const char* filepath)
	{
		scene_asset_t* asset = ARENA_ALLOC_STRUCT_ZERO(arena, scene_asset_t);
		ARENA_SCRATCH_SCOPE()
		{
			*asset = load_scene_from_file(arena, arena_scratch, filepath);
		}
		return asset;
	}

}
