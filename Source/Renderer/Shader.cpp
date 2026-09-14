#include "Shader.hpp"

#include <map>

#include "RendererContext.hpp"
#include "ShaderCompiler.hpp"

void Shader::Load(const std::filesystem::path& filePath)
{
	m_Path = filePath;

	ShaderCompileResult result = ShaderCompiler::Compile(m_Path);

	if (!result.IsValid())
		return;

	m_SpirV = std::move(result.SpirV);
	m_Stages = std::move(result.Stages);
	m_ReflectionData = std::move(result.Reflection);

	CreateShaderModule();
	CreateStageInfos();
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
	Destroy();
	Load(m_Path);
}

void Shader::CreateShaderModule()
{
	VkDevice device = RendererContext::Get().GetDevice();

	VkShaderModuleCreateInfo createInfo
	{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = m_SpirV.size() * sizeof(uint32_t),
		.pCode = m_SpirV.data()
	};

	VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &m_ShaderModule));
}

void Shader::CreateStageInfos()
{
	m_StageInfos.clear();
	m_StageInfos.reserve(m_Stages.size());

	for (ShaderStage stage : m_Stages)
	{
		m_StageInfos.push_back(
			{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.flags = 0,
				.stage = ToVulkanStage(stage),
				.module = m_ShaderModule,
				.pName = "main"
			}
		);
	}
}

void Shader::Destroy()
{
	if (m_ShaderModule != VK_NULL_HANDLE)
	{
		vkDestroyShaderModule(RendererContext::Get().GetDevice(), m_ShaderModule, nullptr);

		m_ShaderModule = VK_NULL_HANDLE;
	}

	m_StageInfos.clear();
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
