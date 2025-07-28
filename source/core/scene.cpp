#include "pch.h"
#include "scene.h"
// TODO: remove and implement a proper asset manager
#include "core/assets/asset_loader.h"
#include "renderer/shaders/shared.hlsl.h"
#include "renderer/renderer.h"

#include "imgui/imgui.h"

namespace scene
{

	static void create_scene_object_from_scene_node(scene_t& scene, const scene_asset_t& asset, const scene_node_t& node, const glm::mat4& node_transform)
	{
		for (uint32_t i = 0; i < node.mesh_count; ++i)
		{
			ASSERT(scene.scene_object_at < scene.scene_object_count);
			scene_object_t* new_object = &scene.scene_objects[scene.scene_object_at];

			new_object->transform_mat = node_transform;
			new_object->mesh = &asset.mesh_assets[node.mesh_indices[i]];
			new_object->material = &asset.material_assets[node.material_indices[i]];

			++scene.scene_object_at;
		}

		for (uint32_t i = 0; i < node.child_count; ++i)
		{
			const scene_node_t& child_node = asset.nodes[node.children[i]];
			glm::mat4 child_transform = node_transform * child_node.transform;

			create_scene_object_from_scene_node(scene, asset, child_node, child_transform);
		}
	}

	static void create_scene_objects_from_scene_asset(scene_t& scene, const scene_asset_t& scene_asset, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale)
	{
		glm::mat4 transform = glm::translate(glm::identity<glm::mat4>(), position);
		transform = transform * glm::mat4_cast(glm::quat(glm::radians(rotation)));
		transform = transform * glm::scale(glm::identity<glm::mat4>(), scale);

		for (uint32_t i = 0; i < scene_asset.root_node_count; ++i)
		{
			uint32_t root_node_index = scene_asset.root_node_indices[i];
			const scene_node_t& root_node = scene_asset.nodes[root_node_index];
			glm::mat4 root_transform = transform * root_node.transform;

			create_scene_object_from_scene_node(scene, scene_asset, root_node, root_transform);
		}
	}

	void create(scene_t& scene)
	{
		// Scene objects
		scene.scene_object_count = 1024;
		scene.scene_object_at = 0;
		scene.scene_objects = ARENA_ALLOC_ARRAY_ZERO(scene.arena, scene_object_t, scene.scene_object_count);

		// Camera and camera controller
		camera::create(scene.camera, glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, 10.0f, 1.0f), 60.0f);
		camera_controller::create(scene.camera_controller, &scene.camera);

		// Plane
		/*vertex_t plane_verts[4] = {};
		plane_verts[0].position = glm::vec3(-1.0f, 0.0f, 1.0f);
		plane_verts[1].position = glm::vec3(1.0f, 0.0f, 1.0f);
		plane_verts[2].position = glm::vec3(1.0f, 0.0f, -1.0f);
		plane_verts[3].position = glm::vec3(-1.0f, 0.0f, -1.0f);
		plane_verts[0].normal = plane_verts[1].normal = plane_verts[2].normal = plane_verts[3].normal = glm::vec3(0.0f, 1.0f, 0.0f);

		uint32_t plane_indices[6] = { 0, 1, 2, 2, 3, 0 };
		renderer::render_mesh_params_t rmesh_params = {};
		rmesh_params.vertex_count = ARRAY_SIZE(plane_verts);
		rmesh_params.vertices = plane_verts;
		rmesh_params.index_count = ARRAY_SIZE(plane_indices);
		rmesh_params.indices = plane_indices;
		rmesh_params.debug_name = WSTRING_LITERAL(L"Plane Mesh");

		render_mesh_handle_t render_mesh_handle_plane = renderer::create_render_mesh(rmesh_params);
		material_t plane_material = material::make_diffuse(glm::vec3(0.5f));
		create_scene_object(scene, render_mesh_handle_plane, plane_material, glm::vec3(0.0f, 0.0f, 80.0f), glm::vec3(0.0f), glm::vec3(120.0f));*/

		// HDR environment map
		//scene.hdr_env = asset_loader::load_texture_hdr(scene.arena, "assets/textures/HDR_Env_Victorian_Hall.hdr", TEXTURE_FORMAT_RGBA32_FLOAT);
		scene.hdr_env = asset_loader::load_texture_hdr(scene.arena, "assets/textures/HDR_Env_St_Peters_Square_Night.hdr", TEXTURE_FORMAT_RGBA32_FLOAT);
		//scene.hdr_env = asset_loader::load_texture_hdr(scene.arena, "assets/textures/HDR_Env_Country_Club.hdr", TEXTURE_FORMAT_RGBA32_FLOAT);

		// Dragon asset and objects
		/*scene.dragon_scene_asset = asset_loader::load_gltf(scene.arena, "assets/gltf/dragon/DragonAttenuation.gltf");
		create_scene_objects_from_scene_asset(scene, *scene.dragon_scene_asset, glm::vec3(-20.0f, 0.0f, 50.0f), glm::vec3(90.0f, 180.0f, 0.0f), glm::vec3(2.0f));
		create_scene_objects_from_scene_asset(scene, *scene.dragon_scene_asset, glm::vec3(20.0f, 0.0f, 50.0f), glm::vec3(90.0f, 0.0f, 0.0f), glm::vec3(2.5f));
		create_scene_objects_from_scene_asset(scene, *scene.dragon_scene_asset, glm::vec3(-30.0f, 0.0f, 70.0f), glm::vec3(90.0f, 180.0f, 0.0f), glm::vec3(3.0f));
		create_scene_objects_from_scene_asset(scene, *scene.dragon_scene_asset, glm::vec3(30.0f, 0.0f, 70.0f), glm::vec3(90.0f, 0.0f, 0.0f), glm::vec3(3.5f));
		create_scene_objects_from_scene_asset(scene, *scene.dragon_scene_asset, glm::vec3(0.0f, 0.0f, 120.0f), glm::vec3(90.0f, 0.0f, 0.0f), glm::vec3(4.0f));*/

		// Sponza
		scene.sponza_scene_asset = asset_loader::load_gltf(scene.arena, "assets/gltf/sponza/Sponza.gltf");
		create_scene_objects_from_scene_asset(scene, *scene.sponza_scene_asset, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(100.0f));
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
