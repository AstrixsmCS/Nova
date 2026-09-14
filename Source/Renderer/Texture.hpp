#pragma once

#include "Image.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

struct TextureSpecification
{
	std::string DebugName;

	Format     Format      = Format::RGBA8_UNorm;
	ImageUsage Usage       = ImageUsage::Texture;
	bool       GenerateMips = true;

	uint32_t Width  = 1;
	uint32_t Height = 1;
};

class Texture2D
{
public:
	Texture2D()  = default;
	~Texture2D() { Destroy(); }

	Texture2D(const Texture2D&)            = delete;
	Texture2D& operator=(const Texture2D&) = delete;

	void Create(const TextureSpecification& specification);
	void Create(const TextureSpecification& specification, const void* data);
	void Create(const TextureSpecification& specification, const std::filesystem::path& path);

	void Destroy();

	bool IsValid() const { return m_Image.IsValid(); }

	const Image2D& GetImage() const { return m_Image; }
	Image2D&       GetImage()       { return m_Image; }

	uint32_t GetWidth()    const { return m_Image.GetWidth();    }
	uint32_t GetHeight()   const { return m_Image.GetHeight();   }
	uint32_t GetMipCount() const { return m_Image.GetMipCount(); }
	Format GetFormat()   const { return m_Image.GetFormat();   }

	VkImageView                  GetView()           const { return m_Image.GetView();           }
	const VkDescriptorImageInfo& GetDescriptorInfo() const { return m_Image.GetDescriptorInfo(); }
	uint32_t                     GetBindlessIndex()  const { return m_Image.GetBindlessIndex();  }
	uint32_t                     GetStorageIndex()   const { return m_Image.GetStorageIndex();   }

	const TextureSpecification& GetSpecification() const { return m_Specification; }

	void GenerateMips();

private:
	void SetData(const void* data, size_t size);

	TextureSpecification m_Specification;
	Image2D              m_Image;
};

class TextureCube
{
public:
	TextureCube()  = default;
	~TextureCube() { Destroy(); }

	TextureCube(const TextureCube&)            = delete;
	TextureCube& operator=(const TextureCube&) = delete;

	void Create(const TextureSpecification& specification);
	void Create(const TextureSpecification& specification, const void* data);

	void Destroy();

	bool IsValid() const { return m_Image != VK_NULL_HANDLE; }

	uint32_t GetWidth()    const { return m_Specification.Width;  }
	uint32_t GetHeight()   const { return m_Specification.Height; }
	uint32_t GetMipCount() const { return m_MipCount;             }
	Format GetFormat()   const { return m_Specification.Format; }

	VkImage     GetHandle() const { return m_Image;     }
	VkImageView GetView()   const { return m_ImageView; }

	const VkDescriptorImageInfo& GetDescriptorInfo() const { return m_DescriptorInfo; }
	uint32_t                     GetBindlessIndex()  const { return m_BindlessIndex;  }
	uint32_t                     GetStorageIndex()   const { return m_StorageIndex;   }

	// Per-face 2D views
	VkImageView GetLayerView(uint32_t face) const;

	// Per-mip views
	VkImageView GetMipView(uint32_t mip);
	uint32_t    GetMipStorageIndex(uint32_t mip);

	const TextureSpecification& GetSpecification() const { return m_Specification; }

	void GenerateMips();

private:
	void SetData(const void* data, size_t size);

	TextureSpecification  m_Specification;

	VkImage               m_Image      = VK_NULL_HANDLE;
	VkImageView           m_ImageView  = VK_NULL_HANDLE;
	VmaAllocation         m_Allocation = VK_NULL_HANDLE;

	VkDescriptorImageInfo m_DescriptorInfo = {};

	uint32_t m_MipCount     = 1;
	uint32_t m_BindlessIndex = Descriptor::INVALID_INDEX;
	uint32_t m_StorageIndex  = Descriptor::INVALID_INDEX;

	std::vector<VkImageView>        m_LayerViews;
	std::map<uint32_t, VkImageView> m_MipViews;
	std::map<uint32_t, uint32_t>    m_MipStorageIndices;
};
