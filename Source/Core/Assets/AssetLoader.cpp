#include "Pch.h"
#include "AssetLoader.h"
#include "Renderer/CPUPathtracer.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"

namespace AssetLoader
{

	TextureAsset* LoadImageHDR(MemoryArena* Arena, const char* Filepath)
	{
		i32 ImageWidth, ImageHeight, ChannelCount;
		f32* PtrImageData = stbi_loadf(Filepath, &ImageWidth, &ImageHeight, &ChannelCount, STBI_rgb_alpha);

		if (!PtrImageData)
		{
			FATAL_ERROR("AssetLoader::LoadImageHDR", "Failed to load HDR image: %s", Filepath);
		}

		TextureAsset* Asset = ARENA_ALLOC_STRUCT(Arena, TextureAsset);
		Asset->RTextureHandle = CPUPathtracer::CreateTexture(ImageWidth, ImageHeight, PtrImageData);

		return Asset;
	}

	template<typename T>
	static T* CGLTFGetDataPointer(const cgltf_accessor* Accessor)
	{
		cgltf_buffer_view* BufferView = Accessor->buffer_view;
		u8* PtrBase = (u8*)(BufferView->buffer->data);
		PtrBase += BufferView->offset;
		PtrBase += Accessor->offset;

		return (T*)PtrBase;
	}

	SceneAsset* LoadGLTF(MemoryArena* Arena, const char* Filepath)
	{
		// Parse the GLTF file
		cgltf_options CGLTFOptions = {};
		cgltf_data* PtrCGLTFData = nullptr;
		cgltf_result CGLTFResult = cgltf_parse_file(&CGLTFOptions, Filepath, &PtrCGLTFData);

		if (CGLTFResult != cgltf_result_success)
		{
			FATAL_ERROR("AssetLoader::LoadGLTF", "Failed to load GLTF: %s", Filepath);
		}

		cgltf_load_buffers(&CGLTFOptions, PtrCGLTFData, Filepath);

		u32 MeshCount = 0;
		for (u32 MeshIdx = 0; MeshIdx < PtrCGLTFData->meshes_count; ++MeshIdx)
		{
			const cgltf_mesh& CGLTFMesh = PtrCGLTFData->meshes[MeshIdx];
			MeshCount += CGLTFMesh.primitives_count;
		}

		SceneAsset* Asset = ARENA_ALLOC_STRUCT(Arena, SceneAsset);
		Asset->RMeshHandles = ARENA_ALLOC_ARRAY_ZERO(Arena, RenderMeshHandle, MeshCount);

		// Use a temporary memory scope here since CPUPathtracer::CreateMesh is blocking, so we can do the scope inside the loop
		ARENA_MEMORY_SCOPE(Arena)
		{
			for (u32 MeshIdx = 0; MeshIdx < PtrCGLTFData->meshes_count; ++MeshIdx)
			{
				const cgltf_mesh& CGLTFMesh = PtrCGLTFData->meshes[MeshIdx];

				for (u32 PrimIdx = 0; PrimIdx < CGLTFMesh.primitives_count; ++PrimIdx)
				{
					const cgltf_primitive& CGLTFPrim = CGLTFMesh.primitives[PrimIdx];

					u32 IndexCount = CGLTFPrim.indices->count;
					u32 VertexCount = CGLTFPrim.attributes[0].data->count;

					u32* Indices = ARENA_ALLOC_ARRAY(Arena, u32, IndexCount);
					Vertex* Vertices = ARENA_ALLOC_ARRAY(Arena, Vertex, VertexCount);

					if (CGLTFPrim.indices->component_type == cgltf_component_type_r_32u)
					{
						memcpy(Indices, CGLTFGetDataPointer<u32>(CGLTFPrim.indices), sizeof(u32) * CGLTFPrim.indices->count);
					}
					else if (CGLTFPrim.indices->component_type == cgltf_component_type_r_16u)
					{
						u16* PtrSrc = CGLTFGetDataPointer<u16>(CGLTFPrim.indices);

						for (u32 Index = 0; Index < CGLTFPrim.indices->count; ++Index)
						{
							Indices[Index] = PtrSrc[Index];
						}
					}

					for (u32 AttribIdx = 0; AttribIdx < CGLTFPrim.attributes_count; ++AttribIdx)
					{
						const cgltf_attribute& CGLTFAttrib = CGLTFPrim.attributes[AttribIdx];
						ASSERT(CGLTFAttrib.data->count == VertexCount);

						switch (CGLTFAttrib.type)
						{
						case cgltf_attribute_type_position:
						{
							glm::vec3* PtrSrc = CGLTFGetDataPointer<glm::vec3>(CGLTFAttrib.data);

							for (u32 VertIdx = 0; VertIdx < CGLTFAttrib.data->count; ++VertIdx)
							{
								Vertices[VertIdx].Position = PtrSrc[VertIdx];
							}
						} break;
						case cgltf_attribute_type_normal:
						{
							glm::vec3* PtrSrc = CGLTFGetDataPointer<glm::vec3>(CGLTFAttrib.data);

							for (u32 VertIdx = 0; VertIdx < CGLTFAttrib.data->count; ++VertIdx)
							{
								Vertices[VertIdx].Normal = PtrSrc[VertIdx];
							}
						} break;
						}
					}

					// Create render mesh
					Asset->RMeshHandles[Asset->MeshHandleCount] = CPUPathtracer::CreateMesh(Vertices, VertexCount, Indices, IndexCount);
					Asset->MeshHandleCount++;
				}
			}
		}

		cgltf_free(PtrCGLTFData);
		return Asset;
	}

}
