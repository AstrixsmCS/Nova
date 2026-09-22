#include "CommandBuffer.hpp"

#include "Context.hpp"
#include "VulkanUtils.hpp"

#include <cassert>

CommandPool::~CommandPool()
{
	Destroy();
}

void CommandPool::Create(uint32_t queueFamilyIndex, uint32_t commandBufferCount)
{
	assert(m_Handle == VK_NULL_HANDLE);
	assert(commandBufferCount > 0);

	VkDevice device = Context::Get().GetDevice();

	const VkCommandPoolCreateInfo poolInfo
	{
		.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
		.queueFamilyIndex = queueFamilyIndex
	};

	VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &m_Handle));

	std::vector<VkCommandBuffer> handles(commandBufferCount);

	const VkCommandBufferAllocateInfo allocInfo
	{
		.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool        = m_Handle,
		.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = commandBufferCount
	};

	VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, handles.data()));

	m_CommandBuffers.resize(commandBufferCount);

	for (uint32_t i = 0; i < commandBufferCount; ++i)
		m_CommandBuffers[i].m_Handle = handles[i];

	m_NextCommandBuffer = 0;
}

void CommandPool::Destroy()
{
	if (m_Handle == VK_NULL_HANDLE)
		return;

	vkDestroyCommandPool(Context::Get().GetDevice(), m_Handle, nullptr);
	m_Handle = VK_NULL_HANDLE;

	for (CommandBuffer& commandBuffer : m_CommandBuffers)
		commandBuffer.m_Handle = VK_NULL_HANDLE;

	m_CommandBuffers.clear();
	m_NextCommandBuffer = 0;
}

void CommandPool::Reset()
{
	assert(m_Handle != VK_NULL_HANDLE);

	VK_CHECK(vkResetCommandPool(Context::Get().GetDevice(), m_Handle, 0));

	m_NextCommandBuffer = 0;
}

CommandBuffer& CommandPool::Acquire()
{
	assert(m_Handle != VK_NULL_HANDLE);
	assert(m_NextCommandBuffer < m_CommandBuffers.size() && "CommandPool exhausted increase MAX_COMMAND_BUFFERS.");

	return m_CommandBuffers[m_NextCommandBuffer++];
}

DebugLabelScope::DebugLabelScope(const CommandBuffer& commandBuffer, const char* label, uint32_t colorRGBA)
	: m_CommandBuffer(commandBuffer)
{
	m_CommandBuffer.PushDebugLabel(label, colorRGBA);
}

DebugLabelScope::~DebugLabelScope()
{
	m_CommandBuffer.PopDebugLabel();
}

void CommandBuffer::Begin(bool oneTimeSubmit)
{
	assert(m_Handle != VK_NULL_HANDLE);

	VkCommandBufferUsageFlags flags = 0;

	if (oneTimeSubmit)
		flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VkCommandBufferBeginInfo beginInfo
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = flags
	};

	VK_CHECK(vkBeginCommandBuffer(m_Handle, &beginInfo));
}

void CommandBuffer::End()
{
	assert(m_Handle != VK_NULL_HANDLE);

	VK_CHECK(vkEndCommandBuffer(m_Handle));
}

// ==== Recording commands ====

void CommandBuffer::BeginRendering(const RenderingInfo& info)
{
	assert(m_Handle != VK_NULL_HANDLE);
	assert(info.ColorAttachments.size() <= 8);

	std::array<VkRenderingAttachmentInfo, 8> colorAttachments{};

	for (uint32_t i = 0; i < static_cast<uint32_t>(info.ColorAttachments.size()); ++i)
	{
		const RenderingAttachmentInfo& a = info.ColorAttachments[i];

		colorAttachments[i] =
		{
			.sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView          = a.ImageView,
			.imageLayout        = a.Layout,
			.resolveMode        = ToVulkan(a.ResolveMode),
			.resolveImageView   = a.ResolveImageView,
			.resolveImageLayout = a.ResolveImageLayout,
			.loadOp             = ToVulkan(a.LoadOp),
			.storeOp            = ToVulkan(a.StoreOp),
			.clearValue         = { .color = { .float32 = {
				a.ClearValue.Color.Float32[0], a.ClearValue.Color.Float32[1],
				a.ClearValue.Color.Float32[2], a.ClearValue.Color.Float32[3]
			}}}
		};
	}

	VkRenderingAttachmentInfo depthAttachment{};

	if (info.DepthAttachment)
	{
		const RenderingAttachmentInfo& a = *info.DepthAttachment;

		depthAttachment =
		{
			.sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView          = a.ImageView,
			.imageLayout        = a.Layout,
			.resolveMode        = ToVulkan(a.ResolveMode),
			.resolveImageView   = a.ResolveImageView,
			.resolveImageLayout = a.ResolveImageLayout,
			.loadOp             = ToVulkan(a.LoadOp),
			.storeOp            = ToVulkan(a.StoreOp),
			.clearValue         = { .depthStencil = {
				a.ClearValue.DepthStencil.Depth,
				a.ClearValue.DepthStencil.Stencil
			}}
		};
	}

	const VkRect2D renderArea = ToVulkan(info.RenderArea);

	const VkRenderingInfo renderingInfo
	{
		.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.flags                = info.Flags,
		.renderArea           = renderArea,
		.layerCount           = info.LayerCount,
		.colorAttachmentCount = static_cast<uint32_t>(info.ColorAttachments.size()),
		.pColorAttachments    = info.ColorAttachments.empty() ? nullptr : colorAttachments.data(),
		.pDepthAttachment     = info.DepthAttachment ? &depthAttachment : nullptr,
		.pStencilAttachment   = nullptr
	};

	vkCmdBeginRendering(m_Handle, &renderingInfo);

	// Negative height flips Y to match the Vulkan convention.
	const VkViewport viewport
	{
		.x        = static_cast<float>(info.RenderArea.X),
		.y        = static_cast<float>(info.RenderArea.Y + info.RenderArea.Height),
		.width    = static_cast<float>(info.RenderArea.Width),
		.height   = -static_cast<float>(info.RenderArea.Height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};

	vkCmdSetViewportWithCount(m_Handle, 1, &viewport);
	vkCmdSetScissorWithCount(m_Handle, 1, &renderArea);
}

void CommandBuffer::EndRendering()
{
	assert(m_Handle != VK_NULL_HANDLE);
	vkCmdEndRendering(m_Handle);
}

void CommandBuffer::SetGraphicsState(const GraphicsState& state, uint32_t colorAttachmentCount)
{
	assert(m_Handle != VK_NULL_HANDLE);

	// Vertex input
	{
		std::vector<VkVertexInputBindingDescription2EXT>   bindings;
		std::vector<VkVertexInputAttributeDescription2EXT> attributes;

		if (state.VertexLayout.GetElementCount() > 0)
		{
			bindings.push_back(
			{
				.sType     = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT,
				.binding   = 0,
				.stride    = state.VertexLayout.GetStride(),
				.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
				.divisor   = 1
			});

			uint32_t location = 0;

			for (const auto& element : state.VertexLayout)
			{
				const VkFormat fmt = ToVulkan(element.Type);
				assert(fmt != VK_FORMAT_UNDEFINED);

				attributes.push_back(
				{
					.sType    = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT,
					.location = location++,
					.binding  = 0,
					.format   = fmt,
					.offset   = element.Offset
				});
			}
		}

		vkCmdSetVertexInputEXT(
			m_Handle,
			static_cast<uint32_t>(bindings.size()),    bindings.empty()   ? nullptr : bindings.data(),
			static_cast<uint32_t>(attributes.size()),  attributes.empty() ? nullptr : attributes.data());
	}

	// Input assembly
	vkCmdSetPrimitiveTopology(m_Handle, ToVulkan(state.PrimitiveTopology));
	vkCmdSetPrimitiveRestartEnable(m_Handle, VK_FALSE);

	// Rasterization
	vkCmdSetRasterizerDiscardEnable(m_Handle, VK_FALSE);
	vkCmdSetPolygonModeEXT(m_Handle, ToVulkan(state.PolygonMode));
	vkCmdSetCullMode(m_Handle, ToVulkan(state.CullMode));
	vkCmdSetFrontFace(m_Handle, ToVulkan(state.FrontFace));
	vkCmdSetDepthBiasEnable(m_Handle, VK_FALSE);
	vkCmdSetDepthClampEnableEXT(m_Handle, VK_FALSE);
	vkCmdSetLineWidth(m_Handle, state.LineWidth);

	// Multisampling
	vkCmdSetRasterizationSamplesEXT(m_Handle, VK_SAMPLE_COUNT_1_BIT);
	const VkSampleMask sampleMask = ~VkSampleMask{ 0 };
	vkCmdSetSampleMaskEXT(m_Handle, VK_SAMPLE_COUNT_1_BIT, &sampleMask);
	vkCmdSetAlphaToCoverageEnableEXT(m_Handle, VK_FALSE);
	vkCmdSetAlphaToOneEnableEXT(m_Handle, VK_FALSE);

	// Depth / stencil
	vkCmdSetDepthTestEnable(m_Handle,  state.DepthTest  ? VK_TRUE : VK_FALSE);
	vkCmdSetDepthWriteEnable(m_Handle, state.DepthWrite ? VK_TRUE : VK_FALSE);
	vkCmdSetDepthCompareOp(m_Handle, ToVulkan(state.DepthCompare));
	vkCmdSetDepthBoundsTestEnable(m_Handle, VK_FALSE);
	vkCmdSetStencilTestEnable(m_Handle, VK_FALSE);

	// Color blending
	if (colorAttachmentCount > 0)
	{
		vkCmdSetLogicOpEnableEXT(m_Handle, VK_FALSE);

		constexpr uint32_t kMax = 8;
		assert(colorAttachmentCount <= kMax);

		const bool blendEnabled = state.Blending != BlendMode::None;

		const VkColorBlendEquationEXT blendEq = [&]() -> VkColorBlendEquationEXT
		{
			switch (state.Blending)
			{
				case BlendMode::Alpha:
					return
					{
						.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
						.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
						.colorBlendOp        = VK_BLEND_OP_ADD,
						.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
						.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
						.alphaBlendOp        = VK_BLEND_OP_ADD
					};

				case BlendMode::PremultipliedAlpha:
					return
					{
						.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
						.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
						.colorBlendOp        = VK_BLEND_OP_ADD,
						.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
						.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
						.alphaBlendOp        = VK_BLEND_OP_ADD
					};

				case BlendMode::Additive:
					return
					{
						.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
						.dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
						.colorBlendOp        = VK_BLEND_OP_ADD,
						.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
						.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
						.alphaBlendOp        = VK_BLEND_OP_ADD
					};

				case BlendMode::Multiply:
					return
					{
						.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR,
						.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
						.colorBlendOp        = VK_BLEND_OP_ADD,
						.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
						.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
						.alphaBlendOp        = VK_BLEND_OP_ADD
					};

				default: // BlendMode::None
					return
					{
						.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
						.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
						.colorBlendOp        = VK_BLEND_OP_ADD,
						.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
						.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
						.alphaBlendOp        = VK_BLEND_OP_ADD
					};
			}
		}();

		constexpr VkColorComponentFlags kWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		std::array<VkBool32,                kMax> blendEnables;
		std::array<VkColorBlendEquationEXT, kMax> blendEquations;
		std::array<VkColorComponentFlags,   kMax> writeMasks;

		for (uint32_t i = 0; i < colorAttachmentCount; ++i)
		{
			blendEnables[i]   = blendEnabled ? VK_TRUE : VK_FALSE;
			blendEquations[i] = blendEq;
			writeMasks[i]     = kWriteMask;
		}

		vkCmdSetColorBlendEnableEXT(m_Handle, 0, colorAttachmentCount, blendEnables.data());
		vkCmdSetColorBlendEquationEXT(m_Handle, 0, colorAttachmentCount, blendEquations.data());
		vkCmdSetColorWriteMaskEXT(m_Handle, 0, colorAttachmentCount, writeMasks.data());
	}
}

void CommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t baseInstance)
{
	assert(m_Handle != VK_NULL_HANDLE);
	if (vertexCount == 0)
		return;
	vkCmdDraw(m_Handle, vertexCount, instanceCount, firstVertex, baseInstance);
}

void CommandBuffer::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t baseInstance)
{
	assert(m_Handle != VK_NULL_HANDLE);
	if (indexCount == 0)
		return;
	vkCmdDrawIndexed(m_Handle, indexCount, instanceCount, firstIndex, vertexOffset, baseInstance);
}

void CommandBuffer::Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
{
	assert(m_Handle != VK_NULL_HANDLE);
	vkCmdDispatch(m_Handle, groupsX, groupsY, groupsZ);
}

void CommandBuffer::BindVertexBuffer(VkBuffer buffer, VkDeviceSize offset, uint32_t binding)
{
	assert(m_Handle != VK_NULL_HANDLE);
	assert(buffer != VK_NULL_HANDLE);
	vkCmdBindVertexBuffers(m_Handle, binding, 1, &buffer, &offset);
}

void CommandBuffer::BindIndexBuffer(VkBuffer buffer, IndexFormat format, VkDeviceSize offset)
{
	assert(m_Handle != VK_NULL_HANDLE);
	assert(buffer != VK_NULL_HANDLE);
	vkCmdBindIndexBuffer(m_Handle, buffer, offset, ToVulkan(format));
}

void CommandBuffer::PushConstants(VkPipelineLayout layout, VkShaderStageFlags stages, const void* data, uint32_t size, uint32_t offset)
{
	assert(m_Handle != VK_NULL_HANDLE);
	assert(layout != VK_NULL_HANDLE);
	assert(data);
	assert(size > 0 && size % 4 == 0);
	vkCmdPushConstants(m_Handle, layout, stages, offset, size, data);
}

void CommandBuffer::ClearColorImage(VkImage image, VkImageLayout layout, const ClearColorValue& color, VkImageSubresourceRange range)
{
	assert(m_Handle != VK_NULL_HANDLE);
	assert(image != VK_NULL_HANDLE);

	const VkClearColorValue vkColor
	{
		.float32 = { color.Float32[0], color.Float32[1], color.Float32[2], color.Float32[3] }
	};

	vkCmdClearColorImage(m_Handle, image, layout, &vkColor, 1, &range);
}

void CommandBuffer::CopyImage(VkImage src, VkImageLayout srcLayout, VkImage dst, VkImageLayout dstLayout, const VkImageCopy2& region)
{
	assert(m_Handle != VK_NULL_HANDLE);
	assert(src != VK_NULL_HANDLE);
	assert(dst != VK_NULL_HANDLE);

	const VkCopyImageInfo2 info
	{
		.sType          = VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2,
		.srcImage       = src,
		.srcImageLayout = srcLayout,
		.dstImage       = dst,
		.dstImageLayout = dstLayout,
		.regionCount    = 1,
		.pRegions       = &region
	};

	vkCmdCopyImage2(m_Handle, &info);
}

void CommandBuffer::BlitImage(VkImage src, VkImageLayout srcLayout, VkImage dst, VkImageLayout dstLayout, const VkImageBlit2& region, VkFilter filter)
{
	assert(m_Handle != VK_NULL_HANDLE);
	assert(src != VK_NULL_HANDLE);
	assert(dst != VK_NULL_HANDLE);

	const VkBlitImageInfo2 info
	{
		.sType          = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
		.srcImage       = src,
		.srcImageLayout = srcLayout,
		.dstImage       = dst,
		.dstImageLayout = dstLayout,
		.regionCount    = 1,
		.pRegions       = &region,
		.filter         = filter
	};

	vkCmdBlitImage2(m_Handle, &info);
}

void CommandBuffer::PushDebugLabel(const char* label, uint32_t colorRGBA) const
{
	if (!label || !vkCmdBeginDebugUtilsLabelEXT)
		return;

	const VkDebugUtilsLabelEXT info
	{
		.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
		.pLabelName = label,
		.color      =
		{
			((colorRGBA >> 24) & 0xFF) / 255.0f,
			((colorRGBA >> 16) & 0xFF) / 255.0f,
			((colorRGBA >>  8) & 0xFF) / 255.0f,
			((colorRGBA >>  0) & 0xFF) / 255.0f
		}
	};

	vkCmdBeginDebugUtilsLabelEXT(m_Handle, &info);
}

void CommandBuffer::PopDebugLabel() const
{
	if (!vkCmdEndDebugUtilsLabelEXT)
		return;

	vkCmdEndDebugUtilsLabelEXT(m_Handle);
}

void CommandBuffer::InsertDebugLabel(const char* label, uint32_t colorRGBA) const
{
	if (!label || !vkCmdInsertDebugUtilsLabelEXT)
		return;

	const VkDebugUtilsLabelEXT info
	{
		.sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
		.pLabelName = label,
		.color      =
		{
			((colorRGBA >> 24) & 0xFF) / 255.0f,
			((colorRGBA >> 16) & 0xFF) / 255.0f,
			((colorRGBA >>  8) & 0xFF) / 255.0f,
			((colorRGBA >>  0) & 0xFF) / 255.0f
		}
	};

	vkCmdInsertDebugUtilsLabelEXT(m_Handle, &info);
}

void CommandBuffer::PipelineBarrier(const VkDependencyInfo& dep)
{
	assert(m_Handle != VK_NULL_HANDLE);
	vkCmdPipelineBarrier2(m_Handle, &dep);
}

void CommandBuffer::ImageBarrier(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageSubresourceRange range, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage)
{
	assert(m_Handle != VK_NULL_HANDLE);

	VkAccessFlags2 srcAccess = VK_ACCESS_2_NONE;
	VkAccessFlags2 dstAccess = VK_ACCESS_2_NONE;

	switch (oldLayout)
	{
		case VK_IMAGE_LAYOUT_UNDEFINED:
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			srcAccess = VK_ACCESS_2_NONE;
			break;
		case VK_IMAGE_LAYOUT_PREINITIALIZED:
			srcAccess = VK_ACCESS_2_HOST_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			srcAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			srcAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			srcAccess = VK_ACCESS_2_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			srcAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			srcAccess = VK_ACCESS_2_SHADER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_GENERAL:
			srcAccess = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
			break;
		default:
			break;
	}

	switch (newLayout)
	{
		case VK_IMAGE_LAYOUT_UNDEFINED:
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			dstAccess = VK_ACCESS_2_NONE;
			break;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			dstAccess = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			dstAccess = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			dstAccess = VK_ACCESS_2_TRANSFER_WRITE_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			dstAccess = VK_ACCESS_2_TRANSFER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			dstAccess = VK_ACCESS_2_SHADER_READ_BIT;
			break;
		case VK_IMAGE_LAYOUT_GENERAL:
			dstAccess = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
			break;
		default:
			break;
	}

	const VkImageMemoryBarrier2 barrier
	{
		.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask        = srcStage,
		.srcAccessMask       = srcAccess,
		.dstStageMask        = dstStage,
		.dstAccessMask       = dstAccess,
		.oldLayout           = oldLayout,
		.newLayout           = newLayout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image               = image,
		.subresourceRange    = range
	};

	const VkDependencyInfo dep
	{
		.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers    = &barrier
	};

	vkCmdPipelineBarrier2(m_Handle, &dep);
}

void CommandBuffer::BufferBarrier(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess, VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess, uint32_t srcQueueFamilyIndex, uint32_t dstQueueFamilyIndex)
{
	assert(m_Handle != VK_NULL_HANDLE);
	assert(buffer   != VK_NULL_HANDLE);

	const VkBufferMemoryBarrier2 barrier
	{
		.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
		.srcStageMask        = srcStage,
		.srcAccessMask       = srcAccess,
		.dstStageMask        = dstStage,
		.dstAccessMask       = dstAccess,
		.srcQueueFamilyIndex = srcQueueFamilyIndex,
		.dstQueueFamilyIndex = dstQueueFamilyIndex,
		.buffer              = buffer,
		.offset              = offset,
		.size                = size
	};

	const VkDependencyInfo dependency
	{
		.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.bufferMemoryBarrierCount = 1,
		.pBufferMemoryBarriers    = &barrier
	};

	vkCmdPipelineBarrier2(m_Handle, &dependency);
}
