#include "Renderer.hpp"

#include "Vulkan/Context.hpp"

void Renderer::Initialize(SDL_Window* windowHandle)
{
	Context::Initialize();

	s_SwapChain = std::make_unique<SwapChain>(windowHandle);
	s_SwapChain->Initialize();

	s_FrameData.Initialize();
	CreateSyncObjects();
}

void Renderer::Shutdown()
{
	WaitForGPU();

	DestroySyncObjects();
	s_FrameData.Shutdown();

	s_SwapChain.reset();

	Context::Shutdown();
}

void Renderer::WaitForGPU()
{
	vkDeviceWaitIdle(Context::Get().GetDevice());
}

bool Renderer::BeginFrame()
{
	const uint32_t frameIndex = GetCurrentFrameIndex();
	const uint64_t waitValue  = s_FrameSignalValues[frameIndex];

	if (waitValue != 0)
		s_FrameTimeline.Wait(waitValue);

	GetCurrentFrame().Reset();

	s_CurrentImageIndex = s_SwapChain->AcquireNextImage(GetImageAvailableSemaphore());

	if (s_CurrentImageIndex == UINT32_MAX)
		return false;

	GetCurrentFrame().GraphicsCommandBuffer.Begin();

	return true;
}

void Renderer::EndFrame()
{
	FrameContext& frame = GetCurrentFrame();

	frame.GraphicsCommandBuffer.End();

	const uint64_t signalValue = s_NextSignalValue++;

	const VkSemaphoreSubmitInfo imageAvailableWait
	{
		.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.semaphore = GetImageAvailableSemaphore(),
		.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
	};

	const VkSemaphoreSubmitInfo signalInfos[]
	{
		{
			.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = GetRenderFinishedSemaphore(s_CurrentImageIndex),
			.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
		},
		{
			.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = s_FrameTimeline.GetHandle(),
			.value     = signalValue,
			.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
		}
	};

	const VkCommandBufferSubmitInfo commandBufferInfo
	{
		.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
		.commandBuffer = frame.GraphicsCommandBuffer.GetHandle()
	};

	const VkSubmitInfo2 submitInfo
	{
		.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.waitSemaphoreInfoCount   = 1,
		.pWaitSemaphoreInfos      = &imageAvailableWait,
		.commandBufferInfoCount   = 1,
		.pCommandBufferInfos      = &commandBufferInfo,
		.signalSemaphoreInfoCount = 2,
		.pSignalSemaphoreInfos    = signalInfos
	};

	VK_CHECK(vkQueueSubmit2(Context::Get().GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE));

	s_FrameSignalValues[GetCurrentFrameIndex()] = signalValue;
}

void Renderer::Present()
{
	s_SwapChain->Present(GetRenderFinishedSemaphore(s_CurrentImageIndex));
	s_FrameData.Advance();
}

void Renderer::CreateSyncObjects()
{
	VkDevice device = Context::Get().GetDevice();

	const uint32_t imageCount = s_SwapChain->GetImageCount();

	s_FrameTimeline.Initialize(0);
	s_FrameSignalValues.fill(0);

	VkSemaphoreCreateInfo semaphoreInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

	s_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	s_RenderFinishedSemaphores.resize(imageCount);

	for (VkSemaphore& semaphore : s_ImageAvailableSemaphores)
		VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore));

	for (VkSemaphore& semaphore : s_RenderFinishedSemaphores)
		VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore));
}

void Renderer::DestroySyncObjects()
{
	VkDevice device = Context::Get().GetDevice();

	s_FrameTimeline.Shutdown();

	for (VkSemaphore semaphore : s_ImageAvailableSemaphores)
		vkDestroySemaphore(device, semaphore, nullptr);

	for (VkSemaphore semaphore : s_RenderFinishedSemaphores)
		vkDestroySemaphore(device, semaphore, nullptr);

	s_ImageAvailableSemaphores.clear();
	s_RenderFinishedSemaphores.clear();
}
