#include "Shader.hpp"

#include "CommandBuffer.hpp"
#include "Descriptors.hpp"
#include "RendererContext.hpp"
#include "ShaderCompiler.hpp"

#include <algorithm>
#include <cassert>
#include <string>
#include <utility>
#include <vector>

namespace
{
	std::vector<VkPushConstantRange> CreatePushConstantRanges(const ShaderReflectionData& reflectionData)
	{
		std::vector<VkPushConstantRange> ranges;
		ranges.reserve(reflectionData.PushConstantRanges.size());

		for (const PushConstantRange& range : reflectionData.PushConstantRanges)
		{
			ranges.push_back(
	{
				 .stageFlags = range.StageFlags,
				 .offset     = range.Offset,
				 .size       = range.Size
			});
		}

		return ranges;
	}

	bool HasStage(const std::vector<ShaderStage>& stages, ShaderStage stage)
	{
		return std::ranges::find(stages, stage) != stages.end();
	}

	bool CanLinkStages(const std::vector<ShaderStage>& stages)
	{
		return stages.size() == 2 && HasStage(stages, ShaderStage::Vertex) && HasStage(stages, ShaderStage::Fragment);
	}

	VkShaderStageFlags GetNextStage(ShaderStage stage, const std::vector<ShaderStage>& stages)
	{
		if (stage == ShaderStage::Vertex && HasStage(stages, ShaderStage::Fragment))
		{
			return VK_SHADER_STAGE_FRAGMENT_BIT;
		}

		return 0;
	}

	bool SupportsShaderObjects(ShaderStage stage)
	{
		switch (stage)
		{
			case ShaderStage::Vertex:
			case ShaderStage::Fragment:
			case ShaderStage::Compute:
				return true;

			default:
				return false;
		}
	}

	const char* GetShaderStageName(ShaderStage stage)
	{
		switch (stage)
		{
			case ShaderStage::Vertex:       return "Vertex";
			case ShaderStage::Fragment:     return "Fragment";
			case ShaderStage::Compute:      return "Compute";
			case ShaderStage::RayGen:       return "Ray Generation";
			case ShaderStage::Miss:         return "Miss";
			case ShaderStage::ClosestHit:   return "Closest Hit";
			case ShaderStage::AnyHit:       return "Any Hit";
			case ShaderStage::Intersection: return "Intersection";
			case ShaderStage::Callable:     return "Callable";
			case ShaderStage::None:         return "None";
		}

		return "Unknown";
	}
}

void Shader::Load(const std::filesystem::path& filePath)
{
	ShaderCompileResult result = ShaderCompiler::Compile(filePath);

	if (!result.IsValid())
		return;

	// Preserve the currently working shader if compilation fails.
	Destroy();

	m_Path           = filePath;
	m_SpirV          = std::move(result.SpirV);
	m_Stages         = std::move(result.Stages);
	m_ReflectionData = std::move(result.Reflection);

	assert(!m_SpirV.empty());
	assert(!m_Stages.empty());

	// Both objects use the same descriptor layouts and push constant ranges.
	CreatePipelineLayout();
	CreateShaderObjects();
}

void Shader::Shutdown()
{
	Destroy();

	m_SpirV.clear();
	m_Stages.clear();
	m_ReflectionData = {};
	m_Path.clear();
}

void Shader::Reload()
{
	if (m_Path.empty())
		return;

	const std::filesystem::path path = m_Path;
	Load(path);
}

void Shader::Bind(CommandBuffer& commandBuffer) const
{
	assert(IsValid());

	vkCmdBindShadersEXT(commandBuffer.GetHandle(), static_cast<uint32_t>(m_ShaderStageBits.size()), m_ShaderStageBits.data(), m_ShaderObjects.data());
}

void Shader::CreatePipelineLayout()
{
	VkDevice device = RendererContext::Get().GetDevice();

	const std::vector<VkPushConstantRange> pushConstantRanges = CreatePushConstantRanges(m_ReflectionData);

	const VkDescriptorSetLayout descriptorSetLayout = Descriptor::GetLayout();

	const VkPipelineLayoutCreateInfo layoutInfo
	{
		.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount         = 1,
		.pSetLayouts            = &descriptorSetLayout,
		.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size()),
		.pPushConstantRanges    = pushConstantRanges.empty() ? nullptr : pushConstantRanges.data()
	};

	VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_PipelineLayout));

	if (!m_Path.empty())
	{
		const std::string name = m_Path.stem().string() + " Layout";
		SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, name, m_PipelineLayout);
	}
}

void Shader::CreateShaderObjects()
{
	VkDevice device = RendererContext::Get().GetDevice();

	const VkDescriptorSetLayout descriptorSetLayout = Descriptor::GetLayout();

	const std::vector<VkPushConstantRange> pushConstantRanges = CreatePushConstantRanges(m_ReflectionData);

	const uint32_t stageCount = static_cast<uint32_t>(m_Stages.size());
	const bool linkStages = CanLinkStages(m_Stages);

	m_ShaderObjects.assign(stageCount, VK_NULL_HANDLE);
	m_ShaderStageBits.resize(stageCount);

	std::vector<VkShaderCreateInfoEXT> createInfos;
	createInfos.reserve(stageCount);

	for (uint32_t i = 0; i < stageCount; ++i)
	{
		const ShaderStage stage = m_Stages[i];

		// Ray-tracing stages still require a ray-tracing pipeline and SBT.
		assert(SupportsShaderObjects(stage));

		const VkShaderStageFlagBits stageBit = ToVulkanStage(stage);

		assert(stageBit != VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM);

		m_ShaderStageBits[i] = stageBit;
		const VkShaderCreateFlagsEXT shaderFlags = linkStages ? VK_SHADER_CREATE_LINK_STAGE_BIT_EXT : VkShaderCreateFlagsEXT{ 0 };

		createInfos.push_back(
		{
			.sType                  = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
			.flags                  = shaderFlags,
			.stage                  = stageBit,
			.nextStage              = GetNextStage(stage, m_Stages),
			.codeType               = VK_SHADER_CODE_TYPE_SPIRV_EXT,
			.codeSize               = m_SpirV.size() * sizeof(uint32_t),
			.pCode                  = m_SpirV.data(),
			.pName                  = "main",
			.setLayoutCount         = 1,
			.pSetLayouts            = &descriptorSetLayout,
			.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size()),
			.pPushConstantRanges    = pushConstantRanges.empty() ? nullptr : pushConstantRanges.data(),
			.pSpecializationInfo    = nullptr
		});
	}

	VK_CHECK(vkCreateShadersEXT(device, stageCount, createInfos.data(), nullptr, m_ShaderObjects.data()));

	if (!m_Path.empty())
	{
		for (uint32_t i = 0; i < stageCount; ++i)
		{
			const std::string name = m_Path.stem().string() + " [" + GetShaderStageName(m_Stages[i]) + "]";

			SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_SHADER_EXT, name, m_ShaderObjects[i]);
		}
	}
}

void Shader::Destroy()
{
	VkDevice device = RendererContext::Get().GetDevice();

	for (VkShaderEXT shader : m_ShaderObjects)
	{
		if (shader != VK_NULL_HANDLE)
			vkDestroyShaderEXT(device, shader, nullptr);
	}

	m_ShaderObjects.clear();
	m_ShaderStageBits.clear();

	if (m_PipelineLayout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);

		m_PipelineLayout = VK_NULL_HANDLE;
	}
}

VkShaderStageFlagBits Shader::ToVulkanStage(ShaderStage stage)
{
	switch (stage)
	{
		case ShaderStage::Vertex:       return VK_SHADER_STAGE_VERTEX_BIT;
		case ShaderStage::Fragment:     return VK_SHADER_STAGE_FRAGMENT_BIT;
		case ShaderStage::Compute:      return VK_SHADER_STAGE_COMPUTE_BIT;
		case ShaderStage::RayGen:       return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
		case ShaderStage::Miss:         return VK_SHADER_STAGE_MISS_BIT_KHR;
		case ShaderStage::ClosestHit:   return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		case ShaderStage::AnyHit:       return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
		case ShaderStage::Intersection: return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
		case ShaderStage::Callable:     return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
		case ShaderStage::None:         break;
	}

	return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
}
