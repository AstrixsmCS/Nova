#pragma once

#include "Core/UUID.hpp"

#include <string>

#include <glm/glm.hpp>

struct IDComponent
{
	UUID ID {0};
};

struct TagComponent
{
	std::string Tag;

	TagComponent() = default;
	TagComponent(const TagComponent& other) = default;
	TagComponent(const std::string& tag)
		: Tag(tag) {}

	operator std::string& () { return Tag; }
	operator const std::string& () const { return Tag; }
};

struct RelationshipComponent
{
	UUID ParentHandle {0};
	std::vector<UUID> Children;

	RelationshipComponent() = default;
	RelationshipComponent(const RelationshipComponent& other) = default;
	RelationshipComponent(UUID parent)
		: ParentHandle(parent) {}
};

struct TransformComponent
{
	glm::vec3 Translation{ 0.0f };
	glm::vec3 Rotation{ 0.0f }; // Euler angles in radians for now
	glm::vec3 Scale{ 1.0f };
};
