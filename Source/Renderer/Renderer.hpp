#pragma once

#include "Vulkan.hpp"
#include "SwapChain.hpp"
#include "TimelineSemaphore.hpp"
#include "FrameData.hpp"

#include <array>
#include <vector>

struct SDL_Window;

class Renderer
{
public:
	static void Initialize(SDL_Window* windowHandle);
	static void Shutdown();

	static void WaitForGPU();

	static SwapChain&     GetSwapChain()           { return *s_SwapChain; }
	static FrameContext&  GetCurrentFrame()         { return s_FrameData.Current(); }
	static CommandBuffer& GetCurrentCommandBuffer() { return GetCurrentFrame().GraphicsCommandBuffer; }

	static VkSemaphore GetImageAvailableSemaphore()              { return s_ImageAvailableSemaphores[s_FrameData.GetFrameIndex() % MAX_FRAMES_IN_FLIGHT]; }
	static VkSemaphore GetRenderFinishedSemaphore(uint32_t index){ return s_RenderFinishedSemaphores[index]; }

	static uint32_t GetCurrentFrameIndex() { return static_cast<uint32_t>(s_FrameData.GetFrameIndex() % MAX_FRAMES_IN_FLIGHT); }
	static uint32_t GetCurrentImageIndex() { return s_CurrentImageIndex; }

	static constexpr uint32_t GetFramesInFlight() { return MAX_FRAMES_IN_FLIGHT; }

	// ==== ~Actual~ Renderer here... TODO: remove confusion later ====

	static bool BeginFrame();
	static void EndFrame();
	static void Present();

	static void ClearColor(float red, float green, float blue, float alpha = 1.0f);
private:
	static void CreateSyncObjects();
	static void DestroySyncObjects();

	inline static std::unique_ptr<SwapChain>           s_SwapChain;
	inline static FrameData                            s_FrameData;
	inline static std::vector<VkSemaphore>             s_ImageAvailableSemaphores;
	inline static std::vector<VkSemaphore>             s_RenderFinishedSemaphores;
	inline static TimelineSemaphore                    s_FrameTimeline;
	inline static uint64_t                             s_NextSignalValue = 1;
	inline static std::array<uint64_t, MAX_FRAMES_IN_FLIGHT> s_FrameSignalValues{};
	inline static uint32_t                             s_CurrentImageIndex = UINT32_MAX;
};
