#include "Texture.hpp"

#include "Allocator.hpp"
#include "RendererContext.hpp"

#include <stb_image.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <format>

void Texture2D::Create(const TextureSpecification& specification)
{
	assert(specification.Width > 0 && specification.Height > 0);

	Destroy();

	m_Specification = specification;

	ImageSpecification imageSpec
	{
		.DebugName = specification.DebugName,
		.Format    = specification.Format,
		.Usage     = specification.Usage,
		.Width     = specification.Width,
		.Height    = specification.Height,
		.Mips      = specification.GenerateMips ? Utils::CalculateMipCount(specification.Width, specification.Height) : 1,
		.Transfer  = true
	};

	m_Image.Create(imageSpec);
}

void Texture2D::Create(const TextureSpecification& specification, const void* data)
{
	assert(data);

	Create(specification);

	const size_t size = static_cast<size_t>(specification.Width) * specification.Height * Utils::GetFormatBytesPerPixel(specification.Format);

	SetData(data, size);
}

void Texture2D::Create(const TextureSpecification& specification, const std::filesystem::path& path)
{
	int width    = 0;
	int height   = 0;
	int channels = 0;

	TextureSpecification fileSpec = specification;

	const std::string pathStr = path.string();
	const bool        isHDR   = stbi_is_hdr(pathStr.c_str()) != 0;

	void* pixels = nullptr;

	if (isHDR)
	{
		pixels = stbi_loadf(pathStr.c_str(), &width, &height, &channels, STBI_rgb_alpha);
		fileSpec.Format = Format::RGBA32_Float;
	}
	else
	{
		pixels = stbi_load(pathStr.c_str(), &width, &height, &channels, STBI_rgb_alpha);
	}

	assert(pixels && "Failed to load texture!");

	if (fileSpec.DebugName.empty())
		fileSpec.DebugName = path.filename().string();

	fileSpec.Width  = static_cast<uint32_t>(width);
	fileSpec.Height = static_cast<uint32_t>(height);

	Create(fileSpec, pixels);

	stbi_image_free(pixels);
}

void Texture2D::Destroy()
{
	m_Image.Destroy();
}

void Texture2D::SetData(const void* data, size_t size)
{
	assert(data && size > 0);
	assert(m_Image.IsValid());

	VkBuffer      stagingBuffer     = VK_NULL_HANDLE;
	VmaAllocation stagingAllocation = VK_NULL_HANDLE;

	VkBufferCreateInfo stagingBufferInfo
	{
		.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size        = static_cast<VkDeviceSize>(size),
		.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	VmaAllocationCreateInfo stagingAllocInfo
	{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO
	};

	VK_CHECK(vmaCreateBuffer(Allocator::GetAllocator(), &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, nullptr));

	void* mapped = nullptr;
	VK_CHECK(vmaMapMemory(Allocator::GetAllocator(), stagingAllocation, &mapped));
	std::memcpy(mapped, data, size);
	vmaUnmapMemory(Allocator::GetAllocator(), stagingAllocation);

	CommandPool&  commandPool  = RendererContext::Get().GetImmediateCommandPool();
	CommandBuffer commandBuffer = commandPool.AllocateCommandBuffer();
	commandBuffer.Begin(true);

	VkImageSubresourceRange mipZero
	{
		.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
		.baseMipLevel   = 0,
		.levelCount     = 1,
		.baseArrayLayer = 0,
		.layerCount     = 1
	};

	SetImageLayout(commandBuffer.GetHandle(), m_Image.GetHandle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipZero);

	VkBufferImageCopy copyRegion
	{
		.imageSubresource =
		{
			.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel       = 0,
			.baseArrayLayer = 0,
			.layerCount     = 1
		},
		.imageExtent = { m_Image.GetWidth(), m_Image.GetHeight(), 1 }
	};

	vkCmdCopyBufferToImage(commandBuffer.GetHandle(), stagingBuffer, m_Image.GetHandle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

	const VkImageLayout afterCopyLayout = m_Image.GetMipCount() > 1 ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	SetImageLayout(commandBuffer.GetHandle(), m_Image.GetHandle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, afterCopyLayout, mipZero);

	commandBuffer.Flush();
	commandPool.Reset();

	vmaDestroyBuffer(Allocator::GetAllocator(), stagingBuffer, stagingAllocation);

	if (m_Image.GetMipCount() > 1)
		GenerateMips();
}

void Texture2D::GenerateMips()
{
	const uint32_t mipCount = m_Image.GetMipCount();
	assert(mipCount > 1);

	CommandPool&  commandPool  = RendererContext::Get().GetImmediateCommandPool();
	CommandBuffer commandBuffer = commandPool.AllocateCommandBuffer();
	commandBuffer.Begin(true);

	int32_t mipWidth  = static_cast<int32_t>(m_Image.GetWidth());
	int32_t mipHeight = static_cast<int32_t>(m_Image.GetHeight());

	for (uint32_t mip = 1; mip < mipCount; ++mip)
	{
		const int32_t nextWidth  = std::max(mipWidth  / 2, 1);
		const int32_t nextHeight = std::max(mipHeight / 2, 1);

		VkImageSubresourceRange dstRange
		{
			.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel   = mip,
			.levelCount     = 1,
			.baseArrayLayer = 0,
			.layerCount     = 1
		};

		SetImageLayout(commandBuffer.GetHandle(), m_Image.GetHandle(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, dstRange);

		VkImageBlit blit{};
		blit.srcOffsets[1]  = { mipWidth, mipHeight, 1 };
		blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mip - 1, 0, 1 };
		blit.dstOffsets[1]  = { nextWidth, nextHeight, 1 };
		blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mip, 0, 1 };

		vkCmdBlitImage(commandBuffer.GetHandle(),
		               m_Image.GetHandle(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		               m_Image.GetHandle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		               1, &blit, VK_FILTER_LINEAR);

		SetImageLayout(commandBuffer.GetHandle(), m_Image.GetHandle(),
		               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstRange);

		mipWidth  = nextWidth;
		mipHeight = nextHeight;
	}

	VkImageSubresourceRange allMips
	{
		.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
		.baseMipLevel   = 0,
		.levelCount     = mipCount,
		.baseArrayLayer = 0,
		.layerCount     = 1
	};

	SetImageLayout(commandBuffer.GetHandle(), m_Image.GetHandle(),
	               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, allMips);

	commandBuffer.Flush();
	commandPool.Reset();
}

void TextureCube::Create(const TextureSpecification& specification)
{
	assert(specification.Width > 0 && specification.Width == specification.Height);

	Destroy();

	m_Specification = specification;
	m_MipCount      = specification.GenerateMips ? Utils::CalculateMipCount(specification.Width, specification.Height) : 1;

	VkDevice device = RendererContext::Get().GetDevice();

	// Always all four usage flags.
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	imageInfo.imageType     = VK_IMAGE_TYPE_2D;
	imageInfo.format        = ToVulkan(specification.Format);
	imageInfo.extent        = { specification.Width, specification.Width, 1 };
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

	// Primary cube view: all mips, all six layers
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

	// Per-face 2D views:
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

	const size_t size = static_cast<size_t>(specification.Width) * specification.Height * 6 * Utils::GetFormatBytesPerPixel(specification.Format);

	SetData(data, size);
}

void TextureCube::Destroy()
{
	if (!IsValid())
		return;

	VkDevice device = RendererContext::Get().GetDevice();

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

	VkDevice device = RendererContext::Get().GetDevice();

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

	VkBuffer      stagingBuffer     = VK_NULL_HANDLE;
	VmaAllocation stagingAllocation = VK_NULL_HANDLE;

	VkBufferCreateInfo stagingBufferInfo
	{
		.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size        = static_cast<VkDeviceSize>(size),
		.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	VmaAllocationCreateInfo stagingAllocInfo
	{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO
	};

	VK_CHECK(vmaCreateBuffer(Allocator::GetAllocator(), &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, nullptr));

	void* mapped = nullptr;
	VK_CHECK(vmaMapMemory(Allocator::GetAllocator(), stagingAllocation, &mapped));
	std::memcpy(mapped, data, size);
	vmaUnmapMemory(Allocator::GetAllocator(), stagingAllocation);

	CommandPool&  commandPool  = RendererContext::Get().GetImmediateCommandPool();
	CommandBuffer commandBuffer = commandPool.AllocateCommandBuffer();
	commandBuffer.Begin(true);

	VkImageSubresourceRange mipZero
	{
		.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
		.baseMipLevel   = 0,
		.levelCount     = 1,
		.baseArrayLayer = 0,
		.layerCount     = 6
	};

	SetImageLayout(commandBuffer.GetHandle(), m_Image,
	               VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipZero);

	VkBufferImageCopy copyRegion
	{
		.imageSubresource =
		{
			.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
			.mipLevel       = 0,
			.baseArrayLayer = 0,
			.layerCount     = 6
		},
		.imageExtent = { m_Specification.Width, m_Specification.Height, 1 }
	};

	vkCmdCopyBufferToImage(commandBuffer.GetHandle(), stagingBuffer,
	                       m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	                       1, &copyRegion);

	const VkImageLayout afterCopyLayout = m_MipCount > 1 ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	SetImageLayout(commandBuffer.GetHandle(), m_Image,
	               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, afterCopyLayout, mipZero);

	commandBuffer.Flush();
	commandPool.Reset();

	vmaDestroyBuffer(Allocator::GetAllocator(), stagingBuffer, stagingAllocation);

	if (m_MipCount > 1)
		GenerateMips();
}

void TextureCube::GenerateMips()
{
	assert(m_MipCount > 1);

	CommandPool&  commandPool  = RendererContext::Get().GetImmediateCommandPool();
	CommandBuffer commandBuffer = commandPool.AllocateCommandBuffer();
	commandBuffer.Begin(true);

	int32_t mipWidth  = static_cast<int32_t>(m_Specification.Width);
	int32_t mipHeight = static_cast<int32_t>(m_Specification.Height);

	for (uint32_t mip = 1; mip < m_MipCount; ++mip)
	{
		const int32_t nextWidth  = std::max(mipWidth  / 2, 1);
		const int32_t nextHeight = std::max(mipHeight / 2, 1);

		VkImageSubresourceRange dstRange
		{
			.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel   = mip,
			.levelCount     = 1,
			.baseArrayLayer = 0,
			.layerCount     = 6
		};

		SetImageLayout(commandBuffer.GetHandle(), m_Image,
		               VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, dstRange);

		for (uint32_t face = 0; face < 6; ++face)
		{
			VkImageBlit blit{};
			blit.srcOffsets[1]  = { mipWidth, mipHeight, 1 };
			blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mip - 1, face, 1 };
			blit.dstOffsets[1]  = { nextWidth, nextHeight, 1 };
			blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, mip, face, 1 };

			vkCmdBlitImage(commandBuffer.GetHandle(),
			               m_Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			               m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			               1, &blit, VK_FILTER_LINEAR);
		}

		SetImageLayout(commandBuffer.GetHandle(), m_Image,
		               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstRange);

		mipWidth  = nextWidth;
		mipHeight = nextHeight;
	}

	VkImageSubresourceRange allMips
	{
		.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
		.baseMipLevel   = 0,
		.levelCount     = m_MipCount,
		.baseArrayLayer = 0,
		.layerCount     = 6
	};

	SetImageLayout(commandBuffer.GetHandle(), m_Image,
	               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, allMips);

	commandBuffer.Flush();
	commandPool.Reset();
}
