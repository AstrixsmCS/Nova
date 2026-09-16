#pragma once

#include "Core/UUID.hpp"

#include "Renderer/Mesh.hpp"

#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

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

	glm::mat4 GetWorldTransform() const
	{
		return glm::translate(glm::mat4(1.0f), Translation) * glm::eulerAngleXYZ(Rotation.x, Rotation.y, Rotation.z) * glm::scale(glm::mat4(1.0f), Scale);
	}
};

struct MeshComponent
{
	std::shared_ptr<Mesh> MeshAsset;

	MeshComponent() = default;
	explicit MeshComponent(std::shared_ptr<Mesh> mesh) : MeshAsset(std::move(mesh)) {}

	bool IsValid() const { return MeshAsset != nullptr && MeshAsset->IsValid(); }
};
