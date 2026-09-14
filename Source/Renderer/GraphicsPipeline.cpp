#include "GraphicsPipeline.hpp"

#include "RendererContext.hpp"

#include "Descriptors.hpp"

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

		case ShaderDataType::UInt:   return VK_FORMAT_R32_UINT;
		case ShaderDataType::UInt2:  return VK_FORMAT_R32G32_UINT;
		case ShaderDataType::UInt3:  return VK_FORMAT_R32G32B32_UINT;
		case ShaderDataType::UInt4:  return VK_FORMAT_R32G32B32A32_UINT;

		case ShaderDataType::Bool:
			break;

		case ShaderDataType::Mat3:
		case ShaderDataType::Mat4:
			break;

		case ShaderDataType::None:
			break;
	}

	assert(false && "Unsupported ShaderDataType!");
	return VK_FORMAT_UNDEFINED;
}

static bool IsIntegerFormat(Format format)
{
	switch (format)
	{
		case Format::R8_UInt:
		case Format::RG8_UInt:
		case Format::RGBA8_UInt:

		case Format::R16_UInt:
		case Format::RG16_UInt:
		case Format::RGBA16_UInt:

		case Format::R32_UInt:
		case Format::RG32_UInt:
		case Format::RGB32_UInt:
		case Format::RGBA32_UInt:
			return true;

		default:
			return false;
	}
}

void Pipeline::Create(const GraphicsPipelineSpecification &specification)
{
	m_Specification = specification;

	VkDevice device = RendererContext::Get().GetDevice();

	CreatePipelineLayout();

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Shader Stages
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	assert(m_Specification.Shader);
	assert(m_Specification.Shader->IsValid());

	const auto& shaderStages = m_Specification.Shader->GetStageInfos();

	assert(!shaderStages.empty());

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Dynamic Rendering
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	std::vector<VkFormat> colorFormats;
	colorFormats.reserve(m_Specification.ColorFormats.size());

	for (Format format : m_Specification.ColorFormats)
	{
		colorFormats.push_back(ToVulkan(format));
	}

	VkPipelineRenderingCreateInfo renderingInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = static_cast<uint32_t>(colorFormats.size()),
		.pColorAttachmentFormats = colorFormats.empty() ? nullptr : colorFormats.data(),
		.depthAttachmentFormat = m_Specification.DepthFormat != Format::Invalid ? ToVulkan(m_Specification.DepthFormat) : VK_FORMAT_UNDEFINED,
		.stencilAttachmentFormat = VK_FORMAT_UNDEFINED
	};

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Vertex Input
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	VertexBufferLayout& vertexLayout = m_Specification.Layout;

	std::vector<VkVertexInputBindingDescription> vertexInputBindingDescriptions;

	if (vertexLayout.GetElementCount())
	{
		VkVertexInputBindingDescription& vertexInputBinding = vertexInputBindingDescriptions.emplace_back();

		vertexInputBinding.binding = 0;
		vertexInputBinding.stride = vertexLayout.GetStride();
		vertexInputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	}

	std::vector<VkVertexInputAttributeDescription> vertexInputAttributes(vertexLayout.GetElementCount());

	uint32_t location = 0;

	for (const auto& element : vertexLayout)
	{
		vertexInputAttributes[location].binding = 0;
		vertexInputAttributes[location].location = location;
		vertexInputAttributes[location].format = ShaderDataTypeToVulkanFormat(element.Type);
		vertexInputAttributes[location].offset = element.Offset;

		location++;
	}

	VkPipelineVertexInputStateCreateInfo vertexInputInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindingDescriptions.size()),
		.pVertexBindingDescriptions = vertexInputBindingDescriptions.empty() ? nullptr : vertexInputBindingDescriptions.data(),
		.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size()),
		.pVertexAttributeDescriptions = vertexInputAttributes.empty() ? nullptr : vertexInputAttributes.data()
	};

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Input Assembly
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = ToVulkan(m_Specification.Topology),
		.primitiveRestartEnable = VK_FALSE
	};

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Viewport
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	VkPipelineViewportStateCreateInfo viewportInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1
	};

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Rasterization
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	const VkCullModeFlags cullMode = m_Specification.BackfaceCulling ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;

	VkPipelineRasterizationStateCreateInfo rasterizationInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = m_Specification.Wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL,
		.cullMode = cullMode,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
		.lineWidth = m_Specification.LineWidth
	};

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Multisampling
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	VkPipelineMultisampleStateCreateInfo multisampleInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = VK_FALSE
	};

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Depth / Stencil
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	VkPipelineDepthStencilStateCreateInfo depthStencilInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = m_Specification.DepthTest ? VK_TRUE : VK_FALSE,
		.depthWriteEnable = m_Specification.DepthWrite ? VK_TRUE : VK_FALSE,
		.depthCompareOp = ToVulkan(m_Specification.DepthCompareOp),
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable = VK_FALSE
	};

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Blending
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(m_Specification.ColorFormats.size());

	for (uint32_t i = 0; i < blendAttachments.size(); i++)
	{
		VkPipelineColorBlendAttachmentState& attachment = blendAttachments[i];

		attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		// Vulkan doesn't support blending on integer attachments.
		if (IsIntegerFormat(m_Specification.ColorFormats[i]))
		{
			attachment.blendEnable = VK_FALSE;
			continue;
		}

		switch (m_Specification.BlendMode)
		{
			case BlendMode::None:
			{
				attachment.blendEnable = VK_FALSE;
				break;
			}

			case BlendMode::Alpha:
			{
				attachment.blendEnable = VK_TRUE;

				attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
				attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				attachment.colorBlendOp = VK_BLEND_OP_ADD;

				attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
				attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				attachment.alphaBlendOp = VK_BLEND_OP_ADD;

				break;
			}

			case BlendMode::PremultipliedAlpha:
			{
				attachment.blendEnable = VK_TRUE;

				attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
				attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				attachment.colorBlendOp = VK_BLEND_OP_ADD;

				attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
				attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
				attachment.alphaBlendOp = VK_BLEND_OP_ADD;

				break;
			}

			case BlendMode::Additive:
			{
				attachment.blendEnable = VK_TRUE;

				attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
				attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
				attachment.colorBlendOp = VK_BLEND_OP_ADD;

				attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
				attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
				attachment.alphaBlendOp = VK_BLEND_OP_ADD;

				break;
			}

			case BlendMode::Multiply:
			{
				attachment.blendEnable = VK_TRUE;

				attachment.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
				attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
				attachment.colorBlendOp = VK_BLEND_OP_ADD;

				attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
				attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
				attachment.alphaBlendOp = VK_BLEND_OP_ADD;

				break;
			}
		}
	}

	VkPipelineColorBlendStateCreateInfo blendInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.attachmentCount = static_cast<uint32_t>(blendAttachments.size()),
		.pAttachments = blendAttachments.empty() ? nullptr : blendAttachments.data()
	};

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Dynamic State
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	const VkDynamicState dynamicStates[]
	{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicStateInfo
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = static_cast<uint32_t>(std::size(dynamicStates)),
		.pDynamicStates = dynamicStates
	};

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Graphics Pipeline
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	VkGraphicsPipelineCreateInfo pipelineInfo
	{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &renderingInfo,
		.stageCount = static_cast<uint32_t>(shaderStages.size()),
		.pStages = shaderStages.data(),
		.pVertexInputState = &vertexInputInfo,
		.pInputAssemblyState = &inputAssemblyInfo,
		.pViewportState = &viewportInfo,
		.pRasterizationState = &rasterizationInfo,
		.pMultisampleState = &multisampleInfo,
		.pDepthStencilState = &depthStencilInfo,
		.pColorBlendState = &blendInfo,
		.pDynamicState = &dynamicStateInfo,
		.layout = m_PipelineLayout,
		.renderPass = VK_NULL_HANDLE,
		.subpass = 0
	};

	VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline));

	if (!m_Specification.DebugName.empty())
	{
		SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_PIPELINE, m_Specification.DebugName, m_Pipeline);
	}

}

void Pipeline::CreatePipelineLayout()
{
	VkDevice device = RendererContext::Get().GetDevice();

	const auto& reflectedPushConstants = m_Specification.Shader->GetPushConstantRanges();

	std::vector<VkPushConstantRange> pushConstantRanges;
	pushConstantRanges.reserve(reflectedPushConstants.size());

	for (const PushConstantRange& range : reflectedPushConstants)
	{
		pushConstantRanges.push_back(
		{
			.stageFlags = range.StageFlags,
			.offset     = range.Offset,
			.size       = range.Size
		});
	}

	const VkDescriptorSetLayout descriptorSetLayout = Descriptor::GetLayout();

	VkPipelineLayoutCreateInfo layoutInfo
	{
		.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount         = 1,
		.pSetLayouts            = &descriptorSetLayout,
		.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size()),
		.pPushConstantRanges    = pushConstantRanges.empty() ? nullptr : pushConstantRanges.data()
	};

	VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_PipelineLayout));

	if (!m_Specification.DebugName.empty())
	{
		const std::string layoutName = m_Specification.DebugName + " Layout";
		SetDebugUtilsObjectName(device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, layoutName, m_PipelineLayout);
	}
}

void Pipeline::Bind(CommandBuffer& commandBuffer) const
{
	vkCmdBindPipeline(commandBuffer.GetHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
}

void Pipeline::Shutdown()
{
	VkDevice device = RendererContext::Get().GetDevice();

	if (m_Pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(device, m_Pipeline, nullptr);
		m_Pipeline = VK_NULL_HANDLE;
	}

	if (m_PipelineLayout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
		m_PipelineLayout = VK_NULL_HANDLE;
	}
}
