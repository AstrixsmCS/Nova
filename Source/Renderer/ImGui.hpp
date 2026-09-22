#pragma once

#include "Vulkan/Buffer.hpp"
#include "Material.hpp"
#include "Renderer.hpp"

#include <array>
#include <cstdint>
#include <memory>

struct SDL_Window;
struct ImDrawData;

class CommandBuffer;
class Shader;
class Texture2D;

class ImGuiLayer
{
public:
	ImGuiLayer() = default;

	ImGuiLayer(const ImGuiLayer&)            = delete;
	ImGuiLayer& operator=(const ImGuiLayer&) = delete;

	void Initialize(SDL_Window* window);
	void Shutdown();

	void Begin();
	void End(CommandBuffer& commandBuffer);

private:
	struct FrameBuffers
	{
		Buffer Vertices;
		Buffer  Indices;

		uint64_t VertexCapacity = 0;
		uint64_t IndexCapacity  = 0;
	};

	bool UploadFontTexture();

	void UploadGeometry(FrameBuffers& frame, const ImDrawData& drawData);
	void SetupRenderState(CommandBuffer& commandBuffer, const FrameBuffers& frame, const ImDrawData& drawData, VkExtent2D extent);
	void RenderDrawData(CommandBuffer& commandBuffer, const ImDrawData& drawData);

private:
	static constexpr uint32_t MaxFramesInFlight = Renderer::GetFramesInFlight();

	std::shared_ptr<Shader>    m_Shader;
	std::shared_ptr<Texture> m_FontTexture;

	GraphicsState m_State;
	Material      m_Material;

	std::array<FrameBuffers, MaxFramesInFlight> m_Frames;

	bool m_Initialized  = false;
	bool m_FrameStarted = false;
};
