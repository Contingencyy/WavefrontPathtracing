#include "pch.h"
#include "scene.h"
// TODO: remove and implement a proper asset manager
#include "core/assets/asset_loader.h"
#include "renderer/shaders/shared.hlsl.h"
#include "renderer/renderer.h"

#include "imgui/imgui.h"

#define SCENE_SPONZA 0
#define SCENE_SUN_TEMPLE 1
#define SCENE_BISTRO 0

namespace scene
{

	static void create_scene_objects_from_scene_asset(scene_t& scene, const scene_asset_t& scene_asset, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale)
	{
		glm::mat4 mat_translation = glm::translate(glm::identity<glm::mat4>(), position);
		glm::mat4 mat_rotation = glm::mat4_cast(glm::quat(glm::radians(rotation)));
		glm::mat4 mat_scale = glm::scale(glm::identity<glm::mat4>(), scale);

		glm::mat4 object_transform = mat_translation * mat_rotation * mat_scale;

		for (uint32_t node_idx = 0; node_idx < scene_asset.node_count; ++node_idx)
		{
			const scene_node_t& node = scene_asset.nodes[node_idx];

			for (uint32_t mesh_idx = 0; mesh_idx < node.mesh_count; ++mesh_idx)
			{
				ASSERT(scene.scene_object_at < scene.scene_object_count);
				scene_object_t* new_object = &scene.scene_objects[scene.scene_object_at];

				new_object->transform_mat = object_transform * node.node_to_world;
				new_object->mesh = &scene_asset.mesh_assets[node.mesh_indices[mesh_idx]];
				new_object->material = &scene_asset.material_assets[node.material_indices[mesh_idx]];

				++scene.scene_object_at;
			}
		}
	}

	void create(scene_t& scene)
	{
		// Scene objects
		scene.scene_object_count = 16384;
		scene.scene_object_at = 0;
		scene.scene_objects = ARENA_ALLOC_ARRAY_ZERO(scene.arena, scene_object_t, scene.scene_object_count);

		// Camera and camera controller
		camera::create(scene.camera, glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, 10.0f, 1.0f), 60.0f);
		camera_controller::create(scene.camera_controller, &scene.camera);

#if SCENE_SPONZA
		//scene.hdr_env = asset_loader::load_texture(scene.arena, "assets/textures/HDR_Env_Victorian_Hall.hdr");
		//scene.hdr_env = asset_loader::load_texture(scene.arena, "assets/textures/HDR_Env_St_Peters_Square_Night.hdr");
		scene.hdr_env = asset_loader::load_texture(scene.arena, "assets/textures/HDR_Env_Country_Club.hdr");
		scene.sponza_scene_asset = asset_loader::load_scene(scene.arena, "assets/scenes/sponza/Sponza.gltf");
		create_scene_objects_from_scene_asset(scene, *scene.sponza_scene_asset, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f));
#endif

#if SCENE_SUN_TEMPLE
		//scene.hdr_env = asset_loader::load_texture(scene.arena, "assets/textures/HDR_Env_Victorian_Hall.hdr");
		//scene.hdr_env = asset_loader::load_texture(scene.arena, "assets/textures/HDR_Env_St_Peters_Square_Night.hdr");
		scene.hdr_env = asset_loader::load_texture(scene.arena, "assets/textures/HDR_Env_Country_Club.hdr");
		scene.sun_temple_scene_asset = asset_loader::load_scene(scene.arena, "assets/scenes/sun_temple/SunTemple.fbx");
		create_scene_objects_from_scene_asset(scene, *scene.sun_temple_scene_asset, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f));
#endif

#if SCENE_BISTRO
		scene.hdr_env = asset_loader::load_texture(scene.arena, "assets/scenes/bistro/san_giuseppe_bridge_4k.hdr");
		
		scene.bistro_exterior_scene_asset = asset_loader::load_scene(scene.arena, "assets/scenes/bistro/BistroExterior.fbx");
		create_scene_objects_from_scene_asset(scene, *scene.bistro_exterior_scene_asset, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f));
		scene.bistro_interior_scene_asset = asset_loader::load_scene(scene.arena, "assets/scenes/bistro/BistroInterior.fbx");
		create_scene_objects_from_scene_asset(scene, *scene.bistro_interior_scene_asset, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f));
		scene.bistro_wine_scene_asset = asset_loader::load_scene(scene.arena, "assets/scenes/bistro/BistroInterior_Wine.fbx");
		create_scene_objects_from_scene_asset(scene, *scene.bistro_wine_scene_asset, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(1.0f));
#endif
	}

	void destroy(scene_t& scene)
	{
		ARENA_RELEASE(scene.arena);
	}

	void update(scene_t& scene, float dt)
	{
		camera_controller::update(scene.camera_controller, dt);
	}

	void render(scene_t& scene)
	{
		renderer::begin_scene(scene.camera, scene.hdr_env->render_texture_handle);

		// Submit every object that needs to be rendered
		for (uint32_t i = 0; i < scene.scene_object_at; ++i)
		{
			const scene_object_t* object = &scene.scene_objects[i];
			renderer::submit_render_mesh(object->mesh->render_mesh_handle, object->transform_mat, *object->material);
		}

		renderer::render();
		renderer::end_scene();
	}

	void render_ui(scene_t& scene)
	{
	}

}
