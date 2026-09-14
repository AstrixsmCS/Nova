#pragma once

#include "Vulkan.hpp"
#include "RendererTypes.hpp"

#include <filesystem>
#include <string>
#include <vector>

struct PushConstantMember
{
	std::string Name;
	uint32_t    Offset = 0;
	uint32_t    Size   = 0;
};

struct PushConstantRange
{
	VkShaderStageFlags StageFlags = 0;
	uint32_t           Offset     = 0;
	uint32_t           Size       = 0;
};

struct DescriptorBinding
{
	uint32_t           Set            = 0;
	uint32_t           Binding        = 0;
	uint32_t           Count          = 1;
	VkDescriptorType   DescriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
	VkShaderStageFlags StageFlags     = 0;
	std::string        Name;
};

struct ShaderReflectionData
{
	std::vector<PushConstantRange>  PushConstantRanges;
	std::vector<PushConstantMember> PushConstantMembers;
	std::vector<DescriptorBinding>  DescriptorBindings;

	bool HasPushConstants()  const { return !PushConstantRanges.empty(); }
	bool HasDescriptorSets() const { return !DescriptorBindings.empty(); }
};

class CommandBuffer;

class Shader
{
public:
	void Load(const std::filesystem::path& filePath);
	void Shutdown();
	void Reload();

	void Bind(CommandBuffer& commandBuffer) const;

	bool IsValid() const { return m_PipelineLayout != VK_NULL_HANDLE && !m_ShaderObjects.empty() && m_ShaderObjects.size() == m_ShaderStageBits.size(); }

	const std::filesystem::path& GetPath() const { return m_Path; }

	VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }

	const std::vector<VkShaderEXT>&           GetShaderObjects() const { return m_ShaderObjects; }
	const std::vector<VkShaderStageFlagBits>& GetShaderStages()  const { return m_ShaderStageBits; }

	// Reflection
	const ShaderReflectionData&            GetReflectionData()      const { return m_ReflectionData; }
	const std::vector<PushConstantRange>&  GetPushConstantRanges()  const { return m_ReflectionData.PushConstantRanges; }
	const std::vector<PushConstantMember>& GetPushConstantMembers() const { return m_ReflectionData.PushConstantMembers; }
	const std::vector<DescriptorBinding>&  GetDescriptorBindings()  const { return m_ReflectionData.DescriptorBindings; }

	static VkShaderStageFlagBits ToVulkanStage(ShaderStage stage);

private:
	void CreatePipelineLayout();
	void CreateShaderObjects();
	void Destroy();

private:
	std::filesystem::path    m_Path;
	std::vector<uint32_t>    m_SpirV;
	std::vector<ShaderStage> m_Stages;

	std::vector<VkShaderEXT>           m_ShaderObjects;
	std::vector<VkShaderStageFlagBits> m_ShaderStageBits;

	VkPipelineLayout     m_PipelineLayout = VK_NULL_HANDLE;
	ShaderReflectionData m_ReflectionData;
};
