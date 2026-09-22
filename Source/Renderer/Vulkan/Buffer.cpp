#include "Buffer.hpp"

#include "Context.hpp"
#include "UploadContext.hpp"

#include <vma/vk_mem_alloc.h>

#include <cassert>
#include <cstring>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Vertex Buffer
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void VertexBuffer::Create(uint64_t size, VertexBufferUsage usage)
{
	assert(size > 0);

	m_Size = size;
	m_Usage = usage;

	VkBufferCreateInfo bufferInfo
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	VmaAllocationCreateInfo allocationInfo
	{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO
	};

	VK_CHECK(vmaCreateBuffer(Allocator::GetAllocator(), &bufferInfo, &allocationInfo, &m_Buffer, &m_Allocation, nullptr));
}

void VertexBuffer::Create(const void* data, uint64_t size, VertexBufferUsage usage)
{
	assert(data);
	assert(size > 0);

	m_Size = size;
	m_Usage = usage;

	VkBufferCreateInfo bufferInfo
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	VmaAllocationCreateInfo allocationInfo
	{
		.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
	};

	VK_CHECK(vmaCreateBuffer(Allocator::GetAllocator(), &bufferInfo, &allocationInfo, &m_Buffer, &m_Allocation, nullptr));

	UploadContext::Get().UploadBuffer(m_Buffer, data, size);
}

void VertexBuffer::Destroy()
{
	if (m_Buffer == VK_NULL_HANDLE)
		return;

	vmaDestroyBuffer(Allocator::GetAllocator(), m_Buffer, m_Allocation);

	m_Buffer = VK_NULL_HANDLE;
	m_Allocation = VK_NULL_HANDLE;

	m_Size = 0;
	m_Usage = VertexBufferUsage::None;
	m_Layout = {};
}

void VertexBuffer::SetData(const void* data, uint64_t size, uint64_t offset)
{
	assert(data);
	assert(size > 0);
	assert(offset + size <= m_Size);

	void* mappedData = nullptr;

	VK_CHECK(vmaMapMemory(Allocator::GetAllocator(), m_Allocation, &mappedData));

	std::memcpy(static_cast<uint8_t*>(mappedData) + offset, data, size);
	vmaUnmapMemory(Allocator::GetAllocator(), m_Allocation);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Index Buffer
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void IndexBuffer::Create(uint64_t size)
{
	assert(size > 0);

	m_Size = size;

	VkBufferCreateInfo bufferInfo
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	VmaAllocationCreateInfo allocationInfo
	{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO
	};

	VK_CHECK(vmaCreateBuffer(Allocator::GetAllocator(), &bufferInfo, &allocationInfo, &m_Buffer, &m_Allocation, nullptr));
}

void IndexBuffer::Create(const void* data, uint64_t size)
{
	assert(data);
	assert(size > 0);

	m_Size = size;

	VkBufferCreateInfo bufferInfo
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	VmaAllocationCreateInfo allocationInfo
	{
		.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
	};

	VK_CHECK(vmaCreateBuffer(Allocator::GetAllocator(), &bufferInfo, &allocationInfo, &m_Buffer, &m_Allocation, nullptr));

	UploadContext::Get().UploadBuffer(m_Buffer, data, size);
}

void IndexBuffer::Destroy()
{
	if (m_Buffer == VK_NULL_HANDLE)
		return;

	vmaDestroyBuffer(Allocator::GetAllocator(), m_Buffer, m_Allocation);

	m_Buffer = VK_NULL_HANDLE;
	m_Allocation = VK_NULL_HANDLE;

	m_Size = 0;
}

void IndexBuffer::SetData(const void* data, uint64_t size, uint64_t offset)
{
	assert(data);
	assert(size > 0);
	assert(offset + size <= m_Size);

	void* mappedData = nullptr;

	VK_CHECK(vmaMapMemory(Allocator::GetAllocator(), m_Allocation, &mappedData));

	std::memcpy(static_cast<uint8_t*>(mappedData) + offset, data, size);

	vmaUnmapMemory(Allocator::GetAllocator(), m_Allocation);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Uniform Buffer
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UniformBuffer::Create(uint32_t size)
{
	assert(size > 0);
	assert(m_Buffer == VK_NULL_HANDLE);

	m_Size = size;

	VkBufferCreateInfo bufferInfo
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	VmaAllocationCreateInfo allocationInfo
	{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO
	};

	VK_CHECK(vmaCreateBuffer(Allocator::GetAllocator(), &bufferInfo, &allocationInfo, &m_Buffer, &m_Allocation, nullptr));

	VkBufferDeviceAddressInfo addressInfo
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.buffer = m_Buffer
	};

	m_DeviceAddress = vkGetBufferDeviceAddress(Context::Get().GetDevice(), &addressInfo);

	assert(m_DeviceAddress != 0);
}

void UniformBuffer::Destroy()
{
	if (m_Buffer == VK_NULL_HANDLE)
		return;

	vmaDestroyBuffer(Allocator::GetAllocator(), m_Buffer, m_Allocation);

	m_Buffer = VK_NULL_HANDLE;
	m_Allocation = VK_NULL_HANDLE;

	m_DeviceAddress = 0;
	m_Size = 0;
}

void UniformBuffer::SetData(const void* data, uint32_t size, uint32_t offset)
{
	assert(data);
	assert(size > 0);
	assert(offset + size <= m_Size);

	void* mappedData = nullptr;

	VK_CHECK(vmaMapMemory(Allocator::GetAllocator(), m_Allocation, &mappedData));

	std::memcpy(static_cast<uint8_t*>(mappedData) + offset, data, size);

	vmaUnmapMemory(Allocator::GetAllocator(), m_Allocation);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Storage Buffer
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void StorageBuffer::Create(VkDeviceSize size)
{
	assert(size > 0);
	assert(m_Buffer == VK_NULL_HANDLE);

	m_Size = size;

	VkBufferCreateInfo bufferInfo
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = size,
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE
	};

	VmaAllocationCreateInfo allocationInfo
	{
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO
	};

	VK_CHECK(vmaCreateBuffer(Allocator::GetAllocator(), &bufferInfo, &allocationInfo, &m_Buffer, &m_Allocation, nullptr));

	VkBufferDeviceAddressInfo addressInfo
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.buffer = m_Buffer
	};

	m_DeviceAddress = vkGetBufferDeviceAddress(Context::Get().GetDevice(), &addressInfo);

	assert(m_DeviceAddress != 0);
}

void StorageBuffer::Destroy()
{
	if (m_Buffer == VK_NULL_HANDLE)
		return;

	vmaDestroyBuffer(Allocator::GetAllocator(), m_Buffer, m_Allocation);

	m_Buffer = VK_NULL_HANDLE;
	m_Allocation = VK_NULL_HANDLE;

	m_DeviceAddress = 0;
	m_Size = 0;
}

void StorageBuffer::SetData(const void* data, VkDeviceSize size, VkDeviceSize offset)
{
	assert(m_Buffer != VK_NULL_HANDLE);
	assert(data);
	assert(size > 0);

	assert(offset <= m_Size);
	assert(size <= m_Size - offset);

	void* mappedData = nullptr;

	VK_CHECK(vmaMapMemory(Allocator::GetAllocator(), m_Allocation, &mappedData));

	std::memcpy(static_cast<uint8_t*>(mappedData) + offset, data, static_cast<size_t>(size));

	vmaUnmapMemory(Allocator::GetAllocator(), m_Allocation);
}
