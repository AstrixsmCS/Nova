#pragma once

#include "Vulkan.hpp"
#include "VulkanUtils.hpp"

#include "CommandBuffer.hpp"
#include "Renderer/RendererTypes.hpp"

#include <array>
#include <span>

// Thin wrapper around Vulkan dynamic rendering.
// Nova does not use VkRenderPass or VkFramebuffer objects.
struct AttachmentInfo
{
	VkImageView ImageView = VK_NULL_HANDLE;

	LoadOp LoadOp = LoadOp::Clear;
	StoreOp StoreOp = StoreOp::Store;

	VkClearValue ClearValue = {};

	VkImageLayout Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	ResolveMode ResolveMode = ResolveMode::None;
	VkImageView ResolveImageView = VK_NULL_HANDLE;
	VkImageLayout ResolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
};

struct RenderPassInfo
{
	std::span<const AttachmentInfo> ColorAttachments;

	const AttachmentInfo* DepthAttachment = nullptr;

	ScissorRect RenderArea = {};

	uint32_t LayerCount = 1;
	VkRenderingFlags Flags = 0;
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
				.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.imageView = attachment.ImageView,
				.imageLayout = attachment.Layout,
				.resolveMode = ToVulkan(attachment.ResolveMode),
				.resolveImageView = attachment.ResolveImageView,
				.resolveImageLayout = attachment.ResolveImageLayout,
				.loadOp = ToVulkan(attachment.LoadOp),
				.storeOp = ToVulkan(attachment.StoreOp),
				.clearValue = attachment.ClearValue
			};
		}
	}

	inline void BeginRendering(CommandBuffer& commandBuffer, const RenderPassInfo& info)
	{
		assert(info.ColorAttachments.size() <= MaxColorAttachments);

		std::array<VkRenderingAttachmentInfo, MaxColorAttachments> colorAttachments{};

		for (uint32_t i = 0; i < static_cast<uint32_t>(info.ColorAttachments.size()); ++i)
			colorAttachments[i] = Detail::ToVkAttachment(info.ColorAttachments[i]);

		VkRenderingAttachmentInfo depthAttachment{};

		if (info.DepthAttachment)
			depthAttachment = Detail::ToVkAttachment(*info.DepthAttachment);

		const VkRect2D renderArea = ToVulkan(info.RenderArea);

		const VkRenderingInfo renderingInfo
		{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.flags = info.Flags,
			.renderArea = renderArea,
			.layerCount = info.LayerCount,
			.colorAttachmentCount = static_cast<uint32_t>(info.ColorAttachments.size()),
			.pColorAttachments = info.ColorAttachments.empty() ? nullptr : colorAttachments.data(),
			.pDepthAttachment = info.DepthAttachment ? &depthAttachment : nullptr,
			.pStencilAttachment = nullptr
		};

		vkCmdBeginRendering(commandBuffer.GetHandle(), &renderingInfo);

		const Viewport viewport
		{
			.X = static_cast<float>(info.RenderArea.X),
			.Y = static_cast<float>(info.RenderArea.Y + info.RenderArea.Height),
			.Width = static_cast<float>(info.RenderArea.Width),
			.Height = -static_cast<float>(info.RenderArea.Height),
			.MinDepth = 0.0f,
			.MaxDepth = 1.0f
		};

		const VkViewport vkViewport = ToVulkan(viewport);
		const VkRect2D vkScissor = ToVulkan(info.RenderArea);

		vkCmdSetViewportWithCount(commandBuffer.GetHandle(), 1, &vkViewport);
		vkCmdSetScissorWithCount(commandBuffer.GetHandle(), 1, &vkScissor);
	}

	inline void EndRendering(CommandBuffer& commandBuffer)
	{
		vkCmdEndRendering(commandBuffer.GetHandle());
	}
}
