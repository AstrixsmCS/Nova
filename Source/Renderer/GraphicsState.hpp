#pragma once

#include "Vulkan.hpp"

#include "Buffer.hpp"
#include "CommandBuffer.hpp"
#include "RendererTypes.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

enum class BlendMode : uint8_t
{
	None = 0,
	Alpha,
	PremultipliedAlpha,
	Additive,
	Multiply
};

struct GraphicsState
{
	VertexBufferLayout VertexLayout;

	Topology  PrimitiveTopology = Topology::Triangle;
	CompareOp DepthCompare      = CompareOp::Less;
	BlendMode Blending          = BlendMode::None;

	VkCullModeFlags CullMode    = VK_CULL_MODE_BACK_BIT;
	VkFrontFace     FrontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	VkPolygonMode   PolygonMode = VK_POLYGON_MODE_FILL;

	VkSampleCountFlagBits Samples = VK_SAMPLE_COUNT_1_BIT;

	bool  DepthTest  = true;
	bool  DepthWrite = true;
	float LineWidth  = 1.0f;

	void Apply(CommandBuffer& commandBuffer, uint32_t colorAttachmentCount) const
	{
		const VkCommandBuffer cmd = commandBuffer.GetHandle();

		ApplyVertexInput(cmd);
		ApplyInputAssembly(cmd);
		ApplyRasterization(cmd);
		ApplyMultisampling(cmd);
		ApplyDepthStencil(cmd);
		ApplyColorBlending(cmd, colorAttachmentCount);
	}

private:
	static VkFormat ShaderDataTypeToVulkanFormat(ShaderDataType type)
	{
		switch (type)
		{
			case ShaderDataType::Float:  return VK_FORMAT_R32_SFLOAT;
			case ShaderDataType::Float2: return VK_FORMAT_R32G32_SFLOAT;
			case ShaderDataType::Float3: return VK_FORMAT_R32G32B32_SFLOAT;
			case ShaderDataType::Float4: return VK_FORMAT_R32G32B32A32_SFLOAT;
			case ShaderDataType::Int:    return VK_FORMAT_R32_SINT;
			case ShaderDataType::Int2:   return VK_FORMAT_R32G32_SINT;
			case ShaderDataType::Int3:   return VK_FORMAT_R32G32B32_SINT;
			case ShaderDataType::Int4:   return VK_FORMAT_R32G32B32A32_SINT;
			default:
				return VK_FORMAT_UNDEFINED;
		}
	}

	static VkColorBlendEquationEXT CreateBlendEquation(BlendMode mode)
	{
		if (mode == BlendMode::None)
		{
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

		return
		{
			.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.colorBlendOp        = VK_BLEND_OP_ADD,
			.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
			.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.alphaBlendOp        = VK_BLEND_OP_ADD
		};
	}

	void ApplyVertexInput(VkCommandBuffer cmd) const
	{
		std::vector<VkVertexInputBindingDescription2EXT> bindings;
		std::vector<VkVertexInputAttributeDescription2EXT> attributes;

		if (VertexLayout.GetElementCount() > 0)
		{
			bindings.push_back(
			{
				.sType     = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT,
				.binding   = 0,
				.stride    = VertexLayout.GetStride(),
				.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
				.divisor   = 1
			});

			uint32_t location = 0;

			for (const auto& element : VertexLayout)
			{
				const VkFormat format = ShaderDataTypeToVulkanFormat(element.Type);

				assert(format != VK_FORMAT_UNDEFINED);

				attributes.push_back(
				{
					.sType    = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT,
					.location = location++,
					.binding  = 0,
					.format   = format,
					.offset   = static_cast<uint32_t>(element.Offset)
				});
			}
		}

		vkCmdSetVertexInputEXT(
			cmd,
			static_cast<uint32_t>(bindings.size()),
			bindings.empty() ? nullptr : bindings.data(),
			static_cast<uint32_t>(attributes.size()),
			attributes.empty() ? nullptr : attributes.data()
		);
	}

	void ApplyInputAssembly(VkCommandBuffer cmd) const
	{
		vkCmdSetPrimitiveTopology(cmd, ToVulkan(PrimitiveTopology));
		vkCmdSetPrimitiveRestartEnable(cmd, VK_FALSE);
	}

	void ApplyRasterization(VkCommandBuffer cmd) const
	{
		vkCmdSetRasterizerDiscardEnable(cmd, VK_FALSE);
		vkCmdSetPolygonModeEXT(cmd, PolygonMode);
		vkCmdSetCullMode(cmd, CullMode);
		vkCmdSetFrontFace(cmd, FrontFace);
		vkCmdSetDepthBiasEnable(cmd, VK_FALSE);
		vkCmdSetDepthClampEnableEXT(cmd, VK_FALSE);
		vkCmdSetLineWidth(cmd, LineWidth);
	}

	void ApplyMultisampling(VkCommandBuffer cmd) const
	{
		vkCmdSetRasterizationSamplesEXT(cmd, Samples);

		const VkSampleMask sampleMask = ~VkSampleMask{ 0 };

		vkCmdSetSampleMaskEXT(cmd, Samples, &sampleMask);

		vkCmdSetAlphaToCoverageEnableEXT(cmd, VK_FALSE);
		vkCmdSetAlphaToOneEnableEXT(cmd, VK_FALSE);
	}

	void ApplyDepthStencil(VkCommandBuffer cmd) const
	{
		vkCmdSetDepthTestEnable(cmd, DepthTest ? VK_TRUE : VK_FALSE);
		vkCmdSetDepthWriteEnable(cmd, DepthWrite ? VK_TRUE : VK_FALSE);
		vkCmdSetDepthCompareOp(cmd, ToVulkan(DepthCompare));

		vkCmdSetDepthBoundsTestEnable(cmd, VK_FALSE);
		vkCmdSetStencilTestEnable(cmd, VK_FALSE);
	}

	void ApplyColorBlending(VkCommandBuffer cmd, uint32_t colorAttachmentCount) const
	{
		vkCmdSetLogicOpEnableEXT(cmd, VK_FALSE);

		if (colorAttachmentCount == 0)
			return;

		constexpr uint32_t maxColorAttachments = 8;

		assert(colorAttachmentCount <= maxColorAttachments);

		const bool blendEnabled = Blending != BlendMode::None;

		const VkColorBlendEquationEXT blendEquation = CreateBlendEquation(Blending);

		constexpr VkColorComponentFlags writeMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		std::array<VkBool32, maxColorAttachments> blendEnables;
		std::array<VkColorBlendEquationEXT, maxColorAttachments> blendEquations;
		std::array<VkColorComponentFlags, maxColorAttachments> writeMasks;

		for (uint32_t i = 0; i < colorAttachmentCount; ++i)
		{
			blendEnables[i]   = blendEnabled ? VK_TRUE : VK_FALSE;
			blendEquations[i] = blendEquation;
			writeMasks[i]     = writeMask;
		}

		vkCmdSetColorBlendEnableEXT(cmd, 0, colorAttachmentCount, blendEnables.data());
		vkCmdSetColorBlendEquationEXT(cmd, 0, colorAttachmentCount, blendEquations.data());
		vkCmdSetColorWriteMaskEXT(cmd, 0, colorAttachmentCount, writeMasks.data());
	}
};
