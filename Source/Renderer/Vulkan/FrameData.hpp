#pragma once

#include "CommandBuffer.hpp"
#include "Context.hpp"

#include <array>
#include <vector>

static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

struct FrameContext
{
	CommandPool   GraphicsCommandPool;
	CommandBuffer GraphicsCommandBuffer;

	std::vector<VkCommandBuffer> CommandBuffers;

	FrameContext()
	{
		CommandBuffers.reserve(16);
	}

	void AddCommandBuffer(VkCommandBuffer commandBuffer)
	{
		CommandBuffers.push_back(commandBuffer);
	}

	void Reset()
	{
		GraphicsCommandPool.Reset();
		CommandBuffers.clear();
	}
};

class FrameData
{
public:
	void Initialize()
	{
		Context& context = Context::Get();

		m_FrameIndex = 0;

		for (auto& frame : m_Frames)
		{
			frame.GraphicsCommandPool.Create(context.GetGraphicsFamily());
			frame.GraphicsCommandBuffer = frame.GraphicsCommandPool.AllocateCommandBuffer();
			frame.CommandBuffers.clear();
		}
	}

	void Shutdown()
	{
		for (auto& frame : m_Frames)
		{
			frame.GraphicsCommandPool.Destroy();
			frame.GraphicsCommandBuffer = {};
			frame.CommandBuffers.clear();
		}

		m_FrameIndex = 0;
	}

	FrameContext&       Current()       { return m_Frames[m_FrameIndex % MAX_FRAMES_IN_FLIGHT]; }
	const FrameContext& Current() const { return m_Frames[m_FrameIndex % MAX_FRAMES_IN_FLIGHT]; }

	FrameContext&       Previous()       { return m_Frames[(m_FrameIndex + MAX_FRAMES_IN_FLIGHT - 1) % MAX_FRAMES_IN_FLIGHT]; }
	const FrameContext& Previous() const { return m_Frames[(m_FrameIndex + MAX_FRAMES_IN_FLIGHT - 1) % MAX_FRAMES_IN_FLIGHT]; }

	FrameContext&       GetFrame(uint64_t index)       { return m_Frames[index % MAX_FRAMES_IN_FLIGHT]; }
	const FrameContext& GetFrame(uint64_t index) const { return m_Frames[index % MAX_FRAMES_IN_FLIGHT]; }

	uint64_t GetFrameIndex() const { return m_FrameIndex; }

	void Advance() { m_FrameIndex++; }

private:
	std::array<FrameContext, MAX_FRAMES_IN_FLIGHT> m_Frames;
	uint64_t m_FrameIndex = 0;
};
