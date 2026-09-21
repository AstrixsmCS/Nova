#include "Renderer.hpp"

#include "Vulkan/Context.hpp"

void Renderer::Initialize(SDL_Window* windowHandle)
{
	Context::Initialize();

	s_SwapChain = std::make_unique<SwapChain>(windowHandle);
	s_SwapChain->Initialize();

	s_FrameData.Initialize();
}

void Renderer::Shutdown()
{
	WaitForGPU();

	s_FrameData.Shutdown();
	s_SwapChain.reset();

	Context::Shutdown();
}

void Renderer::WaitForGPU()
{
	vkDeviceWaitIdle(Context::Get().GetDevice());
}

CommandBuffer& Renderer::GetComputeCommandBuffer()
{
	assert(s_FrameData.HasAsyncCompute());

	FrameContext& frame = GetCurrentFrame();

	if (!frame.ComputeUsed)
	{
		frame.ComputeCommandBuffer.Begin();
		frame.ComputeUsed = true;
	}

	return frame.ComputeCommandBuffer;
}

bool Renderer::BeginFrame()
{
	const uint32_t frameSlot = s_FrameData.GetFrameSlot();

	s_FrameData.WaitForSlot();

	s_CurrentImageIndex = s_SwapChain->AcquireNextImage(frameSlot);

	if (s_CurrentImageIndex == UINT32_MAX)
		return false;

	GetCurrentFrame().Reset(s_FrameData.HasAsyncCompute());
	GetCurrentFrame().GraphicsCommandBuffer.Begin();

	return true;
}

void Renderer::EndFrame()
{
	const uint32_t frameSlot      = s_FrameData.GetFrameSlot();
	const bool     hasAsyncCompute = s_FrameData.HasAsyncCompute();
	FrameContext&  frame           = GetCurrentFrame();

	const uint64_t graphicsSignalValue = s_FrameData.NextGraphicsSignalValue();
	const uint64_t computeSignalValue  = (hasAsyncCompute && frame.ComputeUsed) ? s_FrameData.NextComputeSignalValue() : 0;

	frame.Submit(
	{
		.GraphicsQueue       = Context::Get().GetGraphicsQueue(),
		.ComputeQueue        = hasAsyncCompute ? Context::Get().GetComputeQueue() : VK_NULL_HANDLE,

		.ImageAvailable      = s_SwapChain->GetImageAvailableSemaphore(frameSlot),
		.RenderFinished      = s_SwapChain->GetRenderFinishedSemaphore(s_CurrentImageIndex),

		.GraphicsTimeline    = s_FrameData.GetGraphicsTimeline().GetHandle(),
		.ComputeTimeline     = hasAsyncCompute ? s_FrameData.GetComputeTimeline().GetHandle() : VK_NULL_HANDLE,

		.GraphicsSignalValue = graphicsSignalValue,
		.ComputeSignalValue  = computeSignalValue,

		// Cross-queue dependencies are intentionally 0.
		// The render graph will set these when resources are shared between queues.
		.ComputeWaitGraphicsValue = 0,
		.GraphicsWaitComputeValue = 0
	});
}

void Renderer::Present()
{
	s_SwapChain->Present(s_FrameData.GetFrameSlot());
	s_FrameData.Advance();
}
