#pragma once

#include "Vulkan.hpp"
#include "Allocator.hpp"
#include "Descriptors.hpp"

#include <vma/vk_mem_alloc.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct ImageSpecification
{
	std::string DebugName;

	Format     Format = Format::RGBA8_UNorm;
	ImageUsage Usage  = ImageUsage::Texture;

	Dimensions Size = {};

	uint32_t Mips = 1;

	bool Transfer = false; // Will it be used for transfer ops?
};

struct ImageInfo
{
	VkImage       Image          = VK_NULL_HANDLE;
	VkImageView   ImageView      = VK_NULL_HANDLE; // Color or depth-only view
	VkImageView   AttachmentView = VK_NULL_HANDLE; // Depth/stencil attachment view
	VkImageView   StencilView   = VK_NULL_HANDLE; // Stencil-only view
	VmaAllocation Allocation     = VK_NULL_HANDLE;
};

namespace Utils
{
	inline bool IsDepthFormat(Format format)
	{
		switch (format)
		{
			case Format::D16_UNorm:
			case Format::D24_UNorm_S8_UInt:
			case Format::D32_Float:
			case Format::D32_Float_S8_UInt:
				return true;
			default:
				return false;
		}
	}

	inline bool HasStencil(Format format)
	{
		switch (format)
		{
			case Format::S8_UInt:
			case Format::D24_UNorm_S8_UInt:
			case Format::D32_Float_S8_UInt:
				return true;
			default:
				return false;
		}
	}

	inline uint32_t CalculateMipCount(uint32_t width, uint32_t height)
	{
		return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
	}

	inline uint32_t GetFormatBytesPerPixel(Format format)
	{
		switch (format)
		{
			case Format::R8_UNorm:
			case Format::R8_UInt:
				return 1;
			case Format::R16_UInt:
			case Format::R16_Float:
				return 2;
			case Format::R32_Float:
			case Format::R32_UInt:
			case Format::RG16_Float:
			case Format::RGBA8_UNorm:
			case Format::RGBA8_SRGB:
				return 4;
			case Format::RG32_Float:
			case Format::RGBA16_Float:
				return 8;
			case Format::RGBA32_Float:
				return 16;
			default:
				assert(false && "GetFormatBytesPerPixel: unsupported format");
				return 0;
		}
	}
}

class Image
{
public:
	Image()                        = default;
	Image(const Image&)            = delete;
	Image& operator=(const Image&) = delete;

	bool IsValid() const { return m_Info.Image != VK_NULL_HANDLE; }

	VkImage     GetHandle() const { return m_Info.Image; }

	VkImageView GetView()           const { return m_Info.ImageView;      } // Color or depth-only view
	VkImageView GetAttachmentView() const { return m_Info.AttachmentView; } // Attachment view
	VkImageView GetStencilView()    const { return m_Info.StencilView;    } // Stencil-only view

	const ImageInfo& GetImageInfo() const { return m_Info; }

	uint32_t GetWidth() const { return m_Specification.Size.Width; }
	uint32_t GetHeight() const { return m_Specification.Size.Height; }
	uint32_t GetDepth() const { return m_Specification.Size.Depth; }
	const Dimensions& GetDimensions() const { return m_Specification.Size; }
	uint32_t   GetMipCount() const { return m_Specification.Mips;   }
	Format     GetFormat()     const { return m_Specification.Format; }
	ImageUsage GetUsage()    const { return m_Specification.Usage;  }

	const ImageSpecification& GetSpecification() const { return m_Specification; }
	const VkDescriptorImageInfo& GetDescriptorInfo() const { return m_DescriptorInfo; }

	uint32_t GetBindlessIndex() const { return m_BindlessIndex; }
	uint32_t GetStorageIndex() const { return m_StorageIndex; }

	VkImageView GetMipView(uint32_t mip);

protected:
	ImageSpecification    m_Specification;
	ImageInfo             m_Info;
	VkDescriptorImageInfo m_DescriptorInfo = {};

	uint32_t m_BindlessIndex = Descriptor::INVALID_INDEX;
	uint32_t m_StorageIndex  = Descriptor::INVALID_INDEX;

	std::map<uint32_t, VkImageView> m_PerMipViews;
};

class Image2D : public Image
{
public:
	Image2D()  = default;
	~Image2D() { Destroy(); }

	Image2D(const Image2D&)            = delete;
	Image2D& operator=(const Image2D&) = delete;

	void Create(const ImageSpecification& specification);
	void Destroy();
	void Resize(const Dimensions& size);
};

struct ImageViewSpecification
{
	std::shared_ptr<Image2D> Image;
	uint32_t                 Mip = 0;
	std::string              DebugName;
};

class ImageView
{
public:
	ImageView()  = default;
	~ImageView() { Destroy(); }

	ImageView(const ImageView&)            = delete;
	ImageView& operator=(const ImageView&) = delete;

	void Create(const ImageViewSpecification& specification);
	void Destroy();

	bool        IsValid()   const { return m_ImageView != VK_NULL_HANDLE; }
	VkImageView GetHandle() const { return m_ImageView; }

	const VkDescriptorImageInfo&  GetDescriptorInfo() const { return m_DescriptorInfo; }
	const ImageViewSpecification& GetSpecification()  const { return m_Specification; }

private:
	ImageViewSpecification m_Specification;
	VkImageView            m_ImageView      = VK_NULL_HANDLE;
	VkDescriptorImageInfo  m_DescriptorInfo = {};
};
