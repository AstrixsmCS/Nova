#pragma once

#include "Asset/Asset.hpp"

#include "Renderer/Buffer.hpp"
#include "Renderer/Material.hpp"
#include "Renderer/Texture.hpp"

#include <glm/glm.hpp>

#include <filesystem>
#include <functional>
#include <memory>
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
	uint32_t BaseVertex    = 0;
	uint32_t BaseIndex     = 0;
	uint32_t IndexCount    = 0;
	uint32_t VertexCount   = 0;
	uint32_t MaterialIndex = UINT32_MAX;

	glm::mat4 Transform      { 1.0f }; // baked world transform
	glm::mat4 LocalTransform { 1.0f }; // node local transform

	std::string MeshName;
	std::string NodeName;
};

struct Node
{
	uint32_t              Parent = UINT32_MAX;
	std::vector<uint32_t> Children;
	std::vector<uint32_t> Submeshes;
	std::string           Name;
	glm::mat4             LocalTransform { 1.0f };

	bool IsRoot() const { return Parent == UINT32_MAX; }
};

enum class MeshType : uint8_t
{
	Static  = 0,
	Skinned = 1
};

class Mesh : public Asset
{
public:
	Mesh()  = default;
	~Mesh() { Destroy(); }

	Mesh(const Mesh&)            = delete;
	Mesh& operator=(const Mesh&) = delete;

	static AssetType GetStaticType()        { return AssetType::Mesh; }
	AssetType GetAssetType() const override { return AssetType::Mesh; }

	bool Load(const std::filesystem::path& path);
	void Destroy();

	bool IsValid()   const { return m_VertexBuffer.GetBuffer() != VK_NULL_HANDLE; }
	bool IsStatic()  const { return m_MeshType == MeshType::Static;  }
	bool IsSkinned() const { return m_MeshType == MeshType::Skinned; }

	MeshType GetMeshType() const { return m_MeshType; }

	VkBuffer GetVertexBuffer() const { return m_VertexBuffer.GetBuffer(); }
	VkBuffer GetIndexBuffer()  const { return m_IndexBuffer.GetBuffer();  }

	const std::vector<Submesh>&                    GetSubmeshes()  const { return m_Submeshes;  }
	const std::vector<Node>&                       GetNodes()      const { return m_Nodes;      }
	const std::vector<uint32_t>&                   GetRootNodes()  const { return m_RootNodes;  }
	const std::vector<Material>&                   GetMaterials()  const { return m_Materials;  }
	const std::vector<std::shared_ptr<Texture2D>>& GetTextures()   const { return m_Textures;   }

	const std::string& GetName()      const { return m_Name;      }
	const std::string& GetSceneName() const { return m_SceneName; }

private:
	void BakeTransforms();

	std::string m_Name;
	std::string m_SceneName;

	MeshType m_MeshType = MeshType::Static;

	std::vector<Submesh>                    m_Submeshes;
	std::vector<Node>                       m_Nodes;
	std::vector<uint32_t>                   m_RootNodes;
	std::vector<Material>                   m_Materials;
	std::vector<std::shared_ptr<Texture2D>> m_Textures;

	VertexBuffer m_VertexBuffer;
	IndexBuffer  m_IndexBuffer;
};
