#include "ImGui.hpp"

#include "Vulkan/CommandBuffer.hpp"
#include "Vulkan/Descriptors.hpp"
#include "Vulkan/Shader.hpp"
#include "Vulkan/Texture.hpp"

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>

namespace
{
	constexpr uint64_t InitialVertexBufferSize = 64 * 1024 * sizeof(ImDrawVert);
	constexpr uint64_t InitialIndexBufferSize  = 64 * 1024 * sizeof(ImDrawIdx);

	// ImGui uses a non-standard index type — keep this as a raw VkIndexType
	// since CommandBuffer::BindIndexBuffer takes IndexFormat, not VkIndexType.
	constexpr VkIndexType ImGuiIndexType = sizeof(ImDrawIdx) == 2
		? VK_INDEX_TYPE_UINT16
		: VK_INDEX_TYPE_UINT32;

	constexpr IndexFormat ImGuiIndexFormat = sizeof(ImDrawIdx) == 2
		? IndexFormat::UInt16
		: IndexFormat::UInt32;

	static_assert(sizeof(ImDrawIdx) == 2 || sizeof(ImDrawIdx) == 4);
	static_assert(offsetof(ImDrawVert, pos) == 0);
	static_assert(offsetof(ImDrawVert, uv)  == 8);
	static_assert(offsetof(ImDrawVert, col) == 16);
	static_assert(sizeof(ImDrawVert) == 20);
}

// ---- Lifecycle -------------------------------------------------------------

void ImGuiLayer::Initialize(SDL_Window* window)
{
	assert(window);
	assert(!m_Initialized);
	assert(!ImGui::GetCurrentContext());

	if (!window || m_Initialized || ImGui::GetCurrentContext())
		return;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	const bool platformInitialized = ImGui_ImplSDL3_InitForVulkan(window);
	assert(platformInitialized);

	if (!platformInitialized)
	{
		ImGui::DestroyContext();
		return;
	}

	m_Initialized = true;

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags         |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags         |= ImGuiConfigFlags_DockingEnable;
	io.BackendRendererName  = "Nova Vulkan";
	io.BackendFlags        |= ImGuiBackendFlags_RendererHasVtxOffset;

	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	style.Colors[ImGuiCol_ChildBg].w  = 1.0f;
	style.Colors[ImGuiCol_PopupBg].w  = 1.0f;

	m_Shader = std::make_shared<Shader>();
	m_Shader->Load("Assets/Shaders/ImGui.slang");

	const bool shaderValid = m_Shader->IsValid();
	assert(shaderValid);

	if (!shaderValid)
	{
		Shutdown();
		return;
	}

	m_Material.SetShader(m_Shader);

	m_State.VertexLayout =
	{
		{ ShaderDataType::Float2, "Position" },
		{ ShaderDataType::Float2, "TexCoord" },
		{ ShaderDataType::UInt,   "Color"    },
	};

	m_State.PrimitiveTopology = Topology::Triangle;
	m_State.CullMode          = CullMode::None;
	m_State.DepthTest         = false;
	m_State.DepthWrite        = false;
	m_State.Blending          = BlendMode::Alpha;

	const bool fontUploaded = UploadFontTexture();
	assert(fontUploaded);

	if (!fontUploaded)
		Shutdown();
}

void ImGuiLayer::Shutdown()
{
	if (!m_Initialized)
		return;

	if (m_FrameStarted)
	{
		ImGui::EndFrame();
		m_FrameStarted = false;
	}

	Renderer::WaitForGPU();

	for (FrameBuffers& frame : m_Frames)
	{
		frame.Vertices.Destroy();
		frame.Indices.Destroy();

		frame.VertexCapacity = 0;
		frame.IndexCapacity  = 0;
	}

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->SetTexID(static_cast<ImTextureID>(0));
	io.BackendRendererName = nullptr;
	io.BackendFlags &= ~ImGuiBackendFlags_RendererHasVtxOffset;

	if (m_FontTexture)
	{
		m_FontTexture->Destroy();
		m_FontTexture.reset();
	}

	if (m_Shader)
	{
		m_Shader->Shutdown();
		m_Shader.reset();
	}

	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();

	m_Initialized = false;
}

void ImGuiLayer::Begin()
{
	assert(m_Initialized);
	assert(!m_FrameStarted);

	if (!m_Initialized || m_FrameStarted)
		return;

	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	m_FrameStarted = true;
}

void ImGuiLayer::End(CommandBuffer& commandBuffer)
{
	assert(m_Initialized);
	assert(m_FrameStarted);

	if (!m_Initialized || !m_FrameStarted)
		return;

	ImGui::Render();
	m_FrameStarted = false;

	if (const ImDrawData* drawData = ImGui::GetDrawData())
		RenderDrawData(commandBuffer, *drawData);
}

// ---- Font upload -----------------------------------------------------------

bool ImGuiLayer::UploadFontTexture()
{
	ImGuiIO& io = ImGui::GetIO();

	unsigned char* pixels = nullptr;
	int width  = 0;
	int height = 0;

	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	if (!pixels || width <= 0 || height <= 0)
		return false;

	const TextureSpecification specification
	{
		.DebugName    = "ImGui Font Atlas",
		.Format       = Format::RGBA8_UNorm,
		.GenerateMips = false,
		.Size =
		{
			.Width  = static_cast<uint32_t>(width),
			.Height = static_cast<uint32_t>(height),
		}
	};

	m_FontTexture = std::make_shared<Texture2D>();
	m_FontTexture->Create(specification, pixels);

	if (!m_FontTexture->IsValid())
		return false;

	io.Fonts->SetTexID(static_cast<ImTextureID>(m_FontTexture->GetBindlessIndex()));

	return true;
}

// ---- Geometry upload -------------------------------------------------------

void ImGuiLayer::UploadGeometry(FrameBuffers& frame, const ImDrawData& drawData)
{
	const uint64_t vertexBytes = static_cast<uint64_t>(drawData.TotalVtxCount) * sizeof(ImDrawVert);
	const uint64_t indexBytes  = static_cast<uint64_t>(drawData.TotalIdxCount) * sizeof(ImDrawIdx);

	if (vertexBytes > frame.VertexCapacity)
	{
		const uint64_t capacity = std::max(InitialVertexBufferSize, vertexBytes * 2);
		frame.Vertices.Destroy();
		frame.Vertices.Create(capacity, VertexBufferUsage::Dynamic);
		frame.VertexCapacity = capacity;
	}

	if (indexBytes > frame.IndexCapacity)
	{
		const uint64_t capacity = std::max(InitialIndexBufferSize, indexBytes * 2);
		frame.Indices.Destroy();
		frame.Indices.Create(capacity);
		frame.IndexCapacity = capacity;
	}

	uint64_t vertexOffset = 0;
	uint64_t indexOffset  = 0;

	for (int i = 0; i < drawData.CmdListsCount; ++i)
	{
		const ImDrawList& drawList = *drawData.CmdLists[i];

		const uint64_t vertexSize = static_cast<uint64_t>(drawList.VtxBuffer.Size) * sizeof(ImDrawVert);
		const uint64_t indexSize  = static_cast<uint64_t>(drawList.IdxBuffer.Size) * sizeof(ImDrawIdx);

		if (vertexSize > 0)
			frame.Vertices.SetData(drawList.VtxBuffer.Data, vertexSize, vertexOffset);

		if (indexSize > 0)
			frame.Indices.SetData(drawList.IdxBuffer.Data, indexSize, indexOffset);

		vertexOffset += vertexSize;
		indexOffset  += indexSize;
	}
}

// ---- Render state setup ----------------------------------------------------

void ImGuiLayer::SetupRenderState(CommandBuffer& cmd, const FrameBuffers& frame,
								  const ImDrawData& drawData, VkExtent2D extent)
{
	m_Shader->Bind(cmd);
	cmd.SetGraphicsState(m_State, 1);

	// ImGui uses a downward-positive Y axis so viewport height is positive,
	// unlike the flipped viewport used elsewhere in Nova.
	const VkViewport viewport
	{
		.x        = 0.0f,
		.y        = 0.0f,
		.width    = static_cast<float>(extent.width),
		.height   = static_cast<float>(extent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};

	const VkRect2D scissor
	{
		.offset = { 0, 0 },
		.extent = extent
	};

	vkCmdSetViewportWithCount(cmd.GetHandle(), 1, &viewport);
	vkCmdSetScissorWithCount(cmd.GetHandle(), 1, &scissor);

	const VkDescriptorSet descriptorSet = Descriptor::GetSet();
	vkCmdBindDescriptorSets(cmd.GetHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS,
							m_Shader->GetPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

	cmd.BindVertexBuffer(frame.Vertices.GetBuffer());
	cmd.BindIndexBuffer(frame.Indices.GetBuffer(), ImGuiIndexFormat);

	const glm::vec2 scale
	{
		2.0f / drawData.DisplaySize.x,
		2.0f / drawData.DisplaySize.y
	};

	const glm::vec2 translate
	{
		-1.0f - drawData.DisplayPos.x * scale.x,
		-1.0f - drawData.DisplayPos.y * scale.y
	};

	m_Material.Set("Scale",     scale);
	m_Material.Set("Translate", translate);
}

// ---- Draw ------------------------------------------------------------------

void ImGuiLayer::RenderDrawData(CommandBuffer& cmd, const ImDrawData& drawData)
{
	if (drawData.TotalVtxCount <= 0 || drawData.TotalIdxCount <= 0)
		return;

	if (drawData.DisplaySize.x <= 0.0f || drawData.DisplaySize.y <= 0.0f)
		return;

	const int framebufferWidth  = static_cast<int>(drawData.DisplaySize.x * drawData.FramebufferScale.x);
	const int framebufferHeight = static_cast<int>(drawData.DisplaySize.y * drawData.FramebufferScale.y);

	if (framebufferWidth <= 0 || framebufferHeight <= 0)
		return;

	const uint32_t frameIndex = Renderer::GetFrameSlot();
	assert(frameIndex < m_Frames.size());

	if (frameIndex >= m_Frames.size())
		return;

	const auto& ranges = m_Shader->GetPushConstantRanges();
	assert(ranges.size() == 1);

	if (ranges.size() != 1)
		return;

	const auto& range = ranges[0];

	FrameBuffers& frame = m_Frames[frameIndex];
	UploadGeometry(frame, drawData);

	const VkExtent2D extent
	{
		static_cast<uint32_t>(framebufferWidth),
		static_cast<uint32_t>(framebufferHeight)
	};

	DebugLabelScope label(cmd, "ImGui", 0xff9900ff);

	SetupRenderState(cmd, frame, drawData, extent);

	uint32_t globalVertexOffset = 0;
	uint32_t globalIndexOffset  = 0;

	for (int i = 0; i < drawData.CmdListsCount; ++i)
	{
		const ImDrawList& drawList = *drawData.CmdLists[i];

		for (const ImDrawCmd& drawCommand : drawList.CmdBuffer)
		{
			if (drawCommand.UserCallback)
			{
				if (drawCommand.UserCallback == ImDrawCallback_ResetRenderState)
					SetupRenderState(cmd, frame, drawData, extent);
				else
					drawCommand.UserCallback(&drawList, &drawCommand);

				continue;
			}

			if (drawCommand.ElemCount == 0)
				continue;

			const float clipMinX = std::clamp((drawCommand.ClipRect.x - drawData.DisplayPos.x) * drawData.FramebufferScale.x, 0.0f, static_cast<float>(extent.width));
			const float clipMinY = std::clamp((drawCommand.ClipRect.y - drawData.DisplayPos.y) * drawData.FramebufferScale.y, 0.0f, static_cast<float>(extent.height));
			const float clipMaxX = std::clamp((drawCommand.ClipRect.z - drawData.DisplayPos.x) * drawData.FramebufferScale.x, 0.0f, static_cast<float>(extent.width));
			const float clipMaxY = std::clamp((drawCommand.ClipRect.w - drawData.DisplayPos.y) * drawData.FramebufferScale.y, 0.0f, static_cast<float>(extent.height));

			if (clipMaxX <= clipMinX || clipMaxY <= clipMinY)
				continue;

			const VkRect2D scissor
			{
				.offset = { static_cast<int32_t>(clipMinX), static_cast<int32_t>(clipMinY) },
				.extent = { static_cast<uint32_t>(clipMaxX - clipMinX), static_cast<uint32_t>(clipMaxY - clipMinY) }
			};

			if (scissor.extent.width == 0 || scissor.extent.height == 0)
				continue;

			vkCmdSetScissorWithCount(cmd.GetHandle(), 1, &scissor);

			const uint32_t textureIndex = static_cast<uint32_t>(drawCommand.GetTexID());
			m_Material.Set("Texture", textureIndex);

			const auto& storage = m_Material.GetUniformStorage();
			assert(storage.size() == range.Size);

			if (storage.size() != range.Size)
				continue;

			cmd.PushConstants(m_Shader->GetPipelineLayout(), range.StageFlags, storage.data(), static_cast<uint32_t>(storage.size()), range.Offset);

			cmd.DrawIndexed(drawCommand.ElemCount, 1, drawCommand.IdxOffset  + globalIndexOffset, static_cast<int32_t>(drawCommand.VtxOffset + globalVertexOffset));
		}

		globalVertexOffset += static_cast<uint32_t>(drawList.VtxBuffer.Size);
		globalIndexOffset  += static_cast<uint32_t>(drawList.IdxBuffer.Size);
	}

	// Restore full scissor after per-command clipping.
	const VkRect2D fullScissor{ .offset = { 0, 0 }, .extent = extent };
	vkCmdSetScissorWithCount(cmd.GetHandle(), 1, &fullScissor);
}
