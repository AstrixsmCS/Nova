#pragma once

#include "CommandBuffer.hpp"
#include "Context.hpp"
#include "TimelineSemaphore.hpp"

#include <array>
#include <cassert>
#include <vector>

static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

struct FrameSubmitInfo
{
	VkQueue     GraphicsQueue = VK_NULL_HANDLE;
	VkQueue     ComputeQueue  = VK_NULL_HANDLE;

	VkSemaphore ImageAvailable = VK_NULL_HANDLE;
	VkSemaphore RenderFinished = VK_NULL_HANDLE;

	VkSemaphore GraphicsTimeline = VK_NULL_HANDLE;
	VkSemaphore ComputeTimeline  = VK_NULL_HANDLE;

	uint64_t GraphicsSignalValue = 0;
	uint64_t ComputeSignalValue  = 0;

	// Optional cross-queue dependencies set by the render graph.
	// 0 means no dependency in that direction.
	uint64_t ComputeWaitGraphicsValue  = 0;
	uint64_t GraphicsWaitComputeValue  = 0;
};

struct FrameContext
{
	// Graphics
	CommandPool   GraphicsCommandPool;
	CommandBuffer GraphicsCommandBuffer;

	// Async compute
	CommandPool   ComputeCommandPool;
	CommandBuffer ComputeCommandBuffer;

	bool ComputeUsed = false;

	uint64_t GraphicsSignalValue = 0;
	uint64_t ComputeSignalValue  = 0;

	// Additional command buffers recorded during the frame.
	// The render graph will populate this before Submit() is called.
	std::vector<VkCommandBuffer> CommandBuffers;

	FrameContext()
	{
		CommandBuffers.reserve(16);
	}

	void AddCommandBuffer(VkCommandBuffer cmd)
	{
		CommandBuffers.push_back(cmd);
	}

	void Submit(const FrameSubmitInfo& info)
	{
		assert(info.GraphicsQueue    != VK_NULL_HANDLE);
		assert(info.ImageAvailable   != VK_NULL_HANDLE);
		assert(info.RenderFinished   != VK_NULL_HANDLE);
		assert(info.GraphicsTimeline != VK_NULL_HANDLE);
		assert(info.GraphicsSignalValue > 0);

		if (ComputeUsed)
			SubmitCompute(info);

		SubmitGraphics(info);

		GraphicsSignalValue = info.GraphicsSignalValue;
		ComputeSignalValue  = ComputeUsed ? info.ComputeSignalValue : 0;
	}

	void Reset(bool hasAsyncCompute)
	{
		GraphicsCommandPool.Reset();
		CommandBuffers.clear();
		m_SubmitInfos.clear();

		if (hasAsyncCompute)
			ComputeCommandPool.Reset();

		ComputeUsed = false;
	}

private:
	void SubmitCompute(const FrameSubmitInfo& info)
	{
		assert(info.ComputeQueue    != VK_NULL_HANDLE);
		assert(info.ComputeTimeline != VK_NULL_HANDLE);
		assert(info.ComputeSignalValue > 0);

		ComputeCommandBuffer.End();

		const VkCommandBufferSubmitInfo cmdInfo
		{
			.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
			.commandBuffer = ComputeCommandBuffer.GetHandle()
		};

		VkSemaphoreSubmitInfo waits[1];
		uint32_t waitCount = 0;

		if (info.ComputeWaitGraphicsValue > 0)
		{
			waits[waitCount++] =
			{
				.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.semaphore = info.GraphicsTimeline,
				.value     = info.ComputeWaitGraphicsValue,
				.stageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT
			};
		}

		const VkSemaphoreSubmitInfo signal
		{
			.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = info.ComputeTimeline,
			.value     = info.ComputeSignalValue,
			.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
		};

		const VkSubmitInfo2 submitInfo
		{
			.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
			.waitSemaphoreInfoCount   = waitCount,
			.pWaitSemaphoreInfos      = waitCount > 0 ? waits : nullptr,
			.commandBufferInfoCount   = 1,
			.pCommandBufferInfos      = &cmdInfo,
			.signalSemaphoreInfoCount = 1,
			.pSignalSemaphoreInfos    = &signal
		};

		VK_CHECK(vkQueueSubmit2(info.ComputeQueue, 1, &submitInfo, VK_NULL_HANDLE));
	}

	void SubmitGraphics(const FrameSubmitInfo& info)
	{
		GraphicsCommandBuffer.End();

		m_SubmitInfos.clear();

		m_SubmitInfos.push_back(
		{
			.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
			.commandBuffer = GraphicsCommandBuffer.GetHandle()
		});

		for (VkCommandBuffer cmd : CommandBuffers)
		{
			m_SubmitInfos.push_back(
			{
				.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
				.commandBuffer = cmd
			});
		}

		VkSemaphoreSubmitInfo waits[2];
		uint32_t waitCount = 0;

		waits[waitCount++] =
		{
			.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = info.ImageAvailable,
			.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
		};

		if (ComputeUsed && info.GraphicsWaitComputeValue > 0)
		{
			waits[waitCount++] =
			{
				.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.semaphore = info.ComputeTimeline,
				.value     = info.GraphicsWaitComputeValue,
				.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
			};
		}

		const VkSemaphoreSubmitInfo signals[]
		{
			{
				.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.semaphore = info.RenderFinished,
				.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
			},
			{
				.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.semaphore = info.GraphicsTimeline,
				.value     = info.GraphicsSignalValue,
				.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
			}
		};

		const VkSubmitInfo2 submitInfo
		{
			.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
			.waitSemaphoreInfoCount   = waitCount,
			.pWaitSemaphoreInfos      = waits,
			.commandBufferInfoCount   = static_cast<uint32_t>(m_SubmitInfos.size()),
			.pCommandBufferInfos      = m_SubmitInfos.data(),
			.signalSemaphoreInfoCount = 2,
			.pSignalSemaphoreInfos    = signals
		};

		VK_CHECK(vkQueueSubmit2(info.GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
	}

	std::vector<VkCommandBufferSubmitInfo> m_SubmitInfos;
};

class FrameData
{
public:
	void Initialize()
	{
		m_FrameIndex              = 0;
		m_NextGraphicsSignalValue = 1;
		m_NextComputeSignalValue  = 1;
		m_HasAsyncCompute         = Context::Get().HasDedicatedComputeQueue();

		for (auto& frame : m_Frames)
		{
			frame.GraphicsCommandPool.Create(Context::Get().GetGraphicsFamily());
			frame.GraphicsCommandBuffer = frame.GraphicsCommandPool.AllocateCommandBuffer();

			if (m_HasAsyncCompute)
			{
				frame.ComputeCommandPool.Create(Context::Get().GetComputeFamily());
				frame.ComputeCommandBuffer = frame.ComputeCommandPool.AllocateCommandBuffer();
			}
		}

		m_GraphicsTimeline.Initialize(0);

		if (m_HasAsyncCompute)
			m_ComputeTimeline.Initialize(0);
	}

	void Shutdown()
	{
		m_GraphicsTimeline.Shutdown();

		if (m_HasAsyncCompute)
			m_ComputeTimeline.Shutdown();

		for (auto& frame : m_Frames)
		{
			frame.GraphicsCommandPool.Destroy();
			frame.GraphicsCommandBuffer = {};
			frame.CommandBuffers.clear();

			if (m_HasAsyncCompute)
			{
				frame.ComputeCommandPool.Destroy();
				frame.ComputeCommandBuffer = {};
			}
		}

		m_FrameIndex              = 0;
		m_NextGraphicsSignalValue = 1;
		m_NextComputeSignalValue  = 1;
	}

	void WaitForSlot()
	{
		FrameContext& frame = Current();

		if (frame.GraphicsSignalValue > 0)
			m_GraphicsTimeline.Wait(frame.GraphicsSignalValue);

		if (frame.ComputeSignalValue > 0)
			m_ComputeTimeline.Wait(frame.ComputeSignalValue);
	}

	uint64_t NextGraphicsSignalValue() { return m_NextGraphicsSignalValue++; }
	uint64_t NextComputeSignalValue()  { return m_NextComputeSignalValue++;  }

	bool HasAsyncCompute() const { return m_HasAsyncCompute; }

	TimelineSemaphore&       GetGraphicsTimeline()       { return m_GraphicsTimeline; }
	const TimelineSemaphore& GetGraphicsTimeline() const { return m_GraphicsTimeline; }

	TimelineSemaphore&       GetComputeTimeline()        { assert(m_HasAsyncCompute); return m_ComputeTimeline; }
	const TimelineSemaphore& GetComputeTimeline()  const { assert(m_HasAsyncCompute); return m_ComputeTimeline; }

	FrameContext&       Current()       { return m_Frames[m_FrameIndex % MAX_FRAMES_IN_FLIGHT]; }
	const FrameContext& Current() const { return m_Frames[m_FrameIndex % MAX_FRAMES_IN_FLIGHT]; }

	FrameContext&       Previous()       { return m_Frames[(m_FrameIndex + MAX_FRAMES_IN_FLIGHT - 1) % MAX_FRAMES_IN_FLIGHT]; }
	const FrameContext& Previous() const { return m_Frames[(m_FrameIndex + MAX_FRAMES_IN_FLIGHT - 1) % MAX_FRAMES_IN_FLIGHT]; }

	uint64_t GetFrameIndex() const { return m_FrameIndex; }
	uint32_t GetFrameSlot()  const { return static_cast<uint32_t>(m_FrameIndex % MAX_FRAMES_IN_FLIGHT); }

	void Advance() { m_FrameIndex++; }

private:
	std::array<FrameContext, MAX_FRAMES_IN_FLIGHT> m_Frames;

	TimelineSemaphore m_GraphicsTimeline;
	TimelineSemaphore m_ComputeTimeline;

	uint64_t m_FrameIndex              = 0;
	uint64_t m_NextGraphicsSignalValue = 1;
	uint64_t m_NextComputeSignalValue  = 1;
	bool     m_HasAsyncCompute         = false;
};
