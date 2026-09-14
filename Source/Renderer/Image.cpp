#include "Image.hpp"

#include "RendererContext.hpp"

#include <cassert>
#include <format>

VkImageView Image::GetMipView(uint32_t mip)
{
	assert(mip < m_Specification.Mips);

	auto it = m_PerMipViews.find(mip);
	if (it != m_PerMipViews.end())
		return it->second;

	VkDevice device = RendererContext::Get().GetDevice();

	// Per-mip sampling view
	VkImageAspectFlags aspectMask = Utils::IsDepthFormat(m_Specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

	VkImageViewCreateInfo viewInfo                       = {};
	viewInfo.sType                                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image                                       = m_Info.Image;
	viewInfo.viewType                                    = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format                                      = ToVulkan(m_Specification.Format);
	viewInfo.subresourceRange.aspectMask                 = aspectMask;
	viewInfo.subresourceRange.baseMipLevel               = mip;
	viewInfo.subresourceRange.levelCount                 = 1;
	viewInfo.subresourceRange.baseArrayLayer             = 0;
	viewInfo.subresourceRange.layerCount                 = 1;

	VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &m_PerMipViews[mip]));
	SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_IMAGE_VIEW, std::format("{} image view mip: {}", m_Specification.DebugName, mip), m_PerMipViews[mip]);

	return m_PerMipViews[mip];
}

void Image2D::Create(const ImageSpecification& specification)
{
	assert(specification.Width > 0 && specification.Height > 0 && specification.Mips > 0);

	Destroy();

	m_Specification = specification;

	VkDevice device = RendererContext::Get().GetDevice();

	VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT; // TODO: this (probably) shouldn't be implied
	if (m_Specification.Usage == ImageUsage::Attachment)
	{
		if (Utils::IsDepthFormat(m_Specification.Format))
			usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		else
			usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}
	if (m_Specification.Transfer || m_Specification.Usage == ImageUsage::Texture)
	{
		usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}
	if (m_Specification.Usage == ImageUsage::Storage)
	{
		usage |= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}

	VkImageCreateInfo imageInfo  = {};
	imageInfo.sType              = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType          = VK_IMAGE_TYPE_2D;
	imageInfo.format             = ToVulkan(specification.Format);
	imageInfo.extent             = { specification.Width, specification.Height, 1 };
	imageInfo.mipLevels          = specification.Mips;
	imageInfo.arrayLayers        = 1;
	imageInfo.samples            = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling             = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage              = usage;
	imageInfo.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout      = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	VK_CHECK(vmaCreateImage(Allocator::GetAllocator(), &imageInfo, &allocInfo, &m_Info.Image, &m_Info.Allocation, nullptr));

	SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_IMAGE, specification.DebugName, m_Info.Image);

	// Color or depth-only sampling view
	VkImageAspectFlags samplingAspect = Utils::IsDepthFormat(specification.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

	VkImageViewCreateInfo viewInfo                       = {};
	viewInfo.sType                                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image                                       = m_Info.Image;
	viewInfo.viewType                                    = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format                                      = ToVulkan(specification.Format);
	viewInfo.subresourceRange.aspectMask                 = samplingAspect;
	viewInfo.subresourceRange.baseMipLevel               = 0;
	viewInfo.subresourceRange.levelCount                 = specification.Mips;
	viewInfo.subresourceRange.baseArrayLayer             = 0;
	viewInfo.subresourceRange.layerCount                 = 1;

	VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &m_Info.ImageView));
	SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_IMAGE_VIEW,
		std::format("{} default image view", specification.DebugName), m_Info.ImageView);

	if (Utils::IsDepthFormat(specification.Format) && Utils::HasStencil(specification.Format))
	{
		// Depth/stencil attachment view
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &m_Info.AttachmentView));
		SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_IMAGE_VIEW, std::format("{} attachment image view", specification.DebugName), m_Info.AttachmentView);

		// Stencil-only sampling view
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
		VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &m_Info.StencilView));
		SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_IMAGE_VIEW, std::format("{} stencil image view", specification.DebugName), m_Info.StencilView);
	}
	else
	{
		// Reuse the primary view when no stencil aspect exists
		m_Info.AttachmentView = m_Info.ImageView;
	}

	if (Utils::IsDepthFormat(specification.Format))
		m_DescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	else if (specification.Usage == ImageUsage::Storage)
		m_DescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	else
		m_DescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// Descriptor sampling view
	m_DescriptorInfo.imageView = m_Info.ImageView;

	if (specification.Usage == ImageUsage::Texture || specification.Usage == ImageUsage::Storage || specification.Usage == ImageUsage::Attachment)
	{
		const VkImageLayout bindlessLayout = Utils::IsDepthFormat(specification.Format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		m_BindlessIndex = Descriptor::RegisterTexture(m_Info.ImageView, bindlessLayout);
	}

	if (specification.Usage == ImageUsage::Storage)
		m_StorageIndex = Descriptor::RegisterStorageImage(m_Info.ImageView);
}

void Image2D::Destroy()
{
	if (!IsValid())
		return;

	VkDevice device = RendererContext::Get().GetDevice();

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

	for (auto& [mip, view] : m_PerMipViews)
		vkDestroyImageView(device, view, nullptr);
	m_PerMipViews.clear();

	// Destroy additional depth/stencil views
	if (m_Info.StencilView != VK_NULL_HANDLE)
	{
		vkDestroyImageView(device, m_Info.StencilView, nullptr);
		m_Info.StencilView = VK_NULL_HANDLE;
	}

	if (m_Info.AttachmentView != VK_NULL_HANDLE && m_Info.AttachmentView != m_Info.ImageView)
	{
		vkDestroyImageView(device, m_Info.AttachmentView, nullptr);
		m_Info.AttachmentView = VK_NULL_HANDLE;
	}

	vkDestroyImageView(device, m_Info.ImageView, nullptr);

	vmaDestroyImage(Allocator::GetAllocator(), m_Info.Image, m_Info.Allocation);
	m_Info           = {};
	m_DescriptorInfo = {};
}

void Image2D::Resize(uint32_t width, uint32_t height)
{
	if (width == GetWidth() && height == GetHeight())
		return;

	ImageSpecification specification = m_Specification;
	specification.Width  = width;
	specification.Height = height;
	Create(specification);
}

void ImageView::Create(const ImageViewSpecification& specification)
{
	Destroy();

	assert(specification.Image && specification.Image->IsValid());
	assert(specification.Mip < specification.Image->GetMipCount());

	m_Specification = specification;

	const Image2D&            src     = *specification.Image;
	const ImageSpecification& srcSpec = src.GetSpecification();

	VkDevice device = RendererContext::Get().GetDevice();

	// Color or depth-only sampling view
	VkImageAspectFlags aspectMask = Utils::IsDepthFormat(srcSpec.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

	VkImageViewCreateInfo viewInfo                       = {};
	viewInfo.sType                                       = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image                                       = src.GetHandle();
	viewInfo.viewType                                    = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format                                      = ToVulkan(srcSpec.Format);
	viewInfo.subresourceRange.aspectMask                 = aspectMask;
	viewInfo.subresourceRange.baseMipLevel               = specification.Mip;
	viewInfo.subresourceRange.levelCount                 = 1;
	viewInfo.subresourceRange.baseArrayLayer             = 0;
	viewInfo.subresourceRange.layerCount                 = 1;

	VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &m_ImageView));

	SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_IMAGE_VIEW, std::format("{} image view", specification.DebugName), m_ImageView);

	m_DescriptorInfo           = src.GetDescriptorInfo();
	m_DescriptorInfo.imageView = m_ImageView;
}

void ImageView::Destroy()
{
	if (m_ImageView == VK_NULL_HANDLE)
		return;

	vkDestroyImageView(RendererContext::Get().GetDevice(), m_ImageView, nullptr);
	m_ImageView      = VK_NULL_HANDLE;
	m_DescriptorInfo = {};
}
