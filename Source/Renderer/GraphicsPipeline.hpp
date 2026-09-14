#pragma once

#include "Vulkan.hpp"
#include "Shader.hpp"
#include "Buffer.hpp"
#include "CommandBuffer.hpp"

enum class BlendMode
{
	None = 0,
	Alpha,
	PremultipliedAlpha,
	Additive,
	Multiply
};

struct GraphicsPipelineSpecification
{
	std::string DebugName;
	std::shared_ptr<Shader> Shader;

	std::vector<Format> ColorFormats;
	Format DepthFormat = Format::Invalid;

	VertexBufferLayout Layout;

	Topology Topology = Topology::Triangle;
	CompareOp DepthCompareOp = CompareOp::Less;
	BlendMode BlendMode = BlendMode::None;

	bool BackfaceCulling = true;
	bool DepthTest = true;
	bool DepthWrite = true;
	bool Wireframe = false;
	float LineWidth = 1.0f;
};

class Pipeline
{
public:
	void Create(const GraphicsPipelineSpecification& specification);
	void Shutdown();

	void Bind(CommandBuffer& commandBuffer) const;

	const GraphicsPipelineSpecification& GetSpecification() const { return m_Specification; }

	VkPipeline GetPipeline() const { return m_Pipeline; }
	VkPipelineLayout GetLayout() const { return m_PipelineLayout; }
private:
	void CreatePipelineLayout();
private:
	GraphicsPipelineSpecification m_Specification;

	VkPipeline m_Pipeline = VK_NULL_HANDLE;
	VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
};
