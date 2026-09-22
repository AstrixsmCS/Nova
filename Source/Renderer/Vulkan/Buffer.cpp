#include "Buffer.hpp"

#include "Allocator.hpp"
#include "Context.hpp"
#include "UploadContext.hpp"
#include "VulkanUtils.hpp"

#include <cassert>
#include <cstring>
#include <utility>

static VkBufferUsageFlags ToVulkan(BufferUsage usage)
{
	VkBufferUsageFlags flags = 0;

	if (HasFlag(usage, BufferUsage::Vertex))
		flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

	if (HasFlag(usage, BufferUsage::Index))
		flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

	if (HasFlag(usage, BufferUsage::Uniform))
		flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

	if (HasFlag(usage, BufferUsage::Storage))
		flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	if (HasFlag(usage, BufferUsage::Indirect))
		flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

	return flags;
}

Buffer::~Buffer()
{
	Destroy();
}

Buffer::Buffer(Buffer&& other) noexcept
{
	*this = std::move(other);
}

Buffer& Buffer::operator=(Buffer&& other) noexcept
{
	if (this == &other)
		return *this;

	Destroy();

	m_Specification = std::move(other.m_Specification);

	m_Handle = std::exchange(other.m_Handle, VK_NULL_HANDLE);
	m_Allocation = std::exchange(other.m_Allocation, VK_NULL_HANDLE);
	m_DeviceAddress = std::exchange(other.m_DeviceAddress, 0);
	m_MappedData = std::exchange(other.m_MappedData, nullptr);

	return *this;
}

void Buffer::Create(const BufferSpecification& specification)
{
	assert(specification.Size > 0);
	assert(specification.Usage != BufferUsage::None);
	assert(m_Handle == VK_NULL_HANDLE);

	m_Specification = specification;

	VkBufferUsageFlags usage = ToVulkan(specification.Usage);

	if (specification.Memory == BufferMemory::Device)
		usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	const bool needsDeviceAddress =
		HasFlag(specification.Usage, BufferUsage::Uniform) ||
		HasFlag(specification.Usage, BufferUsage::Storage) ||
		HasFlag(specification.Usage, BufferUsage::Indirect);

	if (needsDeviceAddress)
		usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

	const VkBufferCreateInfo bufferInfo
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = specification.Size,
		.usage = usage,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	VmaAllocationCreateInfo allocationInfo{};

	if (specification.Memory == BufferMemory::HostVisible)
	{
		allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
	}
	else
	{
		allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	}

	VmaAllocationInfo allocationResult{};

	VK_CHECK(vmaCreateBuffer(Allocator::GetAllocator(), &bufferInfo, &allocationInfo, &m_Handle, &m_Allocation, &allocationResult));

	if (specification.Memory == BufferMemory::HostVisible)
		m_MappedData = allocationResult.pMappedData;

	if (needsDeviceAddress)
	{
		const VkBufferDeviceAddressInfo addressInfo
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
			.buffer = m_Handle
		};

		m_DeviceAddress = vkGetBufferDeviceAddress(Context::Get().GetDevice(), &addressInfo);
	}

	if (!specification.DebugName.empty())
	{
		SetDebugUtilsObjectName(Context::Get().GetDevice(), VK_OBJECT_TYPE_BUFFER, specification.DebugName, m_Handle);
	}

	if (specification.Data)
	{
		SetData(specification.Data, specification.Size);
	}
}

void Buffer::Destroy()
{
	if (m_Handle == VK_NULL_HANDLE)
		return;

	vmaDestroyBuffer(Allocator::GetAllocator(), m_Handle, m_Allocation);

	m_Handle = VK_NULL_HANDLE;
	m_Allocation = VK_NULL_HANDLE;
	m_DeviceAddress = 0;
	m_MappedData = nullptr;

	m_Specification = {};
}

void Buffer::SetData(const void* data, VkDeviceSize size, VkDeviceSize offset)
{
	assert(m_Handle != VK_NULL_HANDLE);
	assert(data);
	assert(size > 0);

	assert(offset <= m_Specification.Size);
	assert(size <= m_Specification.Size - offset);

	if (m_Specification.Memory == BufferMemory::HostVisible)
	{
		assert(m_MappedData);

		std::memcpy(static_cast<std::byte*>(m_MappedData) + offset, data, static_cast<size_t>(size));

		return;
	}

	UploadContext::Get().UploadBuffer(m_Handle, data, size, offset);
}
