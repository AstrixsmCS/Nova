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
	glm::vec3 Color     {  1.0f,  1.0f,  1.0f };
	float     Intensity = 3.0f;
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
	void CreateDepthImage(uint32_t width, uint32_t height);
	void DrawMesh(CommandBuffer& commandBuffer, const Mesh& mesh);

private:
	std::shared_ptr<Shader> m_GeometryShader;
	GraphicsState           m_GeometryState;
	Material                m_GeometryMaterial;

	Mesh             m_Mesh;
	DirectionalLight m_DirectionalLight;

	Camera                                            m_Camera;
	std::array<Buffer, Renderer::GetFramesInFlight()> m_CameraBuffers;

	Texture m_DepthImage;
};
