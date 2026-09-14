#pragma once

#include "Core/Application.hpp"

#include "Renderer/Buffer.hpp"
#include "Renderer/Camera.hpp"
#include "Renderer/GraphicsState.hpp"
#include "Renderer/Image.hpp"
#include "Renderer/Material.hpp"
#include "Renderer/MaterialSystem.hpp"
#include "Renderer/Mesh.hpp"
#include "Renderer/Renderer.hpp"
#include "Renderer/Shader.hpp"

#include <array>

struct CameraUniforms
{
	glm::mat4 ViewProjection        { 1.0f };
	glm::mat4 InverseViewProjection { 1.0f };
	glm::vec3 Position;
	float     _Pad0 = 0.0f;
};

class EditorApplication final : public Application
{
public:
	explicit EditorApplication(const ApplicationSpecification& specification);
	~EditorApplication();

protected:
	void OnInitialize() override;
	void OnUpdate(Timestep)     override;
	void OnShutdown()   override;

private:
	void CreateDepthImage(uint32_t width, uint32_t height);
	void DrawMesh(CommandBuffer& cmd, const Mesh& mesh);

private:
	std::shared_ptr<Shader> m_GeometryShader;
	GraphicsState           m_GeometryState;
	Material                m_GeometryMaterial;

	Mesh                  m_Mesh;
	std::vector<uint32_t> m_MaterialIndices;

	Camera m_Camera;
	std::array<UniformBuffer, Renderer::GetFramesInFlight()> m_CameraBuffers;

	Image2D m_DepthImage;
};
