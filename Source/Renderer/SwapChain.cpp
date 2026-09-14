#include "SwapChain.hpp"

#include "RendererContext.hpp"

#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_events.h>

#include <vma/vk_mem_alloc.h>

#include <cassert>
#include <algorithm>
#include <print>

SwapChain::SwapChain(SDL_Window* windowHandle)
		: m_WindowHandle(windowHandle)
{
}

SwapChain::~SwapChain()
{
	Cleanup();
	vkDestroySurfaceKHR(RendererContext::Get().GetInstance(), m_Surface, nullptr);
}

void SwapChain::Initialize()
{
	assert(m_WindowHandle && "SwapChain window handle is null!");

	CreateSurface();

	// Block on (0,0) extent (launched-minimized); Vulkan rejects zero-sized swapchains.
	int windowWidth = 0;
	int windowHeight = 0;

	while (windowWidth == 0 || windowHeight == 0)
	{
		SDL_GetWindowSizeInPixels(m_WindowHandle, &windowWidth, &windowHeight);

		if (windowWidth == 0 || windowHeight == 0)
			SDL_WaitEvent(nullptr);
	}

	uint32_t width = static_cast<uint32_t>(windowWidth);
	uint32_t height = static_cast<uint32_t>(windowHeight);

	CreateSwapchain(&width, &height);
	CreateImageViews();
}

void SwapChain::Cleanup()
{
	VkDevice device = RendererContext::Get().GetDevice();

	for (auto& image : m_Images)
	{
		if (image.ImageView)
		{
			vkDestroyImageView(device, image.ImageView, nullptr);
			image.ImageView = VK_NULL_HANDLE;
		}
	}
	m_Images.clear();

	if (m_SwapChain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(device, m_SwapChain, nullptr);
		m_SwapChain = VK_NULL_HANDLE;
	}
}

void SwapChain::OnResize(uint32_t width, uint32_t height)
{
	if (width == 0 || height == 0)
		return;

	vkDeviceWaitIdle(RendererContext::Get().GetDevice());

	// Destroy image views but keep the swapchain handle alive
	// so it can be passed as oldSwapchain
	for (auto& image : m_Images)
	{
		vkDestroyImageView(RendererContext::Get().GetDevice(), image.ImageView, nullptr);
		image.ImageView = VK_NULL_HANDLE;
	}
	m_Images.clear();

	VkSwapchainKHR oldSwapchain = m_SwapChain;
	m_SwapChain = VK_NULL_HANDLE;

	CreateSwapchain(&width, &height, oldSwapchain);
	CreateImageViews();

	if (oldSwapchain != VK_NULL_HANDLE)
		vkDestroySwapchainKHR(RendererContext::Get().GetDevice(), oldSwapchain, nullptr);

	m_NeedsResize = false;
}

void SwapChain::CreateSurface()
{
	VkPhysicalDevice physicalDevice = RendererContext::Get().GetPhysicalDevice();

	SDL_Vulkan_CreateSurface(m_WindowHandle, RendererContext::Get().GetInstance(), nullptr, &m_Surface);

	VkBool32 presentSupport = VK_FALSE;

	VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, static_cast<uint32_t>(RendererContext::Get().GetGraphicsFamily()), m_Surface, &presentSupport));

	assert(presentSupport && "Graphics queue family does not support presentation on this surface!");

	FindImageFormatAndColorSpace();
}

void SwapChain::CreateSwapchain(uint32_t* width, uint32_t* height, VkSwapchainKHR oldSwapchain)
{
	VkPhysicalDevice physicalDevice = RendererContext::Get().GetPhysicalDevice();
	VkDevice device = RendererContext::Get().GetDevice();

	VkSurfaceCapabilitiesKHR capabilities{};
	VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_Surface, &capabilities));

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Present Mode
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	uint32_t presentModeCount = 0;
	VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentModeCount, nullptr));
	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentModeCount, presentModes.data()));

	// FIFO is always supported.
	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

	// Prefer mailbox when available.
	for (VkPresentModeKHR availablePresentMode : presentModes)
	{
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			presentMode = availablePresentMode;
			break;
		}
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Extent
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	if (capabilities.currentExtent.width != UINT32_MAX)
	{
		m_Extent = capabilities.currentExtent;

		*width = m_Extent.width;
		*height = m_Extent.height;
	}
	else
	{
		m_Extent =
		{
			.width = std::clamp(*width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
			.height = std::clamp(*height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
		};

		*width = m_Extent.width;
		*height = m_Extent.height;
	}

	if (*width == 0 || *height == 0)
		return;

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Image Count
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	uint32_t imageCount = std::max(2u, capabilities.minImageCount + 1);

	if (capabilities.maxImageCount > 0)
		imageCount = std::min(imageCount, capabilities.maxImageCount);

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Composite Alpha
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	constexpr VkCompositeAlphaFlagBitsKHR compositeAlphaFlags[]
	{
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
	};

	for (VkCompositeAlphaFlagBitsKHR flag : compositeAlphaFlags)
	{
		if (capabilities.supportedCompositeAlpha & flag)
		{
			compositeAlpha = flag;
			break;
		}
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Swapchain
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	// Useful for blitting/clearing to the swapchain.
	if (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	VkSwapchainCreateInfoKHR swapchainInfo
	{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = m_Surface,
		.minImageCount = imageCount,
		.imageFormat = m_ColorFormat,
		.imageColorSpace = m_ColorSpace,
		.imageExtent = m_Extent,
		.imageArrayLayers = 1,
		.imageUsage = imageUsage,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.preTransform = capabilities.currentTransform,
		.compositeAlpha = compositeAlpha,
		.presentMode = presentMode,
		.clipped = VK_TRUE,
		.oldSwapchain = oldSwapchain
	};

	VK_CHECK(vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &m_SwapChain));

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Images
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	VK_CHECK(vkGetSwapchainImagesKHR(device, m_SwapChain, &imageCount, nullptr));
	std::vector<VkImage> images(imageCount);
	VK_CHECK(vkGetSwapchainImagesKHR(device, m_SwapChain, &imageCount, images.data()));
	m_Images.resize(imageCount);

	for (uint32_t i = 0; i < imageCount; i++)
		m_Images[i].Image = images[i];
}

void SwapChain::CreateImageViews()
{
	VkDevice device = RendererContext::Get().GetDevice();

	for (auto& image : m_Images)
	{
		VkImageViewCreateInfo createInfo
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = image.Image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = m_ColorFormat,
			.components =
			{
				.r = VK_COMPONENT_SWIZZLE_IDENTITY,
				.g = VK_COMPONENT_SWIZZLE_IDENTITY,
				.b = VK_COMPONENT_SWIZZLE_IDENTITY,
				.a = VK_COMPONENT_SWIZZLE_IDENTITY
			},
			.subresourceRange =
			{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			}
		};

		VK_CHECK(vkCreateImageView(device, &createInfo, nullptr, &image.ImageView));
	}
}

uint32_t SwapChain::AcquireNextImage(VkSemaphore signalSemaphore)
{
	// Consume a deferred rebuild requested by Present().
	if (m_NeedsResize)
	{
		int windowWidth = 0;
		int windowHeight = 0;

		SDL_GetWindowSizeInPixels(m_WindowHandle, &windowWidth, &windowHeight);

		if (windowWidth > 0 && windowHeight > 0)
		{
			OnResize(static_cast<uint32_t>(windowWidth), static_cast<uint32_t>(windowHeight));
		}

		m_NeedsResize = false;

		return UINT32_MAX;
	}

	VkResult result = vkAcquireNextImageKHR(RendererContext::Get().GetDevice(), m_SwapChain, UINT64_MAX, signalSemaphore, VK_NULL_HANDLE, &m_CurrentImageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		int windowWidth = 0;
		int windowHeight = 0;

		SDL_GetWindowSizeInPixels(m_WindowHandle, &windowWidth, &windowHeight);

		if (windowWidth > 0 && windowHeight > 0)
		{
			OnResize(static_cast<uint32_t>(windowWidth), static_cast<uint32_t>(windowHeight));
		}

		return UINT32_MAX;
	}

	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		VK_CHECK(result);
		return UINT32_MAX;
	}

	return m_CurrentImageIndex;
}

void SwapChain::Present(VkSemaphore waitSemaphore)
{
	VkPresentInfoKHR presentInfo
	{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &waitSemaphore,
		.swapchainCount = 1,
		.pSwapchains = &m_SwapChain,
		.pImageIndices = &m_CurrentImageIndex
	};

	VkResult result = vkQueuePresentKHR(RendererContext::Get().GetGraphicsQueue(), &presentInfo);

	// Defer recreation until AcquireNextImage().
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		m_NeedsResize = true;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		VK_CHECK(result);
	}
}

void SwapChain::FindImageFormatAndColorSpace()
{
	VkPhysicalDevice physicalDevice = RendererContext::Get().GetPhysicalDevice();

	uint32_t formatCount;
	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, nullptr));
	assert(formatCount > 0);
	std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, surfaceFormats.data()));

	// No preferred format — pick our ideal default.
	if (formatCount == 1 && surfaceFormats[0].format == VK_FORMAT_UNDEFINED)
	{
		m_ColorFormat = VK_FORMAT_B8G8R8A8_UNORM;
		m_Format      = Format::BGRA8_UNorm;
		m_ColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		return;
	}

	// Prefer BGRA8 UNORM
	for (const auto& surfaceFormat : surfaceFormats)
	{
		if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM)
		{
			m_ColorFormat = surfaceFormat.format;
			m_Format      = Format::BGRA8_UNorm;
			m_ColorSpace  = surfaceFormat.colorSpace;
			return;
		}
	}

	// RGBA8 UNORM fallback
	for (const auto& surfaceFormat : surfaceFormats)
	{
		if (surfaceFormat.format == VK_FORMAT_R8G8B8A8_UNORM)
		{
			m_ColorFormat = surfaceFormat.format;
			m_Format      = Format::RGBA8_UNorm;
			m_ColorSpace  = surfaceFormat.colorSpace;
			return;
		}
	}

	// No suitable format found
	std::println("[SwapChain] No suitable surface format found. Available formats:");
	for (const auto& surfaceFormat : surfaceFormats)
		std::println("  format={} colorSpace={}", static_cast<uint32_t>(surfaceFormat.format), static_cast<uint32_t>(surfaceFormat.colorSpace));

	assert(false && "SwapChain: No suitable surface format found");
}
