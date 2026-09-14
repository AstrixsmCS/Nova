#pragma once

#include "Vulkan.hpp"
#include "RendererTypes.hpp"

#include <filesystem>
#include <vector>

struct PushConstantMember
{
	std::string Name;

	uint32_t Offset = 0;
	uint32_t Size = 0;
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
	std::vector<PushConstantRange> PushConstantRanges;
	std::vector<PushConstantMember> PushConstantMembers;
	std::vector<DescriptorBinding> DescriptorBindings;

	bool HasPushConstants()  const { return !PushConstantRanges.empty(); }
	bool HasDescriptorSets() const { return !DescriptorBindings.empty(); }
};

class Shader
{
public:
	void Load(const std::filesystem::path& filePath);
	void Shutdown();
	void Reload();

	bool IsValid() const { return m_ShaderModule != VK_NULL_HANDLE; }

	const std::filesystem::path& GetPath() const { return m_Path; }

	VkShaderModule GetShaderModule() const { return m_ShaderModule; }

	const std::vector<VkPipelineShaderStageCreateInfo>& GetStageInfos() const { return m_StageInfos; }

	// Reflection
	const ShaderReflectionData&           GetReflectionData()     const { return m_ReflectionData; }
	const std::vector<PushConstantRange>& GetPushConstantRanges() const { return m_ReflectionData.PushConstantRanges; }
	const std::vector<PushConstantMember>& GetPushConstantMembers() const { return m_ReflectionData.PushConstantMembers; }
	const std::vector<DescriptorBinding>& GetDescriptorBindings() const { return m_ReflectionData.DescriptorBindings; }
private:
	void CreateShaderModule();
	void CreateStageInfos();
	void Destroy();

	static VkShaderStageFlagBits ToVulkanStage(ShaderStage stage);

private:
	std::filesystem::path m_Path;

	std::vector<uint32_t> m_SpirV;
	std::vector<ShaderStage> m_Stages;

	VkShaderModule m_ShaderModule = VK_NULL_HANDLE;

	std::vector<VkPipelineShaderStageCreateInfo> m_StageInfos;

	ShaderReflectionData m_ReflectionData;
};
