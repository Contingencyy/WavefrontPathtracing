#pragma once

struct Ray
{
	glm::vec3 origin = {};
	glm::vec3 dir = {};
	glm::vec3 invDir = {};
	float t = 0.0f;
};

struct Triangle
{
	glm::vec3 p0 = {};
	glm::vec3 p1 = {};
	glm::vec3 p2 = {};
};

struct Sphere
{
	glm::vec3 center = {};
	float radiusSquared = 1.0f;
};

struct Plane
{
	glm::vec3 point = {};
	glm::vec3 normal = {};
};

struct AABB
{
	glm::vec3 pmin = {};
	glm::vec3 pmax = {};
};
