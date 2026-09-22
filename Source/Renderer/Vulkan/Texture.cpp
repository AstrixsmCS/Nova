#include "Texture.hpp"

#include "Allocator.hpp"
#include "Context.hpp"
#include "UploadContext.hpp"
#include "VulkanUtils.hpp"

#include <stb_image.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <format>
#include <print>

static VkComponentSwizzle ToVulkan(Swizzle swizzle)
{
	switch (swizzle)
	{
		case Swizzle::Default: return VK_COMPONENT_SWIZZLE_IDENTITY;
		case Swizzle::Zero:    return VK_COMPONENT_SWIZZLE_ZERO;
		case Swizzle::One:     return VK_COMPONENT_SWIZZLE_ONE;
		case Swizzle::R:       return VK_COMPONENT_SWIZZLE_R;
		case Swizzle::G:       return VK_COMPONENT_SWIZZLE_G;
		case Swizzle::B:       return VK_COMPONENT_SWIZZLE_B;
		case Swizzle::A:       return VK_COMPONENT_SWIZZLE_A;
	}
	return VK_COMPONENT_SWIZZLE_IDENTITY;
}

VkImageView VulkanImage::CreateView(VkDevice           device,
									 VkImageViewType    viewType,
									 VkFormat           format,
									 VkImageAspectFlags aspectMask,
									 uint32_t           baseMip,
									 uint32_t           mipCount,
									 uint32_t           baseLayer,
									 uint32_t           layerCount,
									 ComponentMapping   components,
									 const char*        debugName) const
{
	const VkImageViewCreateInfo info
	{
		.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image    = Image,
		.viewType = viewType,
		.format   = format,
		.components =
		{
			.r = ToVulkan(components.R),
			.g = ToVulkan(components.G),
			.b = ToVulkan(components.B),
			.a = ToVulkan(components.A)
		},
		.subresourceRange =
		{
			.aspectMask     = aspectMask,
			.baseMipLevel   = baseMip,
			.levelCount     = mipCount,
			.baseArrayLayer = baseLayer,
			.layerCount     = layerCount
		}
	};

	VkImageView view = VK_NULL_HANDLE;
	VK_CHECK(vkCreateImageView(device, &info, nullptr, &view));

	if (debugName)
		SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_IMAGE_VIEW, debugName, view);

	return view;
}

VkImageView VulkanImage::GetOrCreateMipLayerView(VkDevice device, uint32_t mip, uint32_t layer, const char* debugName)
{
	assert(mip   < MAX_MIP_LEVELS);
	assert(layer < MAX_CUBE_FACES);

	if (MipLayerViews[mip][layer] != VK_NULL_HANDLE)
		return MipLayerViews[mip][layer];

	MipLayerViews[mip][layer] = CreateView(
		device,
		VK_IMAGE_VIEW_TYPE_2D,
		Format,
		IsDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT,
		mip,   1,
		layer, 1,
		{},
		debugName);

	return MipLayerViews[mip][layer];
}

bool VulkanImage::IsDepthFormat(VkFormat format)
{
	switch (format)
	{
		case VK_FORMAT_D16_UNORM:
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_D32_SFLOAT:
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
		case VK_FORMAT_X8_D24_UNORM_PACK32:
			return true;
		default:
			return false;
	}
}

bool VulkanImage::IsStencilFormat(VkFormat format)
{
	switch (format)
	{
		case VK_FORMAT_S8_UINT:
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
			return true;
		default:
			return false;
	}
}

void Texture::Create(const TextureSpecification& specification)
{
	assert(specification.Size.Width   > 0);
	assert(specification.Size.Height  > 0);
	assert(specification.Size.Depth   > 0);
	assert(specification.NumMipLevels > 0);
	assert(specification.Usage        != 0);

	Destroy();

	m_Specification = specification;
	m_OwnsImage     = true;

	VkDevice device = Context::Get().GetDevice();

	const VkFormat vkFormat    = ToVulkan(specification.Format);
	const uint32_t arrayLayers = (specification.Type == TextureType::TextureCube) ? specification.NumLayers * 6 : specification.NumLayers;

	VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
	switch (specification.Type)
	{
		case TextureType::Texture2D:
			viewType = arrayLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
			break;
		case TextureType::Texture3D:
			viewType = VK_IMAGE_VIEW_TYPE_3D;
			break;
		case TextureType::TextureCube:
			viewType = arrayLayers > 6 ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
			break;
	}

	uint32_t mipLevels = specification.NumMipLevels;
	if (specification.GenerateMips && specification.Data)
		mipLevels = CalcMipCount(specification.Size.Width, specification.Size.Height);

	assert(mipLevels   <= MAX_MIP_LEVELS);
	assert(arrayLayers <= MAX_CUBE_FACES || specification.Type != TextureType::TextureCube);

	VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	if (specification.Usage & TextureUsageBits_Sampled)
		usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;

	if (specification.Usage & TextureUsageBits_Storage)
		usageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;

	if (specification.Usage & TextureUsageBits_Attachment)
	{
		usageFlags |= VulkanImage::IsDepthFormat(vkFormat)
			? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
			: VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}

	if (specification.Usage & TextureUsageBits_InputAttachment)
		usageFlags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

	const VkImageCreateInfo imageInfo
	{
		.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.flags         = (specification.Type == TextureType::TextureCube)
							? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
							: VkImageCreateFlags(0),
		.imageType     = (specification.Type == TextureType::Texture3D)
							? VK_IMAGE_TYPE_3D
							: VK_IMAGE_TYPE_2D,
		.format        = vkFormat,
		.extent        =
		{
			specification.Size.Width,
			specification.Size.Height,
			specification.Type == TextureType::Texture3D ? specification.Size.Depth : 1u
		},
		.mipLevels     = mipLevels,
		.arrayLayers   = arrayLayers,
		.samples       = VK_SAMPLE_COUNT_1_BIT,
		.tiling        = VK_IMAGE_TILING_OPTIMAL,
		.usage         = usageFlags,
		.sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};

	const VmaAllocationCreateInfo allocInfo { .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE };

	VK_CHECK(vmaCreateImage(Allocator::GetAllocator(), &imageInfo, &allocInfo, &m_Image.Image, &m_Image.Allocation, nullptr));

	m_Image.UsageFlags  = usageFlags;
	m_Image.Format      = vkFormat;
	m_Image.Extent      = imageInfo.extent;
	m_Image.MipLevels   = mipLevels;
	m_Image.ArrayLayers = arrayLayers;
	m_Image.IsDepth     = VulkanImage::IsDepthFormat(vkFormat);
	m_Image.IsStencil   = VulkanImage::IsStencilFormat(vkFormat);

	if (!specification.DebugName.empty())
		SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_IMAGE, specification.DebugName, m_Image.Image);

	const VkImageAspectFlags aspect = m_Image.IsDepth   ? VK_IMAGE_ASPECT_DEPTH_BIT
									: m_Image.IsStencil ? VK_IMAGE_ASPECT_STENCIL_BIT
									: VK_IMAGE_ASPECT_COLOR_BIT;

	// Default view uses the spec's component mapping
	m_DefaultView = m_Image.CreateView(
		device, viewType, vkFormat, aspect,
		0, VK_REMAINING_MIP_LEVELS,
		0, VK_REMAINING_ARRAY_LAYERS,
		specification.Components,
		specification.DebugName.empty() ? nullptr : (specification.DebugName + " view").c_str());

	// Storage view always identity swizzle
	if (usageFlags & VK_IMAGE_USAGE_STORAGE_BIT)
	{
		m_StorageView = m_Image.CreateView(
			device, viewType, vkFormat, aspect,
			0, VK_REMAINING_MIP_LEVELS,
			0, VK_REMAINING_ARRAY_LAYERS,
			{},
			specification.DebugName.empty() ? nullptr : (specification.DebugName + " storage view").c_str());
	}

	if (usageFlags & VK_IMAGE_USAGE_SAMPLED_BIT)
		m_BindlessIndex = Descriptor::RegisterTexture(m_DefaultView);

	if (usageFlags & VK_IMAGE_USAGE_STORAGE_BIT)
		m_StorageIndex = Descriptor::RegisterStorageImage(m_StorageView);

	if (specification.Data)
	{
		const size_t byteSize = static_cast<size_t>(specification.Size.Width) * specification.Size.Height * arrayLayers * GetFormatBytesPerPixel(specification.Format);
		SetData(specification.Data, byteSize);

		if (specification.GenerateMips && mipLevels > 1)
			GenerateMips();
	}
}

void Texture::CreateView(const Texture& source, const TextureViewSpecification& viewSpecification,
						 const std::string& debugName)
{
	assert(source.IsValid());

	Destroy();

	m_Image            = source.m_Image;
	m_Image.Allocation = VK_NULL_HANDLE;
	m_OwnsImage        = false;
	m_Specification    = source.m_Specification;

	VkDevice device = Context::Get().GetDevice();

	const uint32_t arrayLayers = (viewSpecification.Type == TextureType::TextureCube)
									 ? viewSpecification.NumLayers * 6
									 : viewSpecification.NumLayers;

	VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
	switch (viewSpecification.Type)
	{
		case TextureType::Texture2D:
			viewType = arrayLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
			break;
		case TextureType::Texture3D:
			viewType = VK_IMAGE_VIEW_TYPE_3D;
			break;
		case TextureType::TextureCube:
			viewType = arrayLayers > 6 ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
			break;
	}

	VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	switch (viewSpecification.Aspect)
	{
		case TextureAspect::Depth:   aspect = VK_IMAGE_ASPECT_DEPTH_BIT;   break;
		case TextureAspect::Stencil: aspect = VK_IMAGE_ASPECT_STENCIL_BIT; break;
		default:
			if (m_Image.IsDepth)   aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
			if (m_Image.IsStencil) aspect = VK_IMAGE_ASPECT_STENCIL_BIT;
			break;
	}

	// Default view — uses the view spec's component mapping
	m_DefaultView = m_Image.CreateView(
		device, viewType, m_Image.Format, aspect,
		viewSpecification.MipLevel, viewSpecification.NumMipLevels,
		viewSpecification.Layer,    viewSpecification.NumLayers,
		viewSpecification.Components,
		debugName.empty() ? nullptr : debugName.c_str());

	// Storage view — always identity swizzle
	if (m_Image.UsageFlags & VK_IMAGE_USAGE_STORAGE_BIT)
	{
		m_StorageView = m_Image.CreateView(
			device, viewType, m_Image.Format, aspect,
			viewSpecification.MipLevel, viewSpecification.NumMipLevels,
			viewSpecification.Layer,    viewSpecification.NumLayers,
			{},
			debugName.empty() ? nullptr : (debugName + " storage").c_str());
	}

	if (m_Image.UsageFlags & VK_IMAGE_USAGE_SAMPLED_BIT)
		m_BindlessIndex = Descriptor::RegisterTexture(m_DefaultView);

	if (m_Image.UsageFlags & VK_IMAGE_USAGE_STORAGE_BIT)
		m_StorageIndex = Descriptor::RegisterStorageImage(m_StorageView);
}

bool Texture::Load(const std::filesystem::path& path, bool sRGB)
{
	int w = 0, h = 0, channels = 0;

	stbi_uc* pixels = stbi_load(path.string().c_str(), &w, &h, &channels, STBI_rgb_alpha);

	if (!pixels || w <= 0 || h <= 0)
	{
		std::println("[Texture] stbi_load failed for '{}': {}", path.string(), stbi_failure_reason());
		if (pixels) stbi_image_free(pixels);
		return false;
	}

	Create(
	{
		.Type         = TextureType::Texture2D,
		.Format       = sRGB ? Format::RGBA8_SRGB : Format::RGBA8_UNorm,
		.Size         = { static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1 },
		.Data         = pixels,
		.GenerateMips = true,
		.DebugName    = path.filename().string()
	});

	stbi_image_free(pixels);
	return IsValid();
}

void Texture::Destroy()
{
	if (!m_Image.IsValid())
		return;

	VkDevice device = Context::Get().GetDevice();

	for (uint32_t mip = 0; mip < MAX_MIP_LEVELS; ++mip)
	{
		if (m_MipStorageIndices[mip] != 0 &&
			m_MipStorageIndices[mip] != Descriptor::INVALID_INDEX)
		{
			Descriptor::UnregisterStorageImage(m_MipStorageIndices[mip]);
			m_MipStorageIndices[mip] = 0;
		}
	}

	if (m_StorageIndex != Descriptor::INVALID_INDEX)
	{
		Descriptor::UnregisterStorageImage(m_StorageIndex);
		m_StorageIndex = Descriptor::INVALID_INDEX;
	}

	if (m_BindlessIndex != Descriptor::INVALID_INDEX)
	{
		Descriptor::UnregisterTexture(m_BindlessIndex);
		m_BindlessIndex = Descriptor::INVALID_INDEX;
	}

	for (uint32_t mip = 0; mip < MAX_MIP_LEVELS; ++mip)
	{
		if (m_MipViews[mip] != VK_NULL_HANDLE)
		{
			vkDestroyImageView(device, m_MipViews[mip], nullptr);
			m_MipViews[mip] = VK_NULL_HANDLE;
		}
	}

	for (uint32_t mip = 0; mip < MAX_MIP_LEVELS; ++mip)
		for (uint32_t layer = 0; layer < MAX_CUBE_FACES; ++layer)
			if (m_Image.MipLayerViews[mip][layer] != VK_NULL_HANDLE)
			{
				vkDestroyImageView(device, m_Image.MipLayerViews[mip][layer], nullptr);
				m_Image.MipLayerViews[mip][layer] = VK_NULL_HANDLE;
			}

	if (m_StorageView != VK_NULL_HANDLE)
	{
		vkDestroyImageView(device, m_StorageView, nullptr);
		m_StorageView = VK_NULL_HANDLE;
	}

	if (m_DefaultView != VK_NULL_HANDLE)
	{
		vkDestroyImageView(device, m_DefaultView, nullptr);
		m_DefaultView = VK_NULL_HANDLE;
	}

	if (m_OwnsImage)
		vmaDestroyImage(Allocator::GetAllocator(), m_Image.Image, m_Image.Allocation);

	m_Image         = {};
	m_Specification = {};
}

void Texture::SetData(const void* data, size_t size)
{
	assert(data && size > 0);

	const VkImageSubresourceRange mipZero
	{
		.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
		.baseMipLevel   = 0,
		.levelCount     = 1,
		.baseArrayLayer = 0,
		.layerCount     = m_Image.ArrayLayers
	};

	const VkBufferImageCopy2 region
	{
		.sType            = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
		.imageSubresource =
		{
			.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel       = 0,
			.baseArrayLayer = 0,
			.layerCount     = m_Image.ArrayLayers
		},
		.imageExtent = m_Image.Extent
	};

	const VkImageLayout finalLayout = m_Image.MipLevels > 1 ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	UploadContext::Get().UploadImage(m_Image.Image, data, static_cast<VkDeviceSize>(size), region, mipZero, finalLayout);
}

void Texture::GenerateMips()
{
	assert(m_Image.MipLevels > 1);

	const VkImage  image      = m_Image.Image;
	const uint32_t mipCount   = m_Image.MipLevels;
	const uint32_t layerCount = m_Image.ArrayLayers;
	const int32_t  fullWidth      = static_cast<int32_t>(m_Image.Extent.width);
	const int32_t  fullHeight      = static_cast<int32_t>(m_Image.Extent.height);

	Context::Get().ImmediateSubmit([&](VkCommandBuffer cmd)
	{
		int32_t mipWidth = fullWidth;
		int32_t mipHeight = fullHeight;

		for (uint32_t mip = 1; mip < mipCount; ++mip)
		{
			const int32_t nextWidth = std::max(mipWidth / 2, 1);
			const int32_t nextHeight = std::max(mipHeight / 2, 1);

			const VkImageSubresourceRange dstRange
			{
				.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel   = mip,
				.levelCount     = 1,
				.baseArrayLayer = 0,
				.layerCount     = layerCount
			};

			const VkImageMemoryBarrier2 toDst
			{
				.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.srcStageMask        = VK_PIPELINE_STAGE_2_NONE,
				.srcAccessMask       = VK_ACCESS_2_NONE,
				.dstStageMask        = VK_PIPELINE_STAGE_2_BLIT_BIT,
				.dstAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
				.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image               = image,
				.subresourceRange    = dstRange
			};

			const VkDependencyInfo toDstDep
			{
				.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.imageMemoryBarrierCount = 1,
				.pImageMemoryBarriers    = &toDst
			};

			vkCmdPipelineBarrier2(cmd, &toDstDep);

			for (uint32_t layer = 0; layer < layerCount; ++layer)
			{
				VkImageBlit blit{};
				blit.srcOffsets[1]  = { mipWidth, mipHeight, 1 };
				blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mip - 1, layer, 1 };
				blit.dstOffsets[1]  = { nextWidth, nextHeight, 1 };
				blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mip,     layer, 1 };

				vkCmdBlitImage(cmd,
								image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
								image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								1, &blit, VK_FILTER_LINEAR);
			}

			const VkImageMemoryBarrier2 toSrc
			{
				.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.srcStageMask        = VK_PIPELINE_STAGE_2_BLIT_BIT,
				.srcAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
				.dstStageMask        = VK_PIPELINE_STAGE_2_BLIT_BIT,
				.dstAccessMask       = VK_ACCESS_2_TRANSFER_READ_BIT,
				.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image               = image,
				.subresourceRange    = dstRange
			};

			const VkDependencyInfo toSrcDep
			{
				.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.imageMemoryBarrierCount = 1,
				.pImageMemoryBarriers    = &toSrc
			};

			vkCmdPipelineBarrier2(cmd, &toSrcDep);

			mipWidth  = nextWidth;
			mipHeight = nextHeight;
		}

		const VkImageSubresourceRange allMips
		{
			.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel   = 0,
			.levelCount     = mipCount,
			.baseArrayLayer = 0,
			.layerCount     = layerCount
		};

		const VkImageMemoryBarrier2 toReadOnly
		{
			.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask        = VK_PIPELINE_STAGE_2_BLIT_BIT,
			.srcAccessMask       = VK_ACCESS_2_TRANSFER_READ_BIT,
			.dstStageMask        = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
			.dstAccessMask       = VK_ACCESS_2_SHADER_READ_BIT,
			.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			.newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image               = image,
			.subresourceRange    = allMips
		};

		const VkDependencyInfo finalDep
		{
			.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers    = &toReadOnly
		};

		vkCmdPipelineBarrier2(cmd, &finalDep);
	});
}

// ==== Per-mip / per-layer views ====

VkImageView Texture::GetMipLayerView(uint32_t mip, uint32_t layer)
{
	assert(mip   < m_Image.MipLevels   && mip   < MAX_MIP_LEVELS);
	assert(layer < m_Image.ArrayLayers && layer < MAX_CUBE_FACES);

	const std::string name = m_Specification.DebugName.empty() ? "" : std::format("{} mip {} layer {}", m_Specification.DebugName, mip, layer);

	return m_Image.GetOrCreateMipLayerView(Context::Get().GetDevice(), mip, layer, name.empty() ? nullptr : name.c_str());
}

VkImageView Texture::GetMipView(uint32_t mip)
{
	assert(mip < m_Image.MipLevels && mip < MAX_MIP_LEVELS);

	if (m_MipViews[mip] != VK_NULL_HANDLE)
		return m_MipViews[mip];

	const std::string name = m_Specification.DebugName.empty() ? "" : std::format("{} mip {}", m_Specification.DebugName, mip);

	m_MipViews[mip] = m_Image.CreateView(
		Context::Get().GetDevice(),
		VK_IMAGE_VIEW_TYPE_2D_ARRAY,
		m_Image.Format,
		VK_IMAGE_ASPECT_COLOR_BIT,
		mip, 1,
		0,   m_Image.ArrayLayers,
		{},
		name.empty() ? nullptr : name.c_str());

	return m_MipViews[mip];
}

uint32_t Texture::GetMipStorageIndex(uint32_t mip)
{
	assert(mip < m_Image.MipLevels && mip < MAX_MIP_LEVELS);

	if (m_MipStorageIndices[mip] != 0 && m_MipStorageIndices[mip] != Descriptor::INVALID_INDEX)
		return m_MipStorageIndices[mip];

	const uint32_t index = Descriptor::RegisterStorageImage(GetMipView(mip));
	m_MipStorageIndices[mip] = index;
	return index;
}
