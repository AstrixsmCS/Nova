#pragma once

#include "Vulkan.hpp"

#include <array>
#include <span>

// Thin wrapper around Vulkan dynamic rendering.
// Nova does not use VkRenderPass or VkFramebuffer objects.
struct AttachmentInfo
{
	VkImageView ImageView = VK_NULL_HANDLE;

	Format   Format    = Format::Invalid;
	LoadOp   LoadOp    = LoadOp::Clear;
	StoreOp  StoreOp   = StoreOp::Store;

	VkClearValue  ClearValue = {};
	VkImageLayout Layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkResolveModeFlagBits ResolveMode        = VK_RESOLVE_MODE_NONE;
	VkImageView           ResolveImageView   = VK_NULL_HANDLE;
	VkImageLayout         ResolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
};

struct RenderPassInfo
{
	std::span<const AttachmentInfo> ColorAttachments;

	const AttachmentInfo* DepthAttachment = nullptr;

	VkRect2D         RenderArea = {};
	uint32_t         LayerCount = 1;
	VkRenderingFlags Flags      = 0;
};

namespace DynamicRendering
{
	constexpr uint32_t MaxColorAttachments = 8;

	namespace Detail
	{
		inline VkRenderingAttachmentInfo ToVkAttachment(const AttachmentInfo& attachment)
		{
			return
			{
				.sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.imageView          = attachment.ImageView,
				.imageLayout        = attachment.Layout,
				.resolveMode        = attachment.ResolveMode,
				.resolveImageView   = attachment.ResolveImageView,
				.resolveImageLayout = attachment.ResolveImageLayout,
				.loadOp             = ToVulkan(attachment.LoadOp),
				.storeOp            = ToVulkan(attachment.StoreOp),
				.clearValue         = attachment.ClearValue
			};
		}
	}

	inline void BeginRendering(CommandBuffer& commandBuffer, const RenderPassInfo& info)
	{
		assert(info.ColorAttachments.size() <= MaxColorAttachments);

		std::array<VkRenderingAttachmentInfo, MaxColorAttachments> colorAttachments;

		for (uint32_t i = 0; i < static_cast<uint32_t>(info.ColorAttachments.size()); i++)
			colorAttachments[i] = Detail::ToVkAttachment(info.ColorAttachments[i]);

		VkRenderingAttachmentInfo depthAttachment;
		if (info.DepthAttachment)
			depthAttachment = Detail::ToVkAttachment(*info.DepthAttachment);

		const VkRenderingInfo renderingInfo
		{
			.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.flags                = info.Flags,
			.renderArea           = info.RenderArea,
			.layerCount           = info.LayerCount,
			.colorAttachmentCount = static_cast<uint32_t>(info.ColorAttachments.size()),
			.pColorAttachments    = info.ColorAttachments.empty() ? nullptr : colorAttachments.data(),
			.pDepthAttachment     = info.DepthAttachment ? &depthAttachment : nullptr,
			.pStencilAttachment   = nullptr
		};

		vkCmdBeginRendering(commandBuffer.GetHandle(), &renderingInfo);

		const VkViewport viewport
		{
			.x = static_cast<float>(info.RenderArea.offset.x),
			.y = static_cast<float>(info.RenderArea.offset.y) + static_cast<float>(info.RenderArea.extent.height),
			.width = static_cast<float>(info.RenderArea.extent.width),
			.height = -static_cast<float>(info.RenderArea.extent.height),
			.minDepth = 0.0f,
			.maxDepth = 1.0f
		};

		const VkRect2D scissor = info.RenderArea;

		vkCmdSetViewportWithCount(commandBuffer.GetHandle(), 1, &viewport);
		vkCmdSetScissorWithCount(commandBuffer.GetHandle(), 1, &scissor);
	}

	inline void EndRendering(CommandBuffer& commandBuffer)
	{
		vkCmdEndRendering(commandBuffer.GetHandle());
	}
}
