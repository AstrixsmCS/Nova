#include "CommandBuffer.hpp"

#include "Context.hpp"

#include <cassert>

CommandPool::~CommandPool()
{
	Destroy();
}

void CommandPool::Create(uint32_t queueFamilyIndex)
{
	assert(m_Handle == VK_NULL_HANDLE);

	VkDevice device = Context::Get().GetDevice();

	VkCommandPoolCreateInfo poolInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
		.queueFamilyIndex = queueFamilyIndex
	};

	VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &m_Handle));
}

void CommandPool::Destroy()
{
	if (m_Handle == VK_NULL_HANDLE)
		return;

	VkDevice device = Context::Get().GetDevice();

	vkDestroyCommandPool(device, m_Handle, nullptr);
	m_Handle = VK_NULL_HANDLE;
}

void CommandPool::Reset()
{
	assert(m_Handle != VK_NULL_HANDLE);

	VkDevice device = Context::Get().GetDevice();

	VK_CHECK(vkResetCommandPool(device, m_Handle, 0));
}

CommandBuffer CommandPool::AllocateCommandBuffer()
{
	assert(m_Handle != VK_NULL_HANDLE);

	VkDevice device = Context::Get().GetDevice();

	VkCommandBufferAllocateInfo allocInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = m_Handle,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};

	CommandBuffer commandBuffer;

	VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer.m_Handle));

	return commandBuffer;
}

void CommandBuffer::Begin(bool oneTimeSubmit)
{
	assert(m_Handle != VK_NULL_HANDLE);

	VkCommandBufferUsageFlags flags = 0;

	if (oneTimeSubmit)
		flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VkCommandBufferBeginInfo beginInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = flags
	};

	VK_CHECK(vkBeginCommandBuffer(m_Handle, &beginInfo));
}

void CommandBuffer::End()
{
	assert(m_Handle != VK_NULL_HANDLE);

	VK_CHECK(vkEndCommandBuffer(m_Handle));
}

void CommandBuffer::Flush()
{
	Flush(Context::Get().GetGraphicsQueue());
}

void CommandBuffer::Flush(VkQueue queue)
{
	assert(m_Handle != VK_NULL_HANDLE);
	assert(queue != VK_NULL_HANDLE);

	VkDevice device = Context::Get().GetDevice();

	End();

	VkCommandBufferSubmitInfo commandBufferInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
		.commandBuffer = m_Handle
	};

	VkSubmitInfo2 submitInfo
	{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.commandBufferInfoCount = 1,
		.pCommandBufferInfos = &commandBufferInfo
	};

	VkFenceCreateInfo fenceInfo
	{
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
	};

	VkFence fence = VK_NULL_HANDLE;
	VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &fence));

	VK_CHECK(vkQueueSubmit2(queue, 1, &submitInfo, fence));
	VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));

	vkDestroyFence(device, fence, nullptr);
}
