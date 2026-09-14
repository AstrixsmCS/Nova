#pragma once

#include "Vulkan.hpp"

#include "CommandBuffer.hpp"

#include <unordered_set>

struct SDL_Window;

class RendererContext
{
public:
	static void Initialize();
	static void Shutdown();
	static RendererContext& Get();

	VkInstance GetInstance() const { return m_VulkanInstance; }
	VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
	VkDevice GetDevice() const { return m_LogicalDevice; }

	VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
	uint32_t GetGraphicsFamily() const { return m_GraphicsFamily; }
	VkQueue GetComputeQueue() const { return m_ComputeQueue; }
	uint32_t GetComputeFamily() const { return m_ComputeFamily; }
	VkQueue GetTransferQueue() const { return m_TransferQueue; }
	uint32_t GetTransferFamily() const { return m_TransferFamily; }

	CommandPool& GetImmediateCommandPool() { return *m_ImmediateCommandPool; }
	const CommandPool& GetImmediateCommandPool() const { return *m_ImmediateCommandPool; }

	const VkPhysicalDeviceProperties& GetPhysicalDeviceProperties() const { return m_PhysicalDeviceProperties; }
	const VkPhysicalDeviceLimits& GetPhysicalDeviceLimits() const { return m_PhysicalDeviceProperties.limits; }

	bool IsExtensionSupported(const std::string& extensionName) const;
private:
	void CreateInstance();
	void SetupDebugMessenger();

	void PickPhysicalDevice();
	void CreateLogicalDevice();
private:
	VkInstance m_VulkanInstance = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;

	VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
	VkPhysicalDeviceProperties m_PhysicalDeviceProperties{};
	VkPhysicalDeviceFeatures m_PhysicalDeviceFeatures{};
	VkDevice m_LogicalDevice = VK_NULL_HANDLE;

	VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
	uint32_t m_GraphicsFamily = UINT32_MAX;
	VkQueue m_ComputeQueue = VK_NULL_HANDLE;
	uint32_t m_ComputeFamily = UINT32_MAX;
	VkQueue m_TransferQueue = VK_NULL_HANDLE;
	uint32_t m_TransferFamily = UINT32_MAX;

	std::unique_ptr<CommandPool> m_ImmediateCommandPool = nullptr;

	std::unordered_set<std::string> m_SupportedExtensions;

	bool m_EnableDebugMarkers = false;
};
