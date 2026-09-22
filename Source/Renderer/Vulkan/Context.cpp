#include "Context.hpp"

#include "Allocator.hpp"

#include "UploadContext.hpp"

#include <SDL3/SDL_vulkan.h>

#include <cassert>
#include <cstring>
#include <print>
#include <stdexcept>
#include <vector>

#ifndef VK_API_VERSION_1_4
#error Wrong Vulkan SDK!
#endif

#ifdef NDEBUG
static bool s_Validation = false;
#else
static bool s_Validation = true;
#endif

static Context* s_Instance = nullptr;

// ==== Vulkan Debug Utilities ====

constexpr const char* VkDebugUtilsMessageType(VkDebugUtilsMessageTypeFlagsEXT type)
{
	if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
		return "General";

	if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
		return "Validation";

	if (type & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
		return "Performance";

	return "Unknown";
}

constexpr const char* VkDebugUtilsMessageSeverity(VkDebugUtilsMessageSeverityFlagBitsEXT severity)
{
	switch (severity)
	{
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			return "Error";

		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			return "Warning";

		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			return "Info";

		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			return "Verbose";

		default:
			return "Unknown";
	}
}

static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugUtilsMessengerCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	(void)pUserData;

	if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
		return VK_FALSE;

	std::println("[Vulkan] {} {} message:", VkDebugUtilsMessageType(messageType), VkDebugUtilsMessageSeverity(messageSeverity));

	std::println("{}", pCallbackData->pMessage);

	if (pCallbackData->cmdBufLabelCount > 0)
	{
		std::println("Command Buffer Labels ({}):", pCallbackData->cmdBufLabelCount);

		for (uint32_t i = 0; i < pCallbackData->cmdBufLabelCount; ++i)
		{
			const auto& label = pCallbackData->pCmdBufLabels[i];

			std::println("  [{}] {}", i, label.pLabelName ? label.pLabelName : "NULL");
		}
	}

	if (pCallbackData->objectCount > 0)
	{
		std::println("Objects ({}):", pCallbackData->objectCount);

		for (uint32_t i = 0; i < pCallbackData->objectCount; ++i)
		{
			const auto& object = pCallbackData->pObjects[i];

			std::println("  [{}] {} - handle: {:#x}", i, object.pObjectName ? object.pObjectName : "NULL", object.objectHandle);
		}
	}

	return VK_FALSE;
}

static bool CheckDriverAPIVersionSupport(uint32_t minimumSupportedVersion)
{
	uint32_t instanceVersion = VK_API_VERSION_1_0;

	if (vkEnumerateInstanceVersion(&instanceVersion) != VK_SUCCESS)
	{
		std::println("Failed to query Vulkan instance version.");
		return false;
	}

	if (instanceVersion < minimumSupportedVersion)
	{
		std::println("Incompatible Vulkan driver version!");

		std::println("  You have {}.{}.{}", VK_API_VERSION_MAJOR(instanceVersion), VK_API_VERSION_MINOR(instanceVersion), VK_API_VERSION_PATCH(instanceVersion));
		std::println("  You need at least {}.{}.{}", VK_API_VERSION_MAJOR(minimumSupportedVersion), VK_API_VERSION_MINOR(minimumSupportedVersion), VK_API_VERSION_PATCH(minimumSupportedVersion));

		return false;
	}

	return true;
}

// ==== Initialize / Shutdown ====

void Context::Initialize()
{
	assert(!s_Instance && "Context already initialized!");

	s_Instance = new Context();

	if (volkInitialize() != VK_SUCCESS)
		throw std::runtime_error("Failed to initialize volk!");

	if (!CheckDriverAPIVersionSupport(VK_API_VERSION_1_4))
		throw std::runtime_error("Incompatible Vulkan driver. Update your GPU drivers!");

	s_Instance->CreateInstance();
	s_Instance->SetupDebugMessenger();
	s_Instance->PickPhysicalDevice();
	s_Instance->CreateLogicalDevice();

	{
		const VkCommandPoolCreateInfo poolInfo
		{
			.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
			.queueFamilyIndex = s_Instance->m_GraphicsFamily
		};

		VK_CHECK(vkCreateCommandPool(s_Instance->m_LogicalDevice, &poolInfo, nullptr, &s_Instance->m_ImmediatePool));

		const VkFenceCreateInfo fenceInfo
		{
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
		};

		VK_CHECK(vkCreateFence(s_Instance->m_LogicalDevice, &fenceInfo, nullptr, &s_Instance->m_ImmediateFence));
	}

	Allocator::Initialize();

	UploadContext::Initialize();
}

void Context::Shutdown()
{
	if (!s_Instance)
		return;

	if (s_Instance->m_LogicalDevice)
	{
		vkDeviceWaitIdle(s_Instance->m_LogicalDevice);

		UploadContext::Shutdown();

		Allocator::Shutdown();

		vkDestroyCommandPool(s_Instance->m_LogicalDevice, s_Instance->m_ImmediatePool, nullptr);
		vkDestroyFence(s_Instance->m_LogicalDevice, s_Instance->m_ImmediateFence, nullptr);

		s_Instance->m_ImmediatePool  = VK_NULL_HANDLE;
		s_Instance->m_ImmediateFence = VK_NULL_HANDLE;

		vkDestroyDevice(s_Instance->m_LogicalDevice, nullptr);

		s_Instance->m_LogicalDevice = VK_NULL_HANDLE;

		s_Instance->m_GraphicsQueue = VK_NULL_HANDLE;
		s_Instance->m_ComputeQueue  = VK_NULL_HANDLE;

		s_Instance->m_GraphicsFamily = UINT32_MAX;
		s_Instance->m_ComputeFamily  = UINT32_MAX;
	}

	if (s_Instance->m_DebugMessenger)
	{
		vkDestroyDebugUtilsMessengerEXT(s_Instance->m_VulkanInstance, s_Instance->m_DebugMessenger, nullptr);

		s_Instance->m_DebugMessenger = VK_NULL_HANDLE;
	}

	if (s_Instance->m_VulkanInstance)
	{
		vkDestroyInstance(s_Instance->m_VulkanInstance, nullptr);

		s_Instance->m_VulkanInstance = VK_NULL_HANDLE;
	}

	volkFinalize();

	delete s_Instance;
	s_Instance = nullptr;
}

Context& Context::Get()
{
	assert(s_Instance && "RendererContext is not initialized!");
	return *s_Instance;
}

// ==== Instance ====

void Context::CreateInstance()
{
	uint32_t sdlExtensionCount = 0;

	const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);

	if (!sdlExtensions)
	{
		throw std::runtime_error(std::string("SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError());
	}

	std::vector<const char*> instanceExtensions(sdlExtensions, sdlExtensions + sdlExtensionCount);

	if (s_Validation)
		instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	// === Validation Layers ===

	std::vector<const char*> requestedLayers;

	if (s_Validation)
	{
		constexpr const char* validationLayerName = "VK_LAYER_KHRONOS_validation";

		uint32_t layerCount = 0;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		bool found = false;

		for (const auto& layer : availableLayers)
		{
			if (std::strcmp(layer.layerName, validationLayerName) == 0)
			{
				found = true;
				break;
			}
		}

		if (found)
		{
			requestedLayers.push_back(validationLayerName);
		}
		else
		{
			std::println("[Renderer] Validation layer {} not found. Validation disabled.", validationLayerName);

			s_Validation = false;
		}
	}

	// === Instance Creation ===

	const VkApplicationInfo applicationInfo
	{
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "VkLab",
		.pEngineName = "VkLab",
		.apiVersion = VK_API_VERSION_1_4,
	};

	const VkDebugUtilsMessengerCreateInfoEXT debugInfo
	{
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		.pfnUserCallback = VulkanDebugUtilsMessengerCallback,
	};

	const VkInstanceCreateInfo instanceCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = s_Validation ? &debugInfo : nullptr,
		.pApplicationInfo = &applicationInfo,
		.enabledLayerCount = static_cast<uint32_t>(requestedLayers.size()),
		.ppEnabledLayerNames = requestedLayers.data(),
		.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size()),
		.ppEnabledExtensionNames = instanceExtensions.data(),
	};

	VK_CHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &m_VulkanInstance));

	volkLoadInstance(m_VulkanInstance);
}

// ==== Debug Messenger ====

void Context::SetupDebugMessenger()
{
	if (!s_Validation)
		return;

	const VkDebugUtilsMessengerCreateInfoEXT debugInfo
	{
		.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,

		.pfnUserCallback = VulkanDebugUtilsMessengerCallback,
	};

	VK_CHECK(vkCreateDebugUtilsMessengerEXT(m_VulkanInstance, &debugInfo, nullptr, &m_DebugMessenger));
}

// ==== Physical Device ====

void Context::PickPhysicalDevice()
{
	uint32_t gpuCount = 0;
	vkEnumeratePhysicalDevices(m_VulkanInstance, &gpuCount, nullptr);
	assert(gpuCount > 0 && "Could not find any physical devices!");
	std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
	VK_CHECK(vkEnumeratePhysicalDevices(m_VulkanInstance, &gpuCount, physicalDevices.data()));

	// Default to the first available GPU
	VkPhysicalDevice selectedPhysicalDevice = physicalDevices[0];

	// Prefer a discrete GPU if one is available
	for (VkPhysicalDevice physicalDevice : physicalDevices)
	{
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);

		if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			selectedPhysicalDevice = physicalDevice;
			break;
		}
	}

	m_PhysicalDevice = selectedPhysicalDevice;

	vkGetPhysicalDeviceProperties(m_PhysicalDevice, &m_PhysicalDeviceProperties);
	vkGetPhysicalDeviceFeatures(m_PhysicalDevice, &m_PhysicalDeviceFeatures);

	std::println("[Renderer] Selected GPU: {}", m_PhysicalDeviceProperties.deviceName);
}

// ==== Logical Device ====

void Context::CreateLogicalDevice()
{
	// === Queue Families ===
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);

	assert(queueFamilyCount > 0 && "Physical device has no queue families!");

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, queueFamilies.data());

	// === Graphics Queue Family ===
	for (uint32_t i = 0; i < queueFamilyCount; i++)
	{
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			m_GraphicsFamily = i;
			break;
		}
	}

	assert(m_GraphicsFamily != UINT32_MAX && "Could not find a graphics queue family!");

	// === Compute Queue Family ===

	// Prefer a dedicated compute queue family.
	for (uint32_t i = 0; i < queueFamilyCount; i++)
	{
		const VkQueueFlags flags = queueFamilies[i].queueFlags;

		if ((flags & VK_QUEUE_COMPUTE_BIT) &&
			!(flags & VK_QUEUE_GRAPHICS_BIT))
		{
			m_ComputeFamily = i;
			break;
		}
	}

	// Fall back to the graphics queue family.
	if (m_ComputeFamily == UINT32_MAX)
		m_ComputeFamily = m_GraphicsFamily;

	// === Queue Creation ===

	constexpr float queuePriority = 1.0f;

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::vector<uint32_t> uniqueQueueFamilies;

	const auto addQueueFamily = [&](uint32_t familyIndex)
	{
		for (uint32_t existingFamily : uniqueQueueFamilies)
		{
			if (existingFamily == familyIndex)
				return;
		}

		uniqueQueueFamilies.push_back(familyIndex);

		queueCreateInfos.push_back(
		{
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = familyIndex,
			.queueCount = 1,
			.pQueuePriorities = &queuePriority
		});
	};

	addQueueFamily(m_GraphicsFamily);
	addQueueFamily(m_ComputeFamily);

	// === Supported Features ===

	VkPhysicalDeviceShaderObjectFeaturesEXT supportedShaderObjectFeatures
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT,
		.pNext = nullptr
	};

	VkPhysicalDeviceVulkan14Features supportedFeatures14
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
		.pNext = &supportedShaderObjectFeatures
	};

	VkPhysicalDeviceVulkan13Features supportedFeatures13
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
		.pNext = &supportedFeatures14
	};

	VkPhysicalDeviceVulkan12Features supportedFeatures12
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
		.pNext = &supportedFeatures13
	};

	VkPhysicalDeviceVulkan11Features supportedFeatures11
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
		.pNext = &supportedFeatures12
	};

	VkPhysicalDeviceFeatures2 supportedFeatures
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		.pNext = &supportedFeatures11
	};

	vkGetPhysicalDeviceFeatures2(m_PhysicalDevice, &supportedFeatures);

	assert(supportedShaderObjectFeatures.shaderObject && "Shader objects are not supported!");

	assert(supportedFeatures11.shaderDrawParameters && "Shader draw parameters are not supported!");
	assert(supportedFeatures13.dynamicRendering && "Dynamic rendering is not supported!");
	assert(supportedFeatures13.synchronization2 && "Synchronization2 is not supported!");

	assert(supportedFeatures12.timelineSemaphore && "Timeline semaphores are not supported!");
	assert(supportedFeatures12.bufferDeviceAddress && "Buffer device address is not supported!");

	assert(supportedFeatures12.runtimeDescriptorArray && "Runtime descriptor arrays are not supported!");
	assert(supportedFeatures12.descriptorBindingPartiallyBound && "Partially-bound descriptors are not supported!");
	assert(supportedFeatures12.descriptorBindingSampledImageUpdateAfterBind && "Sampled-image update-after-bind is not supported!");
	assert(supportedFeatures12.descriptorBindingStorageImageUpdateAfterBind && "Storage-image update-after-bind is not supported!");
	assert(supportedFeatures12.shaderSampledImageArrayNonUniformIndexing && "Non-uniform sampled image indexing is not supported!");

	assert(supportedFeatures.features.samplerAnisotropy && "Sampler Anisotropy is not supported!");
	assert(supportedFeatures.features.shaderInt64 && "Shader Int64 is not supported!");

	// === Enabled Features ===

	VkPhysicalDeviceShaderObjectFeaturesEXT shaderObjectFeatures
	{
		.sType        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT,
		.pNext        = nullptr,
		.shaderObject = VK_TRUE
	};

	VkPhysicalDeviceVulkan14Features features14
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
		.pNext = &shaderObjectFeatures
	};

	VkPhysicalDeviceVulkan13Features features13
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
		.pNext = &features14,
		.shaderDemoteToHelperInvocation = VK_TRUE,
		.synchronization2 = VK_TRUE,
		.dynamicRendering = VK_TRUE
	};

	VkPhysicalDeviceVulkan12Features features12
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
		.pNext = &features13,
		.shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
		.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
		.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE,
		.descriptorBindingPartiallyBound = VK_TRUE,
		.runtimeDescriptorArray = VK_TRUE,
		.scalarBlockLayout = VK_TRUE,
		.timelineSemaphore = VK_TRUE,
		.bufferDeviceAddress = VK_TRUE,
	};

	VkPhysicalDeviceVulkan11Features features11
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
		.pNext = &features12,
		.shaderDrawParameters = VK_TRUE
	};

	VkPhysicalDeviceFeatures2 features
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		.pNext = &features11,
		.features =
		{
			.samplerAnisotropy = VK_TRUE,
			.shaderInt64 = VK_TRUE
		}
	};

	// === Device Extensions ===

	uint32_t extensionCount = 0;
	VK_CHECK(vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount, nullptr));
	std::vector<VkExtensionProperties> extensions(extensionCount);
	VK_CHECK(vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount, extensions.data()));

	std::println("[Renderer] Physical device '{}' has {} extensions:", m_PhysicalDeviceProperties.deviceName, extensions.size());

	for (const auto& extension : extensions)
	{
		m_SupportedExtensions.emplace(extension.extensionName);
		std::println("[Renderer]   {}", extension.extensionName);
	}

	std::vector<const char*> deviceExtensions
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_EXT_SHADER_OBJECT_EXTENSION_NAME,
	};

	assert(IsExtensionSupported(VK_KHR_SWAPCHAIN_EXTENSION_NAME) && "VK_KHR_swapchain is not supported!");
	assert(IsExtensionSupported(VK_EXT_SHADER_OBJECT_EXTENSION_NAME) && "VK_EXT_shader_object is not supported!");

	if (IsExtensionSupported(VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
	{
		deviceExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);

		m_EnableDebugMarkers = true;
	}

	// === Device Creation ===

	const VkDeviceCreateInfo deviceCreateInfo
	{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &features,
		.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
		.pQueueCreateInfos = queueCreateInfos.data(),
		.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
		.ppEnabledExtensionNames = deviceExtensions.data(),
		.pEnabledFeatures = nullptr
	};

	VK_CHECK(vkCreateDevice(m_PhysicalDevice, &deviceCreateInfo, nullptr, &m_LogicalDevice));

	volkLoadDevice(m_LogicalDevice);

	// === Queues ===

	vkGetDeviceQueue(m_LogicalDevice, m_GraphicsFamily, 0, &m_GraphicsQueue);
	vkGetDeviceQueue(m_LogicalDevice, m_ComputeFamily, 0, &m_ComputeQueue);

	assert(m_GraphicsQueue && "Could not get graphics queue!");
	assert(m_ComputeQueue && "Could not get compute queue!");

	std::println("[Renderer] Graphics queue family: {}", m_GraphicsFamily);
	std::println("[Renderer] Compute queue family: {}", m_ComputeFamily);

	if (m_EnableDebugMarkers)
		std::println("[Renderer] Debug markers enabled.");
}

// === Extensions ===

bool Context::IsExtensionSupported(const std::string& extensionName) const
{
	return m_SupportedExtensions.find(extensionName) != m_SupportedExtensions.end();
}

// === Immediate Submissions ===

void Context::ImmediateSubmit(std::function<void(VkCommandBuffer)>&& fn)
{
	assert(m_ImmediatePool  != VK_NULL_HANDLE);
	assert(m_ImmediateFence != VK_NULL_HANDLE);

	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

	const VkCommandBufferAllocateInfo allocInfo
	{
		.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool        = m_ImmediatePool,
		.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};

	VK_CHECK(vkAllocateCommandBuffers(m_LogicalDevice, &allocInfo, &commandBuffer));

	const VkCommandBufferBeginInfo beginInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};

	VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

	fn(commandBuffer);

	VK_CHECK(vkEndCommandBuffer(commandBuffer));

	const VkCommandBufferSubmitInfo cmdInfo
	{
		.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
		.commandBuffer = commandBuffer
	};

	const VkSubmitInfo2 submitInfo
	{
		.sType                  = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.commandBufferInfoCount = 1,
		.pCommandBufferInfos    = &cmdInfo
	};

	VK_CHECK(vkResetFences(m_LogicalDevice, 1, &m_ImmediateFence));
	VK_CHECK(vkQueueSubmit2(m_GraphicsQueue, 1, &submitInfo, m_ImmediateFence));
	VK_CHECK(vkWaitForFences(m_LogicalDevice, 1, &m_ImmediateFence, VK_TRUE, UINT64_MAX));

	vkFreeCommandBuffers(m_LogicalDevice, m_ImmediatePool, 1, &commandBuffer);
}
