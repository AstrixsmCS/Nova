#pragma once

#include "Core/Application.hpp"

#include "Renderer/Vulkan/Buffer.hpp"
#include "Renderer/Vulkan/CommandBuffer.hpp"
#include "Renderer/Vulkan/Shader.hpp"
#include "Renderer/Vulkan/Texture.hpp"
#include "Renderer/Vulkan/Image.hpp"

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
	void CreateDepthImage(const Dimensions& size);
	void CreateComputeImage(const Dimensions& size);
	void RecordComputePass(CommandBuffer& commandBuffer, float time);

private:
	Buffer m_VertexBuffer;
	Buffer  m_IndexBuffer;

	VertexBufferLayout m_VertexLayout;

	std::shared_ptr<Shader> m_GraphicsShader;
	GraphicsState           m_GraphicsState;
	Texture                 m_DepthImage;

	std::shared_ptr<Shader> m_ComputeShader;
	Texture                 m_ComputeImage;

	std::shared_ptr<Texture> m_Texture;

	float m_Time = 0.0f;
};
