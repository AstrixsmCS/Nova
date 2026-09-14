#pragma once

#include "Buffer.hpp"
#include "Texture.hpp"
#include "Material.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <filesystem>
#include <string>
#include <vector>

struct Vertex
{
	glm::vec3 Position;
	glm::vec3 Normal;
	glm::vec2 TexCoord;
	glm::vec4 Tangent;
};

struct Index
{
	uint32_t V1, V2, V3;
};
static_assert(sizeof(Index) == 3 * sizeof(uint32_t));

struct Submesh
{
	uint32_t BaseVertex  = 0;
	uint32_t VertexCount = 0;
	uint32_t BaseIndex   = 0;
	uint32_t IndexCount  = 0;

	uint32_t MaterialIndex = UINT32_MAX;
};

struct Node
{
	uint32_t Parent = UINT32_MAX;

	std::vector<uint32_t> Children;
	std::vector<uint32_t> Submeshes;

	std::string Name;

	glm::mat4 LocalTransform{ 1.0f };

	bool IsRoot() const { return Parent == UINT32_MAX; }
};

class Mesh
{
public:
	Mesh()  = default;
	~Mesh() { Destroy(); }

	bool Load(const std::filesystem::path& path);
	void Destroy();

	const std::string&           GetName()      const { return m_Name;      }
	const std::string&           GetSceneName() const { return m_SceneName; }
	const std::vector<uint32_t>& GetRootNodes() const { return m_RootNodes; }
	const std::vector<Submesh>&  GetSubmeshes() const { return m_Submeshes; }
	const std::vector<Material>& GetMaterials() const { return m_Materials; }
	const std::vector<Node>&     GetNodes()     const { return m_Nodes;     }

	VkBuffer GetVertexBuffer() const { return m_VertexBuffer.GetBuffer(); }
	VkBuffer GetIndexBuffer()  const { return m_IndexBuffer.GetBuffer();  }

	// Traverses every root in the selected glTF scene depth-first.
	void TraverseNodes(const std::function<void(const Node&, const glm::mat4&)>& callback) const;
private:
	bool GenerateTangents(std::vector<Vertex>& vertices, std::vector<Index>& indices);
private:
	std::string m_Name;
	std::string m_SceneName;

	std::vector<uint32_t> m_RootNodes;
	std::vector<Submesh>  m_Submeshes;
	std::vector<Material> m_Materials;
	std::vector<Node>     m_Nodes;

	std::vector<std::shared_ptr<Texture2D>> m_Textures;

	VertexBuffer m_VertexBuffer;
	IndexBuffer  m_IndexBuffer;
};
