#pragma once

#include "Core/Application.hpp"

#include "Renderer/Camera.hpp"
#include "Renderer/Material.hpp"
#include "Renderer/MaterialSystem.hpp"
#include "Renderer/Mesh.hpp"
#include "Renderer/Renderer.hpp"
#include "Renderer/Vulkan/Buffer.hpp"
#include "Renderer/Vulkan/CommandBuffer.hpp"
#include "Renderer/Vulkan/Shader.hpp"
#include "Renderer/Vulkan/Texture.hpp"

#include <glm/glm.hpp>

#include <array>
#include <memory>

struct CameraUniforms
{
	glm::mat4 ViewProjection        { 1.0f };
	glm::mat4 InverseViewProjection { 1.0f };
	glm::vec3 Position              { 0.0f };
	float     _Pad0 = 0.0f;
};
static_assert(sizeof(CameraUniforms) == 144);

struct DirectionalLight
{
	glm::vec3 Direction { -1.0f, -1.0f, -1.0f };
	glm::vec3 Radiance  { 1.0f, 1.0f, 1.0f };
	float     Intensity = 1.0f;
};

//   Albedo       RGBA8_SRGB         rgb = base color,          a = ambient occlusion (alpha is always linear)
//   Normal       RG16_Float         xy = octahedral-encoded world-space normal
//   Material     RGBA8_UNorm        r = unused, g = roughness, b = metallic (glTF metallicRoughness packing)
//   Emissive     RGBA16_Float       rgb = emitted radiance,    a = unused
//   DepthStencil D32_Float_S8_UInt  depth sampled by the lighting pass, stencil reserved
struct GBuffer
{
	Texture Albedo;
	Texture Normal;
	Texture Material;
	Texture Emissive;
	Texture DepthStencil;
};

class EditorApplication final : public Application
{
public:
	explicit EditorApplication(const ApplicationSpecification& specification);
	~EditorApplication();

protected:
	void OnInitialize()     override;
	void OnUpdate(Timestep) override;
	void OnShutdown()       override;

private:
	void CreateRendererResources();
	void DestroyRendererResources();

	void CreateRenderTargets(uint32_t width, uint32_t height);
	void DestroyRenderTargets();

	void UpdateCamera(Timestep ts);

	void GeometryPass(CommandBuffer& commandBuffer, VkExtent2D extent);
	void LightingPass(CommandBuffer& commandBuffer, VkExtent2D extent);
	void CompositePass(CommandBuffer& commandBuffer, VkExtent2D extent);

	void DrawMesh(CommandBuffer& commandBuffer, const Mesh& mesh);

private:
	Mesh m_Mesh;
	Camera m_Camera;
	DirectionalLight m_DirectionalLight;

	std::array<Buffer, Renderer::GetFramesInFlight()> m_CameraBuffers;

	std::shared_ptr<Shader> m_GBufferShader;
	GraphicsState           m_GBufferState;
	Material                m_GBufferMaterial;
	GBuffer                 m_GBuffer;

	std::shared_ptr<Shader> m_LightingShader;
	Material                m_LightingMaterial;

	Texture m_HDRColor;

	std::shared_ptr<Shader> m_CompositeShader;
	GraphicsState           m_CompositeState;
	Material                m_CompositeMaterial;
};
