#include "Renderer.hpp"
#include "Vulkan/Context.hpp"

#include "Vulkan/Descriptors.hpp"

void Renderer::Initialize(SDL_Window* windowHandle)
{
	Context::Initialize();

	Descriptor::Initialize();

	s_SwapChain = std::make_unique<SwapChain>(windowHandle);
	s_SwapChain->Initialize();

	s_FrameData.Initialize();
}

void Renderer::Shutdown()
{
	WaitForGPU();

	s_FrameData.Shutdown();
	s_SwapChain.reset();

	Descriptor::Shutdown();

	Context::Shutdown();
}

void Renderer::WaitForGPU()
{
	vkDeviceWaitIdle(Context::Get().GetDevice());
}

CommandBuffer& Renderer::AcquireCommandBuffer(bool dedicatedCompute)
{
	assert(!dedicatedCompute || s_FrameData.HasAsyncCompute());

	CommandBuffer& commandBuffer = GetCurrentFrame().AcquireCommandBuffer(dedicatedCompute);
	commandBuffer.Begin();

	return commandBuffer;
}

bool Renderer::BeginFrame()
{
	const uint32_t frameSlot = s_FrameData.GetFrameSlot();

	s_FrameData.WaitForSlot();

	s_CurrentImageIndex = s_SwapChain->AcquireNextImage(frameSlot);

	if (s_CurrentImageIndex == UINT32_MAX)
		return false;

	GetCurrentFrame().Reset(s_FrameData.HasAsyncCompute());

	return true;
}

void Renderer::EndFrame()
{
	const uint32_t frameSlot       = s_FrameData.GetFrameSlot();
	const bool     hasAsyncCompute = s_FrameData.HasAsyncCompute();
	FrameContext&  frame           = GetCurrentFrame();

	const uint64_t graphicsSignalValue = s_FrameData.NextGraphicsSignalValue();
	const uint64_t computeSignalValue  = (hasAsyncCompute && frame.HasComputeWork()) ? s_FrameData.NextComputeSignalValue() : 0;

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
		.GraphicsWaitComputeValue = hasAsyncCompute && frame.HasComputeWork() ? computeSignalValue : 0
	});
}

void Renderer::Present()
{
	s_SwapChain->Present(s_FrameData.GetFrameSlot());
	s_FrameData.Advance();
}
