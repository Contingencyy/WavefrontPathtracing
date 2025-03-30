#include "pch.h"
#include "scene.h"

// TODO: remove and implement a proper asset manager
#include "core/assets/asset_loader.h"

#include "renderer/renderer.h"
#include "renderer/material.h"

#include "imgui/imgui.h"

namespace scene
{

	static void create_scene_object(scene_t& scene, render_mesh_handle_t render_mesh_handle, const material_t& material,
		const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale)
	{
		ASSERT(scene.scene_object_at < scene.scene_object_count);
		scene_object_t* new_object = &scene.scene_objects[scene.scene_object_at];

		new_object->render_mesh_handle = render_mesh_handle;
		new_object->material = material;

		new_object->transform_mat = glm::translate(glm::identity<glm::mat4>(), position);
		new_object->transform_mat = new_object->transform_mat * glm::mat4_cast(glm::quat(glm::radians(rotation)));
		new_object->transform_mat = new_object->transform_mat * glm::scale(glm::identity<glm::mat4>(), scale);

		scene.scene_object_at++;
	}

	void scene::create(scene_t& scene)
	{
		// Scene objects
		scene.scene_object_count = 100;
		scene.scene_object_at = 0;
		scene.scene_objects = ARENA_ALLOC_ARRAY_ZERO(&scene.arena, scene_object_t, scene.scene_object_count);

		// Camera and camera controller
		camera::create(scene.camera, glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, 10.0f, 1.0f), 60.0f);
		camera_controller::create(scene.camera_controller, &scene.camera);

		// Plane
		vertex_t plane_verts[4] = {};
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
		create_scene_object(scene, render_mesh_handle_plane, plane_material, glm::vec3(0.0f, 0.0f, 80.0f), glm::vec3(0.0f), glm::vec3(120.0f));

		// HDR environment map
		//scene.hdr_env = asset_loader::load_image_hdr(&scene.arena, "assets/textures/HDR_Env_Victorian_Hall.hdr");
		//scene.hdr_env = asset_loader::load_image_hdr(&scene.arena, "assets/textures/HDR_Env_St_Peters_Square_Night.hdr");
		scene.hdr_env = asset_loader::load_image_hdr(&scene.arena, "assets/textures/HDR_Env_Country_Club.hdr");

		// Dragon
		scene.dragon_mesh = asset_loader::load_gltf(&scene.arena, "assets/gltf/dragon/DragonAttenuation.gltf");
		// Dragon 1
		//material_t dragon_material = material_t::make_refractive(glm::vec3(1.0f), 0.0f, 1.0f, 1.517f, glm::vec3(0.2f, 0.95f, 0.95f));
		material_t dragon_material = material::make_diffuse(glm::vec3(0.7f, 0.1f, 0.05f));
		create_scene_object(scene, scene.dragon_mesh->render_mesh_handles[1], dragon_material, glm::vec3(-20.0f, 0.0f, 50.0f), glm::vec3(90.0f, 180.0f, 0.0f), glm::vec3(2.0f));

		// Dragon 2
		dragon_material = material::make_diffuse(glm::vec3(0.05f, 0.1f, 0.7f));
		create_scene_object(scene, scene.dragon_mesh->render_mesh_handles[1], dragon_material, glm::vec3(20.0f, 0.0f, 50.0f), glm::vec3(90.0f, 0.0f, 0.0f), glm::vec3(2.5f));

		// Dragon 3
		dragon_material = material::make_diffuse(glm::vec3(0.1f, 0.7f, 0.1f));
		create_scene_object(scene, scene.dragon_mesh->render_mesh_handles[1], dragon_material, glm::vec3(-30.0f, 0.0f, 70.0f), glm::vec3(90.0f, 180.0f, 0.0f), glm::vec3(3.0f));

		// Dragon 4
		dragon_material = material::make_diffuse(glm::vec3(0.5f, 0.5f, 0.1f));
		create_scene_object(scene, scene.dragon_mesh->render_mesh_handles[1], dragon_material, glm::vec3(30.0f, 0.0f, 70.0f), glm::vec3(90.0f, 0.0f, 0.0f), glm::vec3(3.5f));

		// Dragon 5
		dragon_material = material::make_specular(glm::vec3(0.8f, 0.7f, 0.2f), 1.0f);
		create_scene_object(scene, scene.dragon_mesh->render_mesh_handles[1], dragon_material, glm::vec3(0.0f, 0.0f, 120.0f), glm::vec3(90.0f, 0.0f, 0.0f), glm::vec3(4.0f));
	}

	void scene::destroy(scene_t& scene)
	{
		ARENA_RELEASE(&scene.arena);
	}

	void scene::update(scene_t& scene, float dt)
	{
		camera_controller::update(scene.camera_controller, dt);
	}

	void scene::render(scene_t& scene)
	{
		renderer::begin_scene(scene.camera, scene.hdr_env->render_texture_handle);

		// Submit every object that needs to be rendered
		for (uint32_t i = 0; i < scene.scene_object_at; ++i)
		{
			const scene_object_t* object = &scene.scene_objects[i];
			renderer::submit_render_mesh(object->render_mesh_handle, object->transform_mat, object->material);
		}

		renderer::render();
		renderer::end_scene();
	}

	void scene::render_ui(scene_t& scene)
	{
	}

}
