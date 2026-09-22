#pragma once

#include "Allocator.hpp"
#include "Context.hpp"
#include "Vulkan.hpp"

#include <vma/vk_mem_alloc.h>

#include <cassert>
#include <cstring>

class UploadContext
{
public:
	static constexpr VkDeviceSize STAGING_SIZE = 64ull * 1024ull * 1024ull; // 64 MB

	static void Initialize()
	{
		assert(!s_Instance);

		s_Instance = new UploadContext();

		UploadContext& upload = *s_Instance;

		const VkBufferCreateInfo bufferInfo
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = STAGING_SIZE,
			.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE
		};

		const VmaAllocationCreateInfo allocationInfo
		{
			.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
			.usage = VMA_MEMORY_USAGE_AUTO
		};

		VmaAllocationInfo allocationResult{};

		VK_CHECK(vmaCreateBuffer(Allocator::GetAllocator(), &bufferInfo, &allocationInfo, &upload.m_StagingBuffer, &upload.m_Allocation, &allocationResult));

		upload.m_MappedData = allocationResult.pMappedData;

		assert(upload.m_MappedData);
	}

	static void Shutdown()
	{
		if (!s_Instance)
			return;

		vmaDestroyBuffer(Allocator::GetAllocator(), s_Instance->m_StagingBuffer, s_Instance->m_Allocation);

		s_Instance->m_StagingBuffer = VK_NULL_HANDLE;
		s_Instance->m_Allocation = VK_NULL_HANDLE;
		s_Instance->m_MappedData = nullptr;

		delete s_Instance;
		s_Instance = nullptr;
	}

	static UploadContext& Get()
	{
		assert(s_Instance);
		return *s_Instance;
	}

	void UploadBuffer(VkBuffer destination, const void* data, VkDeviceSize size, VkDeviceSize destinationOffset = 0)
	{
		assert(destination != VK_NULL_HANDLE);
		assert(data);
		assert(size > 0);
		assert(size <= STAGING_SIZE && "Upload size exceeds staging buffer capacity.");

		std::memcpy(m_MappedData, data, static_cast<size_t>(size));

		Context::Get().ImmediateSubmit([&](VkCommandBuffer commandBuffer)
		{
			const VkBufferCopy2 region
			{
				.sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
				.srcOffset = 0,
				.dstOffset = destinationOffset,
				.size      = size
			};

			const VkCopyBufferInfo2 copyInfo
			{
				.sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
				.srcBuffer   = m_StagingBuffer,
				.dstBuffer   = destination,
				.regionCount = 1,
				.pRegions    = &region
			};

			vkCmdCopyBuffer2(commandBuffer, &copyInfo);
		});
	}

	void UploadImage(VkImage destination, const void* data, VkDeviceSize size, const VkBufferImageCopy2& copyRegion, const VkImageSubresourceRange& subresourceRange, VkImageLayout finalLayout)
	{
		assert(destination != VK_NULL_HANDLE);
		assert(data);
		assert(size > 0);
		assert(size <= STAGING_SIZE && "Upload size exceeds staging buffer capacity.");

		std::memcpy(m_MappedData, data, static_cast<size_t>(size));

		Context::Get().ImmediateSubmit([&](VkCommandBuffer commandBuffer)
		{
			const VkImageMemoryBarrier2 preCopy
			{
				.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.srcStageMask        = VK_PIPELINE_STAGE_2_NONE,
				.srcAccessMask       = VK_ACCESS_2_NONE,
				.dstStageMask        = VK_PIPELINE_STAGE_2_COPY_BIT,
				.dstAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
				.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image               = destination,
				.subresourceRange    = subresourceRange
			};

			const VkDependencyInfo preCopyDependency
			{
				.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.imageMemoryBarrierCount = 1,
				.pImageMemoryBarriers    = &preCopy
			};

			vkCmdPipelineBarrier2(commandBuffer, &preCopyDependency);

			const VkCopyBufferToImageInfo2 copyInfo
			{
				.sType          = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
				.srcBuffer      = m_StagingBuffer,
				.dstImage       = destination,
				.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.regionCount    = 1,
				.pRegions       = &copyRegion
			};

			vkCmdCopyBufferToImage2(commandBuffer, &copyInfo);

			const VkImageMemoryBarrier2 postCopy
			{
				.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.srcStageMask        = VK_PIPELINE_STAGE_2_COPY_BIT,
				.srcAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
				.dstStageMask        = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
				.dstAccessMask       = VK_ACCESS_2_MEMORY_READ_BIT,
				.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.newLayout           = finalLayout,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image               = destination,
				.subresourceRange    = subresourceRange
			};

			const VkDependencyInfo postCopyDependency
			{
				.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
				.imageMemoryBarrierCount = 1,
				.pImageMemoryBarriers    = &postCopy
			};

			vkCmdPipelineBarrier2(commandBuffer, &postCopyDependency);
		});
	}

private:
	UploadContext() = default;

private:
	inline static UploadContext* s_Instance = nullptr;

	VkBuffer      m_StagingBuffer = VK_NULL_HANDLE;
	VmaAllocation m_Allocation    = VK_NULL_HANDLE;

	void*         m_MappedData = nullptr;
};
