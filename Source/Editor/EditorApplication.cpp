#include "EditorApplication.hpp"

#include "Renderer/Renderer.hpp"
#include "Renderer/Vulkan/Descriptors.hpp"

#include <glm/glm.hpp>

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
	uint32_t TextureIndex = 0;
};

EditorApplication::EditorApplication(const ApplicationSpecification& specification)
	: Application(specification)
{
}

EditorApplication::~EditorApplication() = default;

void EditorApplication::OnInitialize()
{
	Descriptor::Initialize();

	m_VertexBuffer.Create(QuadVertices, sizeof(QuadVertices), VertexBufferUsage::Static);
	m_VertexBuffer.SetLayout(
	{
		{ ShaderDataType::Float2, "Position" },
		{ ShaderDataType::Float2, "TexCoord" },
	});

	m_IndexBuffer.Create(QuadIndices, sizeof(QuadIndices));

	m_Shader = std::make_shared<Shader>();
	m_Shader->Load("Assets/Shaders/TexturedQuad.slang");
	assert(m_Shader->IsValid());

	m_GraphicsState.VertexLayout = m_VertexBuffer.GetLayout();
	m_GraphicsState.DepthTest    = false;
	m_GraphicsState.DepthWrite   = false;
	m_GraphicsState.CullMode     = CullMode::None;

	m_Texture = std::make_shared<Texture2D>();
	const bool loaded = m_Texture->Load("Assets/Textures/Test.png");
	assert(loaded && m_Texture->IsValid());

	const VkExtent2D extent = Renderer::GetSwapChain().GetExtent();
	CreateDepthImage({ .Width = extent.width, .Height = extent.height });
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

void EditorApplication::OnUpdate(Timestep /*ts*/)
{
	CommandBuffer& commandBuffer  = Renderer::GetCurrentCommandBuffer();
	SwapChain&     swapChain = Renderer::GetSwapChain();

	const VkExtent2D extent = swapChain.GetExtent();

	if (m_DepthImage.GetWidth() != extent.width || m_DepthImage.GetHeight() != extent.height)
	{
		Renderer::WaitForGPU();
		CreateDepthImage({ .Width = extent.width, .Height = extent.height });
	}

	// ==== Barriers: undefined → attachments ====

	commandBuffer.ImageBarrier(swapChain.GetCurrentImage(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	commandBuffer.ImageBarrier(m_DepthImage.GetHandle(), VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	// ==== Pass ====

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
		DebugLabelScope label(commandBuffer, "Textured Quad", 0xAEC6CFFF);

		m_Shader->Bind(commandBuffer);
		commandBuffer.SetGraphicsState(m_GraphicsState, 1);

		const VkDescriptorSet descriptorSet = Descriptor::GetSet();
		vkCmdBindDescriptorSets(commandBuffer.GetHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_Shader->GetPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

		commandBuffer.BindVertexBuffer(m_VertexBuffer.GetBuffer());
		commandBuffer.BindIndexBuffer(m_IndexBuffer.GetBuffer(), IndexFormat::UInt32);

		const auto& ranges = m_Shader->GetPushConstantRanges();
		assert(!ranges.empty());

		const QuadPushConstants push { .TextureIndex = m_Texture->GetBindlessIndex() };
		commandBuffer.PushConstants(m_Shader->GetPipelineLayout(), ranges[0].StageFlags, push, ranges[0].Offset);

		commandBuffer.DrawIndexed(m_IndexBuffer.GetCount());
	}
	commandBuffer.EndRendering();

	// ==== Barrier: color attachment → present ====

	commandBuffer.ImageBarrier(swapChain.GetCurrentImage(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

void EditorApplication::OnShutdown()
{
	Renderer::WaitForGPU();

	m_Texture.reset();
	m_Shader->Shutdown();

	m_DepthImage.Destroy();
	m_IndexBuffer.Destroy();
	m_VertexBuffer.Destroy();

	Descriptor::Shutdown();
}
