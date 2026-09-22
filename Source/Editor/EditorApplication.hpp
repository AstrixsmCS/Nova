#pragma once

#include "Core/Application.hpp"
#include "Asset/AssetManager.hpp"

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
	void RecordComputePass(CommandBuffer& cmd, float time);

private:
	VertexBuffer m_VertexBuffer;
	IndexBuffer  m_IndexBuffer;

	std::shared_ptr<Shader> m_GraphicsShader;
	GraphicsState           m_GraphicsState;
	Image2D                 m_DepthImage;

	std::shared_ptr<Shader> m_ComputeShader;
	Image2D                 m_ComputeImage;

	std::shared_ptr<Texture2D> m_Texture;

	float m_Time = 0.0f;
};
