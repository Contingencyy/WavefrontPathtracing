#include "Pch.h"
#include "AssetLoader.h"
#include "Renderer/CPUPathtracer.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"

namespace AssetLoader
{

	TextureAsset LoadImageHDR(const std::filesystem::path& filepath)
	{
		int32_t imageWidth, imageHeight, numChannels;
		float* imageData = stbi_loadf(filepath.string().c_str(), &imageWidth, &imageHeight, &numChannels, STBI_rgb_alpha);

		if (!imageData)
		{
			FATAL_ERROR("AssetLoader::LoadImageHDR", "Failed to load HDR image: %s", filepath.string());
		}

		std::vector<glm::vec4> pixelData(imageWidth * imageHeight);
		TextureAsset textureAsset = {};

		memcpy(pixelData.data(), imageData, imageWidth * imageHeight * numChannels * sizeof(float));
		textureAsset.renderTextureHandle = CPUPathtracer::CreateTexture(imageWidth, imageHeight, pixelData);

		return textureAsset;
	}

	template<typename T>
	static T* CGLTFGetDataPointer(const cgltf_accessor* accessor)
	{
		cgltf_buffer_view* buffer_view = accessor->buffer_view;
		uint8_t* base_ptr = (uint8_t*)(buffer_view->buffer->data);
		base_ptr += buffer_view->offset;
		base_ptr += accessor->offset;

		return (T*)base_ptr;
	}

	SceneAsset LoadGLTF(const std::filesystem::path& filepath)
	{
		SceneAsset sceneAsset = {};
		size_t currentMeshIndex = 0;

		// Parse the GLTF file
		cgltf_options gltfOptions = {};
		cgltf_data* gltfData = nullptr;
		cgltf_result gltfResult = cgltf_parse_file(&gltfOptions, filepath.string().c_str(), &gltfData);

		if (gltfResult != cgltf_result_success)
		{
			FATAL_ERROR("AssetLoader::LoadGLTF", "Failed to load GLTF: %s", filepath.string());
		}

		cgltf_load_buffers(&gltfOptions, gltfData, filepath.string().c_str());

		for (size_t meshIndex = 0; meshIndex < gltfData->meshes_count; ++meshIndex)
		{
			const cgltf_mesh& gltfMesh = gltfData->meshes[meshIndex];

			for (size_t primIndex = 0; primIndex < gltfMesh.primitives_count; ++primIndex)
			{
				const cgltf_primitive& gltfPrim = gltfMesh.primitives[primIndex];

				std::vector<uint32_t> indices(gltfPrim.indices->count);
				std::vector<Vertex> vertices(gltfPrim.attributes[0].data->count);

				if (gltfPrim.indices->component_type == cgltf_component_type_r_32u)
				{
					memcpy(indices.data(), CGLTFGetDataPointer<uint32_t>(gltfPrim.indices), sizeof(uint32_t) * gltfPrim.indices->count);
				}
				else if (gltfPrim.indices->component_type == cgltf_component_type_r_16u)
				{
					uint16_t* indices_ptr = CGLTFGetDataPointer<uint16_t>(gltfPrim.indices);

					for (size_t i = 0; i < gltfPrim.indices->count; ++i)
					{
						indices[i] = indices_ptr[i];
					}
				}

				for (size_t attrIndex = 0; attrIndex < gltfPrim.attributes_count; ++attrIndex)
				{
					const cgltf_attribute& gltfAttr = gltfPrim.attributes[attrIndex];
					ASSERT(gltfAttr.data->count == vertices.size());

					switch (gltfAttr.type)
					{
					case cgltf_attribute_type_position:
					{
						glm::vec3* srcPtr = CGLTFGetDataPointer<glm::vec3>(gltfAttr.data);
						
						for (size_t vertIndex = 0; vertIndex < gltfAttr.data->count; ++vertIndex)
						{
							vertices[vertIndex].position = srcPtr[vertIndex];
						}
					} break;
					case cgltf_attribute_type_normal:
					{
						glm::vec3* srcPtr = CGLTFGetDataPointer<glm::vec3>(gltfAttr.data);

						for (size_t vertIndex = 0; vertIndex < gltfAttr.data->count; ++vertIndex)
						{
							vertices[vertIndex].normal = srcPtr[vertIndex];
						}
					} break;
					}
				}

				// Create render mesh
				sceneAsset.renderMeshHandles.push_back(CPUPathtracer::CreateMesh(vertices, indices));
				currentMeshIndex++;
			}
		}

		cgltf_free(gltfData);
		return sceneAsset;
	}

}
