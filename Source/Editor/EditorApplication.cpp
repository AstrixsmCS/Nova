#include "EditorApplication.hpp"

#include "Renderer/Renderer.hpp"
#include "Renderer/Vulkan/Context.hpp"
#include "Renderer/Vulkan/Descriptors.hpp"

#include <glm/glm.hpp>
#include <print>

struct QuadVertex
{
	glm::vec2 Position;
	glm::vec2 TexCoord;
};

static constexpr QuadVertex QuadVertices[4] =
{
	{ { -0.5f, -0.5f }, { 0.0f, 0.0f } },
	{ {  0.5f, -0.5f }, { 1.0f, 0.0f } },
	{ {  0.5f,  0.5f }, { 1.0f, 1.0f } },
	{ { -0.5f,  0.5f }, { 0.0f, 1.0f } },
};

static constexpr uint32_t QuadIndices[6] = { 0, 1, 2, 2, 3, 0 };

struct QuadPushConstants
{
	uint32_t TextureIndex  = 0;
	uint32_t GradientIndex = 0;
};

struct ComputePushConstants
{
	float    Time   = 0.0f;
	uint32_t Width  = 0;
	uint32_t Height = 0;
};

EditorApplication::EditorApplication(const ApplicationSpecification& specification)
	: Application(specification)
{
}

EditorApplication::~EditorApplication() = default;

void EditorApplication::OnInitialize()
{
	m_VertexBuffer.Create(QuadVertices, sizeof(QuadVertices), VertexBufferUsage::Static);
	m_VertexBuffer.SetLayout(
	{
		{ ShaderDataType::Float2, "Position" },
		{ ShaderDataType::Float2, "TexCoord" },
	});

	m_IndexBuffer.Create(QuadIndices, sizeof(QuadIndices));

	m_GraphicsShader = std::make_shared<Shader>();
	m_GraphicsShader->Load("Assets/Shaders/TexturedQuad.slang");
	assert(m_GraphicsShader->IsValid());

	m_GraphicsState.VertexLayout = m_VertexBuffer.GetLayout();
	m_GraphicsState.DepthTest    = false;
	m_GraphicsState.DepthWrite   = false;
	m_GraphicsState.CullMode     = CullMode::None;

	m_ComputeShader = std::make_shared<Shader>();
	m_ComputeShader->Load("Assets/Shaders/Gradient.slang");
	assert(m_ComputeShader->IsValid());

	m_Texture = std::make_shared<Texture2D>();
	const bool loaded = m_Texture->Load("Assets/Textures/Test.png");
	assert(loaded && m_Texture->IsValid());

	const VkExtent2D extent = Renderer::GetSwapChain().GetExtent();
	CreateDepthImage({ .Width = extent.width, .Height = extent.height });
	CreateComputeImage({ .Width = extent.width, .Height = extent.height });
}

void EditorApplication::CreateDepthImage(const Dimensions& size)
{
	m_DepthImage.Destroy();
	m_DepthImage.Create(
	{
		.Format = Format::D32_Float,
		.Usage  = ImageUsage::Attachment,
		.Size   = size
	});
}

void EditorApplication::CreateComputeImage(const Dimensions& size)
{
	m_ComputeImage.Destroy();
	m_ComputeImage.Create(
	{
		.Format = Format::RGBA8_UNorm,
		.Usage  = ImageUsage::Storage,
		.Size   = size
	});
}

void EditorApplication::RecordComputePass(CommandBuffer& cmd, float time)
{
	const VkExtent2D extent    = Renderer::GetSwapChain().GetExtent();
	const bool       async     = Renderer::HasAsyncCompute();
	const uint32_t   gfxFamily = Context::Get().GetGraphicsFamily();
	const uint32_t   cmpFamily = Context::Get().GetComputeFamily();

	// UNDEFINED → GENERAL for storage write.
	cmd.ImageBarrier(m_ComputeImage.GetHandle(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_NONE, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

	m_ComputeShader->Bind(cmd);

	const ComputePushConstants push
	{
		.Time   = time,
		.Width  = extent.width,
		.Height = extent.height
	};

	const auto& ranges = m_ComputeShader->GetPushConstantRanges();
	assert(!ranges.empty());

	cmd.PushConstants(m_ComputeShader->GetPipelineLayout(), ranges[0].StageFlags, push, ranges[0].Offset);

	const VkDescriptorSet computeSet = Descriptor::GetSet();
	vkCmdBindDescriptorSets(cmd.GetHandle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_ComputeShader->GetPipelineLayout(), 0, 1, &computeSet, 0, nullptr);

	const uint32_t groupsX = (extent.width  + 7) / 8;
	const uint32_t groupsY = (extent.height + 7) / 8;
	cmd.Dispatch(groupsX, groupsY, 1);

	// Release to graphics or transition inline.
	{
		const VkImageMemoryBarrier2 release
		{
			.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask        = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
			.srcAccessMask       = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
			.dstStageMask        = async ? VK_PIPELINE_STAGE_2_NONE : VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
			.dstAccessMask       = async ? VK_ACCESS_2_NONE : VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
			.oldLayout           = VK_IMAGE_LAYOUT_GENERAL,
			.newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.srcQueueFamilyIndex = async ? cmpFamily : VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = async ? gfxFamily : VK_QUEUE_FAMILY_IGNORED,
			.image               = m_ComputeImage.GetHandle(),
			.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		};

		const VkDependencyInfo dependency
		{
			.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers    = &release
		};

		cmd.PipelineBarrier(dependency);
	}
}

void EditorApplication::OnUpdate(Timestep ts)
{
	m_Time += ts.GetSeconds();

	SwapChain&     swapChain = Renderer::GetSwapChain();
	const VkExtent2D extent  = swapChain.GetExtent();

	if (m_DepthImage.GetWidth() != extent.width || m_DepthImage.GetHeight() != extent.height)
	{
		Renderer::WaitForGPU();
		CreateDepthImage({ .Width = extent.width, .Height = extent.height });
		CreateComputeImage({ .Width = extent.width, .Height = extent.height });
	}

	// ==== Compute pass ====
	{
		CommandBuffer& computeCmd = Renderer::AcquireCommandBuffer(Renderer::HasAsyncCompute());
		DebugLabelScope label(computeCmd, "Gradient Compute", 0xFF6B6BFF);
		RecordComputePass(computeCmd, m_Time);
	}

	// ==== Acquire barrier on graphics queue ====
	CommandBuffer& commandBuffer = Renderer::AcquireCommandBuffer();

	if (Renderer::HasAsyncCompute())
	{
		const VkImageMemoryBarrier2 acquire
		{
			.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask        = VK_PIPELINE_STAGE_2_NONE,
			.srcAccessMask       = VK_ACCESS_2_NONE,
			.dstStageMask        = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
			.dstAccessMask       = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
			.oldLayout           = VK_IMAGE_LAYOUT_GENERAL,
			.newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.srcQueueFamilyIndex = Context::Get().GetComputeFamily(),
			.dstQueueFamilyIndex = Context::Get().GetGraphicsFamily(),
			.image               = m_ComputeImage.GetHandle(),
			.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		};

		const VkDependencyInfo dependency
		{
			.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers    = &acquire
		};

		commandBuffer.PipelineBarrier(dependency);
	}

	// ==== Barriers: undefined → attachments ====
	commandBuffer.ImageBarrier(swapChain.GetCurrentImage(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	commandBuffer.ImageBarrier(m_DepthImage.GetHandle(), VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	// ==== Graphics pass ====
	const RenderingAttachmentInfo color
	{
		.ImageView  = swapChain.GetCurrentImageView(),
		.LoadOp     = LoadOp::Clear,
		.StoreOp    = StoreOp::Store,
		.ClearValue = { .Color = { .Float32 = { 0.1f, 0.1f, 0.1f, 1.0f } } },
		.Layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};

	const RenderingAttachmentInfo depth
	{
		.ImageView  = m_DepthImage.GetAttachmentView(),
		.LoadOp     = LoadOp::Clear,
		.StoreOp    = StoreOp::DontCare,
		.ClearValue = { .DepthStencil = { 1.0f, 0 } },
		.Layout     = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
	};

	const RenderingInfo passInfo
	{
		.ColorAttachments = { &color, 1 },
		.DepthAttachment  = &depth,
		.RenderArea =
		{
			.X      = 0,
			.Y      = 0,
			.Width  = extent.width,
			.Height = extent.height
		},
	};

	commandBuffer.BeginRendering(passInfo);
	{
		DebugLabelScope label(commandBuffer, "Textured Quad Gradient", 0xAEC6CFFF);

		m_GraphicsShader->Bind(commandBuffer);
		commandBuffer.SetGraphicsState(m_GraphicsState, 1);

		const VkDescriptorSet descriptorSet = Descriptor::GetSet();
		vkCmdBindDescriptorSets(commandBuffer.GetHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsShader->GetPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

		commandBuffer.BindVertexBuffer(m_VertexBuffer.GetBuffer());
		commandBuffer.BindIndexBuffer(m_IndexBuffer.GetBuffer(), IndexFormat::UInt32);

		const auto& ranges = m_GraphicsShader->GetPushConstantRanges();
		assert(!ranges.empty());

		const QuadPushConstants push
		{
			.TextureIndex  = m_Texture->GetBindlessIndex(),
			.GradientIndex = m_ComputeImage.GetBindlessIndex()
		};

		commandBuffer.PushConstants(m_GraphicsShader->GetPipelineLayout(), ranges[0].StageFlags, push, ranges[0].Offset);
		commandBuffer.DrawIndexed(m_IndexBuffer.GetCount());
	}
	commandBuffer.EndRendering();

	// ==== Barrier: color attachment → present ====

	commandBuffer.ImageBarrier(swapChain.GetCurrentImage(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

void EditorApplication::OnShutdown()
{
	Renderer::WaitForGPU();

	m_ComputeShader->Shutdown();
	m_GraphicsShader->Shutdown();

	m_ComputeImage.Destroy();
	m_DepthImage.Destroy();
	m_IndexBuffer.Destroy();
	m_VertexBuffer.Destroy();
	m_Texture.reset();
}
