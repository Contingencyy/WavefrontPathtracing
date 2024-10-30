#include "pch.h"
#include "scene.h"

// TODO: remove and implement a proper asset manager
#include "core/assets/asset_loader.h"

#include "renderer/renderer.h"
#include "renderer/material.h"

#include "imgui/imgui.h"

void scene_t::init()
{
	// scene_t objects
	m_scene_object_count = 100;
	m_scene_object_at = 0;
	m_scene_objects = ARENA_ALLOC_ARRAY_ZERO(&m_arena, scene_object_t, m_scene_object_count);

	// camera_t Controller
	m_camera_controller = camera_controller_t(camera_t(
		glm::vec3(0.0f, 10.0f, 0.0f), // Eye position
		glm::vec3(0.0f, 10.0f, 1.0f), // Look at position
		60.0f // Vertical FOV in degrees
	));

	// plane_t
	vertex_t plane_verts[4] = {};
	plane_verts[0].position = glm::vec3(-1.0f, 0.0f, 1.0f);
	plane_verts[1].position = glm::vec3(1.0f, 0.0f, 1.0f);
	plane_verts[2].position = glm::vec3(1.0f, 0.0f, -1.0f);
	plane_verts[3].position = glm::vec3(-1.0f, 0.0f, -1.0f);
	plane_verts[0].normal = plane_verts[1].normal = plane_verts[2].normal = plane_verts[3].normal = glm::vec3(0.0f, 1.0f, 0.0f);

	u32 plane_indices[6] = { 0, 1, 2, 2, 3, 0 };
	render_mesh_handle_t render_mesh_handle_plane = renderer::create_render_mesh(plane_verts, ARRAY_SIZE(plane_verts), plane_indices, ARRAY_SIZE(plane_indices));

	material_t plane_material = material::make_diffuse(glm::vec3(1.0f));
	create_scene_object(render_mesh_handle_plane, plane_material, glm::vec3(0.0f, 0.0f, 80.0f), glm::vec3(0.0f), glm::vec3(120.0f));

	// HDR environment map
	//m_hdr_env_texture_asset = asset_loader::load_image_hdr(&m_arena, "assets/textures/HDR_Env_Victorian_Hall.hdr");
	//m_hdr_env_texture_asset = asset_loader::load_image_hdr(&m_arena, "assets/textures/HDR_Env_St_Peters_Square_Night.hdr");
	m_hdr_env_texture_asset = asset_loader::load_image_hdr(&m_arena, "assets/textures/HDR_Env_Country_Club.hdr");

	// Dragon
	m_dragon_scene_asset = asset_loader::load_gltf(&m_arena, "assets/gltf/dragon/DragonAttenuation.gltf");
	// Dragon 1
	//material_t dragon_material = material_t::make_refractive(glm::vec3(1.0f), 0.0f, 1.0f, 1.517f, glm::vec3(0.2f, 0.95f, 0.95f));
	material_t dragon_material = material::make_diffuse(glm::vec3(0.9f, 0.1f, 0.05f));
	create_scene_object(m_dragon_scene_asset->render_mesh_handles[1], dragon_material, glm::vec3(-15.0f, 0.0f, 40.0f), glm::vec3(90.0f, 180.0f, 0.0f), glm::vec3(1.0f));

	// Dragon 2
	dragon_material = material::make_diffuse(glm::vec3(0.05f, 0.1f, 0.9f));
	create_scene_object(m_dragon_scene_asset->render_mesh_handles[1], dragon_material, glm::vec3(15.0f, 0.0f, 40.0f), glm::vec3(90.0f, 0.0f, 0.0f), glm::vec3(2.0f));

	// Dragon 3
	dragon_material = material::make_diffuse(glm::vec3(0.1f, 0.9f, 0.1f));
	create_scene_object(m_dragon_scene_asset->render_mesh_handles[1], dragon_material, glm::vec3(-30.0f, 0.0f, 70.0f), glm::vec3(90.0f, 180.0f, 0.0f), glm::vec3(3.0f));

	// Dragon 4
	dragon_material = material::make_diffuse(glm::vec3(0.9f, 0.9f, 0.1f));
	create_scene_object(m_dragon_scene_asset->render_mesh_handles[1], dragon_material, glm::vec3(30.0f, 0.0f, 70.0f), glm::vec3(90.0f, 0.0f, 0.0f), glm::vec3(4.0f));

	// Dragon 5
	dragon_material = material::make_specular(glm::vec3(0.8f, 0.7f, 0.2f), 1.0f);
	create_scene_object(m_dragon_scene_asset->render_mesh_handles[1], dragon_material, glm::vec3(0.0f, 0.0f, 120.0f), glm::vec3(90.0f, 0.0f, 0.0f), glm::vec3(5.0f));
}

void scene_t::destroy()
{
	ARENA_RELEASE(&m_arena);
}

void scene_t::update(f32 DeltaTime)
{
	m_camera_controller.update(DeltaTime);
}

void scene_t::render()
{
	renderer::begin_scene(m_camera_controller.get_camera(), m_hdr_env_texture_asset->render_texture_handle);

	// Submit every object that needs to be rendered
	for (u32 i = 0; i < m_scene_object_at; ++i)
	{
		const scene_object_t* object = &m_scene_objects[i];
		renderer::submit_render_mesh(object->render_mesh_handle, object->transform_mat, object->material);
	}

	renderer::render();
	renderer::end_scene();
}

void scene_t::render_ui()
{
}

void scene_t::create_scene_object(render_mesh_handle_t render_mesh_handle, const material_t& material, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale)
{
	ASSERT(m_scene_object_at < m_scene_object_count);
	scene_object_t* new_object = &m_scene_objects[m_scene_object_at];

	new_object->render_mesh_handle = render_mesh_handle;
	new_object->material = material;

	new_object->transform_mat = glm::translate(glm::identity<glm::mat4>(), position);
	new_object->transform_mat = new_object->transform_mat * glm::mat4_cast(glm::quat(glm::radians(rotation)));
	new_object->transform_mat = new_object->transform_mat * glm::scale(glm::identity<glm::mat4>(), scale);

	m_scene_object_at++;
}
