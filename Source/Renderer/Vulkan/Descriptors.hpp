#pragma once

#include "Vulkan.hpp"

#include "Renderer/RendererTypes.hpp"

#include <cstdint>
#include <vector>
#include <array>

class Descriptor
{
public:
	static constexpr uint32_t INVALID_INDEX = UINT32_MAX;
	static constexpr uint32_t NULL_TEXTURE = 0;

	static void Initialize();
	static void Shutdown();

	static uint32_t RegisterTexture(VkImageView imageView, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	static void UnregisterTexture(uint32_t index);

	static uint32_t RegisterStorageImage(VkImageView imageView);
	static void UnregisterStorageImage(uint32_t index);

	static uint32_t GetDefaultSamplerIndex(DefaultSampler sampler) { return static_cast<uint32_t>(sampler); }

	static VkDescriptorSetLayout GetLayout() { return s_Layout; }
	static VkDescriptorSet GetSet() { return s_Set; }
private:
	static void CreateDefaultSamplers();
	static void WriteSampler(uint32_t index, VkSampler sampler);
private:
	static constexpr uint32_t MAX_TEXTURES = 4096;
	static constexpr uint32_t MAX_STORAGE_IMAGES = 1024;
	static constexpr uint32_t DEFAULT_SAMPLER_COUNT = static_cast<uint32_t>(DefaultSampler::Count);

	static inline VkDescriptorSetLayout s_Layout = VK_NULL_HANDLE;
	static inline VkDescriptorPool      s_Pool   = VK_NULL_HANDLE;
	static inline VkDescriptorSet       s_Set    = VK_NULL_HANDLE;

	static inline std::array<VkSampler, DEFAULT_SAMPLER_COUNT> s_DefaultSamplers{};

	static inline std::vector<uint32_t> s_FreeTextureIndices;
	static inline std::vector<uint32_t> s_FreeStorageImageIndices;
};
