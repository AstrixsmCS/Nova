#include "Texture.hpp"

#include "VulkanUtils.hpp"

#include "Context.hpp"

#include "UploadContext.hpp"

#include <stb_image.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <format>

void Texture2D::Create(const TextureSpecification& specification)
{
	assert(specification.Size.Width > 0);
	assert(specification.Size.Height > 0);
	assert(specification.Size.Depth == 1);

	Destroy();

	m_Specification = specification;

	ImageSpecification imageSpec
	{
		.DebugName = specification.DebugName,
		.Format    = specification.Format,
		.Usage     = specification.Usage,
		.Size      = specification.Size,
		.Mips      = specification.GenerateMips ? Utils::CalculateMipCount(specification.Size.Width, specification.Size.Height) : 1,
		.Transfer  = true
	};

	m_Image.Create(imageSpec);
}

void Texture2D::Create(const TextureSpecification& specification, const void* data)
{
	Create(specification);

	if (data)
	{
		const size_t byteSize = static_cast<size_t>(specification.Size.Width) * specification.Size.Height * Utils::GetFormatBytesPerPixel(specification.Format);

		SetData(data, byteSize);
	}

	if (specification.GenerateMips && m_Image.GetMipCount() > 1)
		GenerateMips();
}

bool Texture2D::Load(const std::filesystem::path& path, bool sRGB)
{
	int width    = 0;
	int height   = 0;
	int channels = 0;

	stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);

	if (!pixels || width <= 0 || height <= 0)
	{
		std::println("[Texture2D] stbi_load failed for '{}': {}", path.string(), stbi_failure_reason());

		if (pixels)
			stbi_image_free(pixels);

		return false;
	}

	const TextureSpecification spec
	{
		.DebugName    = path.filename().string(),
		.Format       = sRGB ? Format::RGBA8_SRGB : Format::RGBA8_UNorm,
		.Usage        = ImageUsage::Texture,
		.GenerateMips = true,
		.Size =
		{
			.Width  = static_cast<uint32_t>(width),
			.Height = static_cast<uint32_t>(height),
		}
	};

	Create(spec, pixels);

	stbi_image_free(pixels);

	return IsValid();
}

void Texture2D::Destroy()
{
	m_Image.Destroy();
}

void Texture2D::SetData(const void* data, size_t size)
{
	assert(data && size > 0);
	assert(m_Image.IsValid());

	const VkImageSubresourceRange mipZero
	{
		.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
		.baseMipLevel   = 0,
		.levelCount     = 1,
		.baseArrayLayer = 0,
		.layerCount     = 1
	};

	const VkBufferImageCopy copyRegion
	{
		.imageSubresource =
		{
			.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel       = 0,
			.baseArrayLayer = 0,
			.layerCount     = 1
		},
		.imageExtent = ToVulkan(m_Image.GetDimensions())
	};

	const VkImageLayout finalLayout = m_Image.GetMipCount() > 1 ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	UploadContext::Get().UploadImage(m_Image.GetHandle(), data, static_cast<VkDeviceSize>(size), copyRegion, mipZero, finalLayout);
}

void Texture2D::GenerateMips()
{
	const uint32_t mipCount = m_Image.GetMipCount();
	assert(mipCount > 1);

	const VkImage image     = m_Image.GetHandle();
	const int32_t fullWidth = static_cast<int32_t>(m_Image.GetWidth());
	const int32_t fullHeight= static_cast<int32_t>(m_Image.GetHeight());

	Context::Get().ImmediateSubmit([&](VkCommandBuffer cmd)
	{
		int32_t mipWidth  = fullWidth;
		int32_t mipHeight = fullHeight;

		for (uint32_t mip = 1; mip < mipCount; ++mip)
		{
			const int32_t nextWidth  = std::max(mipWidth  / 2, 1);
			const int32_t nextHeight = std::max(mipHeight / 2, 1);

			const VkImageSubresourceRange dstRange
			{
				.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel   = mip,
				.levelCount     = 1,
				.baseArrayLayer = 0,
				.layerCount     = 1
			};

			// UNDEFINED → TRANSFER_DST (dst mip)
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

			VkImageBlit blit{};
			blit.srcOffsets[1]  = { mipWidth,  mipHeight,  1 };
			blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mip - 1, 0, 1 };
			blit.dstOffsets[1]  = { nextWidth,  nextHeight,  1 };
			blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mip,     0, 1 };

			vkCmdBlitImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

			// TRANSFER_DST → TRANSFER_SRC (dst mip becomes next src)
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

		// All mips TRANSFER_SRC → SHADER_READ_ONLY
		const VkImageSubresourceRange allMips
		{
			.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel   = 0,
			.levelCount     = mipCount,
			.baseArrayLayer = 0,
			.layerCount     = 1
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

void TextureCube::Create(const TextureSpecification& specification)
{
	assert(specification.Size.Width > 0);
	assert(specification.Size.Height > 0);
	assert(specification.Size.Depth == 1);
	assert(specification.Size.Width == specification.Size.Height);

	Destroy();

	m_Specification = specification;
	m_MipCount      = specification.GenerateMips ? Utils::CalculateMipCount(specification.Size.Width, specification.Size.Height) : 1;

	VkDevice device = Context::Get().GetDevice();

	VkImageCreateInfo imageInfo = {};
	imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	imageInfo.imageType     = VK_IMAGE_TYPE_2D;
	imageInfo.format        = ToVulkan(specification.Format);
	imageInfo.extent        = ToVulkan(specification.Size);
	imageInfo.mipLevels     = m_MipCount;
	imageInfo.arrayLayers   = 6;
	imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	VK_CHECK(vmaCreateImage(Allocator::GetAllocator(), &imageInfo, &allocInfo, &m_Image, &m_Allocation, nullptr));

	SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_IMAGE, specification.DebugName, m_Image);

	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image                           = m_Image;
	viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_CUBE;
	viewInfo.format                          = ToVulkan(specification.Format);
	viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel   = 0;
	viewInfo.subresourceRange.levelCount     = m_MipCount;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount     = 6;

	VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &m_ImageView));

	SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_IMAGE_VIEW, std::format("{} default image view", specification.DebugName), m_ImageView);

	m_DescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	m_DescriptorInfo.imageView   = m_ImageView;

	if (specification.Usage == ImageUsage::Texture || specification.Usage == ImageUsage::Storage || specification.Usage == ImageUsage::Attachment)
		m_BindlessIndex = Descriptor::RegisterTexture(m_ImageView);

	if (specification.Usage == ImageUsage::Storage)
		m_StorageIndex = Descriptor::RegisterStorageImage(m_ImageView);

	m_LayerViews.resize(6, VK_NULL_HANDLE);

	for (uint32_t face = 0; face < 6; ++face)
	{
		VkImageViewCreateInfo faceViewInfo = {};
		faceViewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		faceViewInfo.image                           = m_Image;
		faceViewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
		faceViewInfo.format                          = ToVulkan(specification.Format);
		faceViewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
		faceViewInfo.subresourceRange.baseMipLevel   = 0;
		faceViewInfo.subresourceRange.levelCount     = m_MipCount;
		faceViewInfo.subresourceRange.baseArrayLayer = face;
		faceViewInfo.subresourceRange.layerCount     = 1;

		VK_CHECK(vkCreateImageView(device, &faceViewInfo, nullptr, &m_LayerViews[face]));

		SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_IMAGE_VIEW, std::format("{} image view layer: {}", specification.DebugName, face), m_LayerViews[face]);
	}
}

void TextureCube::Create(const TextureSpecification& specification, const void* data)
{
	assert(data);

	Create(specification);

	const size_t size = static_cast<size_t>(specification.Size.Width) * specification.Size.Height * 6 * Utils::GetFormatBytesPerPixel(specification.Format);

	SetData(data, size);
}

void TextureCube::Destroy()
{
	if (!IsValid())
		return;

	VkDevice device = Context::Get().GetDevice();

	for (auto& [mip, index] : m_MipStorageIndices)
	{
		if (index != Descriptor::INVALID_INDEX)
			Descriptor::UnregisterStorageImage(index);
	}
	m_MipStorageIndices.clear();

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

	for (VkImageView view : m_LayerViews)
		vkDestroyImageView(device, view, nullptr);
	m_LayerViews.clear();

	for (auto& [mip, view] : m_MipViews)
		vkDestroyImageView(device, view, nullptr);
	m_MipViews.clear();

	if (m_ImageView != VK_NULL_HANDLE)
	{
		vkDestroyImageView(device, m_ImageView, nullptr);
		m_ImageView = VK_NULL_HANDLE;
	}

	vmaDestroyImage(Allocator::GetAllocator(), m_Image, m_Allocation);
	m_Image          = VK_NULL_HANDLE;
	m_Allocation     = VK_NULL_HANDLE;
	m_DescriptorInfo = {};
}

VkImageView TextureCube::GetLayerView(uint32_t face) const
{
	assert(face < m_LayerViews.size());
	return m_LayerViews[face];
}

VkImageView TextureCube::GetMipView(uint32_t mip)
{
	assert(mip < m_MipCount);

	auto it = m_MipViews.find(mip);
	if (it != m_MipViews.end())
		return it->second;

	VkDevice device = Context::Get().GetDevice();

	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image                           = m_Image;
	viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	viewInfo.format                          = ToVulkan(m_Specification.Format);
	viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel   = mip;
	viewInfo.subresourceRange.levelCount     = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount     = 6;

	VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &m_MipViews[mip]));

	SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_IMAGE_VIEW, std::format("{} image view mip: {}", m_Specification.DebugName, mip), m_MipViews[mip]);

	return m_MipViews[mip];
}

uint32_t TextureCube::GetMipStorageIndex(uint32_t mip)
{
	assert(mip < m_MipCount);

	auto it = m_MipStorageIndices.find(mip);

	if (it != m_MipStorageIndices.end())
		return it->second;

	const VkImageView mipView = GetMipView(mip);

	const uint32_t index = Descriptor::RegisterStorageImage(mipView);

	m_MipStorageIndices[mip] = index;

	return index;
}

void TextureCube::SetData(const void* data, size_t size)
{
	assert(data && size > 0);
	assert(IsValid());

	const VkImageSubresourceRange mipZero
	{
		.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
		.baseMipLevel   = 0,
		.levelCount     = 1,
		.baseArrayLayer = 0,
		.layerCount     = 6
	};

	const VkBufferImageCopy copyRegion
	{
		.imageSubresource =
		{
			.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel       = 0,
			.baseArrayLayer = 0,
			.layerCount     = 6
		},
		.imageExtent = ToVulkan(m_Specification.Size)
	};

	const VkImageLayout finalLayout = m_MipCount > 1 ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	UploadContext::Get().UploadImage(m_Image, data, static_cast<VkDeviceSize>(size), copyRegion, mipZero, finalLayout);

	if (m_MipCount > 1)
		GenerateMips();
}

void TextureCube::GenerateMips()
{
	assert(m_MipCount > 1);

	const VkImage image     = m_Image;
	const int32_t fullWidth = static_cast<int32_t>(m_Specification.Size.Width);
	const int32_t fullHeight= static_cast<int32_t>(m_Specification.Size.Height);

	Context::Get().ImmediateSubmit([&](VkCommandBuffer cmd)
	{
		int32_t mipWidth  = fullWidth;
		int32_t mipHeight = fullHeight;

		for (uint32_t mip = 1; mip < m_MipCount; ++mip)
		{
			const int32_t nextWidth  = std::max(mipWidth  / 2, 1);
			const int32_t nextHeight = std::max(mipHeight / 2, 1);

			const VkImageSubresourceRange dstRange
			{
				.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel   = mip,
				.levelCount     = 1,
				.baseArrayLayer = 0,
				.layerCount     = 6
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

			for (uint32_t face = 0; face < 6; ++face)
			{
				VkImageBlit blit{};
				blit.srcOffsets[1]  = { mipWidth,  mipHeight,  1 };
				blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mip - 1, face, 1 };
				blit.dstOffsets[1]  = { nextWidth,  nextHeight,  1 };
				blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mip,     face, 1 };

				vkCmdBlitImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
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
			.levelCount     = m_MipCount,
			.baseArrayLayer = 0,
			.layerCount     = 6
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
