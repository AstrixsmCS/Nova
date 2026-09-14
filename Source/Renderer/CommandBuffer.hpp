#pragma once

#include "Vulkan.hpp"

#include <string>
#include <vector>

class CommandBuffer;

class CommandPool
{
public:
	CommandPool() = default;
	~CommandPool();

	void Create(uint32_t queueFamilyIndex);
	void Destroy();

	void Reset();

	CommandBuffer AllocateCommandBuffer();

	VkCommandPool GetHandle() const { return m_Handle; }

private:
	VkCommandPool m_Handle = VK_NULL_HANDLE;
};

class CommandBuffer
{
public:
	void Begin(bool oneTimeSubmit = false);
	void End();

	void Flush();
	void Flush(VkQueue queue);

	VkCommandBuffer GetHandle() const { return m_Handle; }
	bool IsValid() const { return m_Handle != VK_NULL_HANDLE; }

private:
	friend class CommandPool;

	VkCommandBuffer m_Handle = VK_NULL_HANDLE;
};
