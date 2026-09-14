#include "Mesh.hpp"

#include "Allocator.hpp"

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>
#include <mikktspace.h>

#include <print>
#include <cassert>

// Helpers
static glm::mat4 NodeToMatrix(const fastgltf::Node& node)
{
	if (const auto* trs = std::get_if<fastgltf::TRS>(&node.transform))
	{
		const glm::vec3 translation = glm::make_vec3(trs->translation.data());
		const glm::quat rotation    = glm::make_quat(trs->rotation.data());
		const glm::vec3 scale       = glm::make_vec3(trs->scale.data());

		return glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1.0f), scale);
	}

	if (const auto* mat = std::get_if<fastgltf::math::fmat4x4>(&node.transform))
		return glm::make_mat4(mat->data());

	return glm::mat4(1.0f);
}

// MikkTSpace
struct MikkTSpaceUserData
{
	std::vector<Vertex>&   Vertices;
	std::vector<Index>&    Indices;
	std::vector<glm::vec4> Tangents;
};

static uint32_t CornerIndex(const Index& index, int vertex)
{
	switch (vertex)
	{
		case 0: return index.V1;
		case 1: return index.V2;
		case 2: return index.V3;
		default: return 0;
	}
}

static int MikkGetNumFaces(const SMikkTSpaceContext* context)
{
	return static_cast<int>(static_cast<const MikkTSpaceUserData*>(context->m_pUserData)->Indices.size());
}

static int MikkGetNumVerticesOfFace(const SMikkTSpaceContext*, int)
{
	return 3;
}

static void MikkGetPosition(const SMikkTSpaceContext* context, float out[], int iFace, int iVert)
{
	const auto*      data = static_cast<const MikkTSpaceUserData*>(context->m_pUserData);
	const glm::vec3& pos  = data->Vertices[CornerIndex(data->Indices[iFace], iVert)].Position;
	out[0] = pos.x; out[1] = pos.y; out[2] = pos.z;
}

static void MikkGetNormal(const SMikkTSpaceContext* context, float out[], int iFace, int iVert)
{
	const auto*      data = static_cast<const MikkTSpaceUserData*>(context->m_pUserData);
	const glm::vec3& n    = data->Vertices[CornerIndex(data->Indices[iFace], iVert)].Normal;
	out[0] = n.x; out[1] = n.y; out[2] = n.z;
}

static void MikkGetTexCoord(const SMikkTSpaceContext* context, float out[], int iFace, int iVert)
{
	const auto*      data = static_cast<const MikkTSpaceUserData*>(context->m_pUserData);
	const glm::vec2& uv   = data->Vertices[CornerIndex(data->Indices[iFace], iVert)].TexCoord;
	out[0] = uv.x; out[1] = uv.y;
}

static void MikkSetTSpaceBasic(const SMikkTSpaceContext* context, const float tangent[], float sign, int iFace, int iVert)
{
	auto* data = static_cast<MikkTSpaceUserData*>(context->m_pUserData);
	data->Tangents[iFace * 3 + iVert] = { tangent[0], tangent[1], tangent[2], sign };
}

bool Mesh::GenerateTangents(std::vector<Vertex>& vertices, std::vector<Index>& indices)
{
	if (vertices.empty() || indices.empty())
		return false;

	MikkTSpaceUserData userData
	{
		.Vertices = vertices,
		.Indices  = indices,
		.Tangents = std::vector<glm::vec4>(indices.size() * 3)
	};

	SMikkTSpaceInterface iface{};
	iface.m_getNumFaces          = MikkGetNumFaces;
	iface.m_getNumVerticesOfFace = MikkGetNumVerticesOfFace;
	iface.m_getPosition          = MikkGetPosition;
	iface.m_getNormal            = MikkGetNormal;
	iface.m_getTexCoord          = MikkGetTexCoord;
	iface.m_setTSpaceBasic       = MikkSetTSpaceBasic;
	iface.m_setTSpace            = nullptr;

	SMikkTSpaceContext context{};
	context.m_pInterface = &iface;
	context.m_pUserData  = &userData;

	if (!genTangSpaceDefault(&context))
	{
		std::println("[Mesh] MikkTSpace tangent generation failed");
		return false;
	}

	std::vector<Vertex> newVertices;
	std::vector<Index>  newIndices;

	newVertices.reserve(indices.size() * 3);
	newIndices.reserve(indices.size());

	for (size_t face = 0; face < indices.size(); face++)
	{
		Index    newIndex{};
		uint32_t* dst[] = { &newIndex.V1, &newIndex.V2, &newIndex.V3 };

		for (int v = 0; v < 3; v++)
		{
			Vertex vertex  = vertices[CornerIndex(indices[face], v)];
			vertex.Tangent = userData.Tangents[face * 3 + v];
			*dst[v]        = static_cast<uint32_t>(newVertices.size());
			newVertices.push_back(vertex);
		}

		newIndices.push_back(newIndex);
	}

	vertices = std::move(newVertices);
	indices  = std::move(newIndices);

	return true;
}

// Mesh
bool Mesh::Load(const std::filesystem::path& path)
{
	if (!std::filesystem::exists(path))
	{
		std::println("[Mesh] File not found: {}", path.string());
		return false;
	}

	constexpr fastgltf::Options options =
		fastgltf::Options::GenerateMeshIndices |
		fastgltf::Options::DecomposeNodeMatrices |
		fastgltf::Options::LoadExternalBuffers;

	fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_transmission);

	auto dataResult = fastgltf::GltfDataBuffer::FromPath(path);
	if (dataResult.error() != fastgltf::Error::None)
	{
		std::println("[Mesh] Failed to read file '{}': {}", path.string(), fastgltf::getErrorMessage(dataResult.error()));
		return false;
	}

	const std::string ext = path.extension().string();

	fastgltf::Expected<fastgltf::Asset> assetResult { fastgltf::Error::None };

	if (ext == ".glb")
		assetResult = parser.loadGltfBinary(dataResult.get(), path.parent_path(), options);
	else if (ext == ".gltf")
		assetResult = parser.loadGltf(dataResult.get(), path.parent_path(), options);
	else
	{
		std::println("[Mesh] Unsupported extension '{}': expected .gltf or .glb", ext);
		return false;
	}

	if (assetResult.error() != fastgltf::Error::None)
	{
		std::println("[Mesh] Failed to parse '{}': {}", path.string(), fastgltf::getErrorMessage(assetResult.error()));
		return false;
	}

	fastgltf::Asset& asset = assetResult.get();

	m_Name = path.stem().string();

	// Determine per-texture color space.
	// Normal, metallic/roughness, and occlusion maps are always linear.
	std::vector<Format> textureFormats(asset.textures.size(), Format::RGBA8_SRGB);

	for (const fastgltf::Material& gltfMaterial : asset.materials)
	{
		const auto& pbr = gltfMaterial.pbrData;

		if (gltfMaterial.normalTexture.has_value())
		{
			const size_t i = gltfMaterial.normalTexture->textureIndex;
			if (i < textureFormats.size())
				textureFormats[i] = Format::RGBA8_UNorm;
		}

		if (pbr.metallicRoughnessTexture.has_value())
		{
			const size_t i = pbr.metallicRoughnessTexture->textureIndex;
			if (i < textureFormats.size())
				textureFormats[i] = Format::RGBA8_UNorm;
		}

		if (gltfMaterial.occlusionTexture.has_value())
		{
			const size_t i = gltfMaterial.occlusionTexture->textureIndex;
			if (i < textureFormats.size())
				textureFormats[i] = Format::RGBA8_UNorm;
		}
	}

	// Textures
	m_Textures.reserve(asset.textures.size());

	for (size_t i = 0; i < asset.textures.size(); i++)
	{
		const fastgltf::Texture& gltfTexture = asset.textures[i];

		if (!gltfTexture.imageIndex.has_value())
		{
			m_Textures.push_back(nullptr);
			continue;
		}

		const fastgltf::Image& gltfImage = asset.images[gltfTexture.imageIndex.value()];
		const Format           format    = textureFormats[i];

		auto texture = std::make_shared<Texture2D>();
		bool loaded  = false;

		std::visit(fastgltf::visitor
		{
			[&](const fastgltf::sources::URI& uri)
			{
				const std::filesystem::path texturePath = path.parent_path() / uri.uri.path();

				TextureSpecification spec;
				spec.DebugName    = gltfImage.name.empty() ? texturePath.filename().string() : std::string(gltfImage.name);
				spec.Format       = format;
				spec.GenerateMips = true;

				texture->Create(spec, texturePath);
				loaded = texture->IsValid();
			},
			[&](const fastgltf::sources::Array& arr)
			{
				int width = 0, height = 0, channels = 0;
				stbi_uc* pixels = stbi_load_from_memory(
					reinterpret_cast<const stbi_uc*>(arr.bytes.data()),
					static_cast<int>(arr.bytes.size()),
					&width, &height, &channels, STBI_rgb_alpha);

				if (pixels)
				{
					TextureSpecification spec;
					spec.DebugName    = gltfImage.name.empty() ? std::string(m_Name) : std::string(gltfImage.name);
					spec.Format       = format;
					spec.GenerateMips = true;
					spec.Width        = static_cast<uint32_t>(width);
					spec.Height       = static_cast<uint32_t>(height);

					texture->Create(spec, pixels);
					stbi_image_free(pixels);
					loaded = texture->IsValid();
				}
			},
			[&](const fastgltf::sources::BufferView& bufferView)
			{
				const fastgltf::BufferView& view   = asset.bufferViews[bufferView.bufferViewIndex];
				const fastgltf::Buffer&     buffer = asset.buffers[view.bufferIndex];

				std::visit(fastgltf::visitor
				{
					[&](const fastgltf::sources::Array& arr)
					{
						int width = 0, height = 0, channels = 0;
						stbi_uc* pixels = stbi_load_from_memory(
							reinterpret_cast<const stbi_uc*>(arr.bytes.data() + view.byteOffset),
							static_cast<int>(view.byteLength),
							&width, &height, &channels, STBI_rgb_alpha);

						if (pixels)
						{
							TextureSpecification spec;
							spec.DebugName    = gltfImage.name.empty() ? std::string(m_Name) : std::string(gltfImage.name);
							spec.Format       = format;
							spec.GenerateMips = true;
							spec.Width        = static_cast<uint32_t>(width);
							spec.Height       = static_cast<uint32_t>(height);

							texture->Create(spec, pixels);
							stbi_image_free(pixels);
							loaded = texture->IsValid();
						}
					},
					[](auto&) {}
				}, buffer.data);
			},
			[](auto&) {}
		}, gltfImage.data);

		if (loaded)
		{
			std::println("[Mesh] Loaded texture '{}' ({})", texture->GetSpecification().DebugName, format == Format::RGBA8_SRGB ? "sRGB" : "Linear");
		}
		else
		{
			std::println("[Mesh] Failed to load texture at index {}", i);
		}

		m_Textures.push_back(loaded ? std::move(texture) : nullptr);
	}

	// Materials
	m_Materials.reserve(asset.materials.size());

	for (const fastgltf::Material& gltfMaterial : asset.materials)
	{
		Material material;

		// Alpha mode
		switch (gltfMaterial.alphaMode)
		{
			case fastgltf::AlphaMode::Opaque:
				material.SetRenderMode(Material::RenderMode::Opaque);
				material.SetAlphaCutoff(0.0f);
				break;
			case fastgltf::AlphaMode::Mask:
				material.SetRenderMode(Material::RenderMode::Cutout);
				material.SetAlphaCutoff(static_cast<float>(gltfMaterial.alphaCutoff));
				break;
			case fastgltf::AlphaMode::Blend:
				material.SetRenderMode(Material::RenderMode::Transparent);
				material.SetBlendSrc(Material::BlendFactor::SrcAlpha);
				material.SetBlendDst(Material::BlendFactor::OneMinusSrcAlpha);
				break;
		}

		// PBR factors
		const auto& pbr = gltfMaterial.pbrData;
		const auto& c   = pbr.baseColorFactor;
		material.SetColor({ c[0], c[1], c[2], c[3] });
		material.SetMetalness(pbr.metallicFactor);
		material.SetRoughness(pbr.roughnessFactor);

		if (gltfMaterial.transmission)
			material.SetTransmission(static_cast<float>(gltfMaterial.transmission->transmissionFactor));

		// Albedo texture
		if (pbr.baseColorTexture.has_value())
		{
			const size_t texIndex = pbr.baseColorTexture->textureIndex;
			assert(texIndex < m_Textures.size());

			if (m_Textures[texIndex])
				material.SetTexture({ .Texture = m_Textures[texIndex], .Type = MapType::Albedo, .UvIndex = 0, .Enabled = true });
		}

		// Normal texture
		if (gltfMaterial.normalTexture.has_value())
		{
			const size_t texIndex = gltfMaterial.normalTexture->textureIndex;
			assert(texIndex < m_Textures.size());

			if (m_Textures[texIndex])
				material.SetTexture({ .Texture = m_Textures[texIndex], .Type = MapType::Normal, .UvIndex = 0, .Enabled = true });
		}

		// Metallic/roughness texture
		if (pbr.metallicRoughnessTexture.has_value())
		{
			const size_t texIndex = pbr.metallicRoughnessTexture->textureIndex;
			assert(texIndex < m_Textures.size());

			if (m_Textures[texIndex])
				material.SetTexture({ .Texture = m_Textures[texIndex], .Type = MapType::MetallicRoughness, .UvIndex = 0, .Enabled = true });
		}

		// Occlusion texture
		if (gltfMaterial.occlusionTexture.has_value())
		{
			const size_t texIndex = gltfMaterial.occlusionTexture->textureIndex;
			assert(texIndex < m_Textures.size());

			if (m_Textures[texIndex])
				material.SetTexture({ .Texture = m_Textures[texIndex], .Type = MapType::Occlusion, .UvIndex = 0, .Enabled = true });
		}

		m_Materials.push_back(std::move(material));
	}

	// Geometry
	std::vector<Vertex> vertices;
	std::vector<Index>  indices;

	std::vector<std::vector<uint32_t>> meshSubmeshes(asset.meshes.size());

	for (size_t meshIndex = 0; meshIndex < asset.meshes.size(); meshIndex++)
	{
		for (const fastgltf::Primitive& primitive : asset.meshes[meshIndex].primitives)
		{
			if (primitive.type != fastgltf::PrimitiveType::Triangles)
				continue;

			std::vector<Vertex> primVertices;
			std::vector<Index>  primIndices;

			bool hasNormals   = false;
			bool hasTexCoords = false;

			// Positions
			{
				const auto it = primitive.findAttribute("POSITION");
				assert(it != primitive.attributes.end() && "Mesh has no POSITION attribute");

				const fastgltf::Accessor& accessor = asset.accessors[it->accessorIndex];
				primVertices.resize(accessor.count);

				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset, accessor,
					[&](const fastgltf::math::fvec3& pos, size_t i)
					{
						primVertices[i].Position = { pos.x(), pos.y(), pos.z() };
					});
			}

			// Normals
			{
				const auto it = primitive.findAttribute("NORMAL");
				if (it != primitive.attributes.end())
				{
					fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset, asset.accessors[it->accessorIndex],
						[&](const fastgltf::math::fvec3& n, size_t i)
						{
							primVertices[i].Normal = { n.x(), n.y(), n.z() };
						});
					hasNormals = true;
				}
			}

			// TexCoords
			{
				const auto it = primitive.findAttribute("TEXCOORD_0");
				if (it != primitive.attributes.end())
				{
					fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(asset, asset.accessors[it->accessorIndex],
						[&](const fastgltf::math::fvec2& uv, size_t i)
						{
							primVertices[i].TexCoord = { uv.x(), uv.y() };
						});
					hasTexCoords = true;
				}
			}

			// Indices
			{
				assert(primitive.indicesAccessor.has_value() && "Mesh primitive has no indices");

				const fastgltf::Accessor& accessor = asset.accessors[primitive.indicesAccessor.value()];

				if (accessor.count % 3 != 0)
				{
					std::println("[Mesh] Triangle primitive has invalid index count: {}", accessor.count);
					return false;
				}

				primIndices.resize(accessor.count / 3);

				fastgltf::iterateAccessorWithIndex<uint32_t>(asset, accessor,
					[&](uint32_t index, size_t i)
					{
						Index& tri = primIndices[i / 3];
						switch (i % 3)
						{
							case 0: tri.V1 = index; break;
							case 1: tri.V2 = index; break;
							case 2: tri.V3 = index; break;
						}
					});
			}

			// MikkTSpace tangents
			if (hasNormals && hasTexCoords)
				GenerateTangents(primVertices, primIndices);

			const uint32_t submeshIndex = static_cast<uint32_t>(m_Submeshes.size());
			meshSubmeshes[meshIndex].push_back(submeshIndex);

			Submesh& submesh    = m_Submeshes.emplace_back();
			submesh.BaseVertex  = static_cast<uint32_t>(vertices.size());
			submesh.BaseIndex   = static_cast<uint32_t>(indices.size() * 3);
			submesh.VertexCount = static_cast<uint32_t>(primVertices.size());
			submesh.IndexCount  = static_cast<uint32_t>(primIndices.size() * 3);

			if (primitive.materialIndex.has_value())
				submesh.MaterialIndex = static_cast<uint32_t>(primitive.materialIndex.value());

			vertices.insert(vertices.end(), primVertices.begin(), primVertices.end());
			indices.insert(indices.end(), primIndices.begin(), primIndices.end());
		}
	}

	// Nodes retain their original glTF indices. A glTF scene owns an array of
	// root-node indices, so no synthetic root node is inserted.
	m_Nodes.resize(asset.nodes.size());

	// Create Nodes
	for (size_t i = 0; i < asset.nodes.size(); i++)
	{
		const fastgltf::Node& gltfNode = asset.nodes[i];
		Node&                 node     = m_Nodes[i];

		node.Name           = gltfNode.name;
		node.LocalTransform = NodeToMatrix(gltfNode);

		// Children
		for (size_t childIndex : gltfNode.children)
		{
			assert(childIndex < m_Nodes.size());
			node.Children.push_back(static_cast<uint32_t>(childIndex));
		}

		// Mesh / Submeshes
		if (gltfNode.meshIndex.has_value())
		{
			const size_t mi = gltfNode.meshIndex.value();
			assert(mi < meshSubmeshes.size());

			for (uint32_t si : meshSubmeshes[mi])
				node.Submeshes.push_back(si);
		}
	}

	// Parent relationships — second pass so all nodes exist first.
	for (size_t i = 0; i < m_Nodes.size(); i++)
	{
		for (uint32_t childIndex : m_Nodes[i].Children)
		{
			assert(childIndex < m_Nodes.size());
			m_Nodes[childIndex].Parent = static_cast<uint32_t>(i);
		}
	}

	// Scene / root nodes
	if (!asset.scenes.empty())
	{
		const size_t sceneIndex = asset.defaultScene.value_or(0);
		assert(sceneIndex < asset.scenes.size());

		const fastgltf::Scene& scene = asset.scenes[sceneIndex];
		m_SceneName = scene.name;

		for (size_t nodeIndex : scene.nodeIndices)
		{
			assert(nodeIndex < m_Nodes.size());
			m_RootNodes.push_back(static_cast<uint32_t>(nodeIndex));
		}
	}
	else
	{
		// No scene declared - treat every parentless node as a root.
		for (size_t i = 0; i < m_Nodes.size(); i++)
		{
			if (m_Nodes[i].IsRoot())
				m_RootNodes.push_back(static_cast<uint32_t>(i));
		}
	}

	// GPU upload - CPU arrays are discarded after upload.
	if (!vertices.empty())
		m_VertexBuffer.Create(vertices.data(), vertices.size() * sizeof(Vertex));

	if (!indices.empty())
		m_IndexBuffer.Create(indices.data(), indices.size() * sizeof(Index));

	std::println("[Mesh] Loaded '{}' - {} vertices, {} indices, {} submeshes, {} nodes, {} textures, {} materials",
		m_Name,
		vertices.size(),
		indices.size() * 3,
		m_Submeshes.size(),
		m_Nodes.size(),
		m_Textures.size(),
		m_Materials.size());

	return true;
}

void Mesh::Destroy()
{
	m_VertexBuffer.Destroy();
	m_IndexBuffer.Destroy();

	for (auto& texture : m_Textures)
	{
		if (texture)
			texture->Destroy();
	}

	m_Textures.clear();
	m_Submeshes.clear();
	m_Materials.clear();
	m_Nodes.clear();
	m_RootNodes.clear();
	m_Name.clear();
	m_SceneName.clear();
}

void Mesh::TraverseNodes(const std::function<void(const Node&, const glm::mat4&)>& callback) const
{
	if (m_Nodes.empty())
		return;

	std::function<void(uint32_t, const glm::mat4&)> traverse = [&](uint32_t nodeIndex, const glm::mat4& parentTransform)
	{
		const Node&     node           = m_Nodes[nodeIndex];
		const glm::mat4 worldTransform = parentTransform * node.LocalTransform;

		callback(node, worldTransform);

		for (uint32_t childIndex : node.Children)
			traverse(childIndex, worldTransform);
	};

	for (uint32_t root : m_RootNodes)
		traverse(root, glm::mat4(1.0f));
}
