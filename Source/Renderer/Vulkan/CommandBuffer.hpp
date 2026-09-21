#pragma once

#include "Vulkan.hpp"

#include "Buffer.hpp"

#include <string>
#include <array>
#include <span>

struct ClearDepthStencil
{
	float    Depth   = 1.0f;
	uint32_t Stencil = 0;
};

union ClearValue
{
	ClearColorValue   Color = { .Float32 = { 0.0f, 0.0f, 0.0f, 1.0f } };
	ClearDepthStencil DepthStencil;
};

struct RenderingAttachmentInfo
{
	VkImageView  ImageView  = VK_NULL_HANDLE;
	LoadOp       LoadOp     = LoadOp::Clear;
	StoreOp      StoreOp    = StoreOp::Store;
	ClearValue   ClearValue = {};
	VkImageLayout Layout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	ResolveMode   ResolveMode        = ResolveMode::None;
	VkImageView   ResolveImageView   = VK_NULL_HANDLE;
	VkImageLayout ResolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
};

struct RenderingInfo
{
	std::span<const RenderingAttachmentInfo> ColorAttachments;
	const RenderingAttachmentInfo*           DepthAttachment = nullptr;
	ScissorRect                     RenderArea      = {};
	uint32_t                        LayerCount      = 1;
	VkRenderingFlags                Flags           = 0;
};

struct GraphicsState
{
	VertexBufferLayout VertexLayout;

	Topology    PrimitiveTopology = Topology::Triangle;
	CompareOp   DepthCompare      = CompareOp::Less;
	BlendMode   Blending          = BlendMode::None;
	CullMode    CullMode          = CullMode::Back;
	WindingMode FrontFace         = WindingMode::CCW;
	PolygonMode PolygonMode       = PolygonMode::Fill;

	bool  DepthTest  = true;
	bool  DepthWrite = true;
	float LineWidth  = 1.0f;
};

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

class DebugLabelScope
{
public:
	explicit DebugLabelScope(const CommandBuffer& commandBuffer, const char* label, uint32_t colorRGBA = 0xffffffff);
	~DebugLabelScope();

	DebugLabelScope(const DebugLabelScope&)            = delete;
	DebugLabelScope& operator=(const DebugLabelScope&) = delete;

private:
	const CommandBuffer& m_CommandBuffer;
};

class CommandBuffer
{
public:
	void Begin(bool oneTimeSubmit = false);
	void End();

	void Flush();
	void Flush(VkQueue queue);

	// ==== Recording commands ====
	void BeginRendering(const RenderingInfo& info);
	void EndRendering();
	void SetGraphicsState(const GraphicsState& state, uint32_t colorAttachmentCount);

	void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex   = 0, uint32_t baseInstance  = 0);
	void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex    = 0, int32_t  vertexOffset  = 0, uint32_t baseInstance  = 0);

	void Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ = 1);

	void BindVertexBuffer(VkBuffer buffer, VkDeviceSize offset = 0, uint32_t binding = 0);
	void BindIndexBuffer(VkBuffer buffer, IndexFormat format, VkDeviceSize offset = 0);

	void PushConstants(VkPipelineLayout layout, VkShaderStageFlags stages, const void* data, uint32_t size, uint32_t offset = 0);
	template<typename T>
	void PushConstants(VkPipelineLayout layout, VkShaderStageFlags stages, const T& data, uint32_t offset = 0)
	{
		PushConstants(layout, stages, &data, static_cast<uint32_t>(sizeof(T)), offset);
	}

	void ClearColorImage(VkImage image, VkImageLayout layout, const ClearColorValue& color, VkImageSubresourceRange  range);
	void CopyImage(VkImage src, VkImageLayout srcLayout, VkImage dst, VkImageLayout dstLayout, const VkImageCopy2& region);
	void BlitImage(VkImage src, VkImageLayout srcLayout, VkImage dst, VkImageLayout dstLayout, const VkImageBlit2&  region, VkFilter filter = VK_FILTER_LINEAR);

	void PushDebugLabel (const char* label, uint32_t colorRGBA = 0xffffffff) const;
	void PopDebugLabel() const;
	void InsertDebugLabel(const char* label, uint32_t colorRGBA = 0xffffffff) const;

	void PipelineBarrier(const VkDependencyInfo& dep);
	void ImageBarrier(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageSubresourceRange range, VkPipelineStageFlags2 srcStage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VkPipelineStageFlags2 dstStage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
	inline void ImageBarrier(VkImage image, VkImageAspectFlags aspectMask, VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags2 srcStage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VkPipelineStageFlags2 dstStage = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT)
	{
		ImageBarrier(image, oldLayout, newLayout,
			VkImageSubresourceRange
			{
				.aspectMask     = aspectMask,
				.baseMipLevel   = 0,
				.levelCount     = VK_REMAINING_MIP_LEVELS,
				.baseArrayLayer = 0,
				.layerCount     = VK_REMAINING_ARRAY_LAYERS
			},
			srcStage, dstStage);
	}

	// ==== Raw handle ====

	VkCommandBuffer GetHandle() const { return m_Handle; }
	bool IsValid() const { return m_Handle != VK_NULL_HANDLE; }

private:
	friend class CommandPool;

	VkCommandBuffer m_Handle = VK_NULL_HANDLE;
};
