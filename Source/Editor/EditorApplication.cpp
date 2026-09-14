#include "EditorApplication.hpp"

#include "Renderer/Renderer.hpp"
#include "Renderer/Descriptors.hpp"
#include "Renderer/DynamicRendering.hpp"

EditorApplication::EditorApplication(const ApplicationSpecification& specification)
	: Application(specification)
{
}

EditorApplication::~EditorApplication() = default;

void EditorApplication::OnInitialize()
{
	Descriptor::Initialize();

	struct Vertex
	{
		float Position[2];
		float Color[3];
	};

	const Vertex vertices[3] =
	{
		{ { 0.0f,  0.5f }, { 1.0f, 0.2f, 0.2f } },
		{ {-0.5f, -0.5f }, { 0.2f, 1.0f, 0.2f } },
		{ { 0.5f, -0.5f }, { 0.2f, 0.4f, 1.0f } },
	};

	m_VertexBuffer.Create(vertices, sizeof(vertices), VertexBufferUsage::Static);
	m_VertexBuffer.SetLayout({
		{ ShaderDataType::Float2, "Position" },
		{ ShaderDataType::Float3, "Color"    },
	});

	m_TriangleShader = std::make_shared<Shader>();
	m_TriangleShader->Load("Assets/Shaders/Triangle.slang");

	SwapChain& swapChain = Renderer::GetSwapChain();

	GraphicsPipelineSpecification spec;
	spec.DebugName       = "Triangle";
	spec.Shader          = m_TriangleShader;
	spec.ColorFormats    = { swapChain.GetColorFormat() };
	spec.DepthFormat     = Format::Invalid;
	spec.Layout          = m_VertexBuffer.GetLayout();
	spec.BackfaceCulling = true;
	spec.DepthTest       = false;
	spec.DepthWrite      = false;

	m_TrianglePipeline.Create(spec);
}

void EditorApplication::OnUpdate()
{
	CommandBuffer& cmd  = Renderer::GetCurrentCommandBuffer();
	SwapChain&     swap = Renderer::GetSwapChain();

	const VkImageMemoryBarrier2 toAttachment
	{
		.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask        = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
		.srcAccessMask       = VK_ACCESS_2_NONE,
		.dstStageMask        = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstAccessMask       = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image               = swap.GetCurrentImage(),
		.subresourceRange    =
		{
			.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel   = 0,
			.levelCount     = 1,
			.baseArrayLayer = 0,
			.layerCount     = 1,
		},
	};

	const VkDependencyInfo toAttachmentDep
	{
		.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers    = &toAttachment,
	};

	vkCmdPipelineBarrier2(cmd.GetHandle(), &toAttachmentDep);

	// ==== Render pass ====
	const AttachmentInfo color
	{
		.ImageView  = swap.GetCurrentImageView(),
		.Format     = swap.GetColorFormat(),
		.LoadOp     = LoadOp::Clear,
		.StoreOp    = StoreOp::Store,
		.ClearValue = { .color = { .float32 = { 0.03f, 0.02f, 0.08f, 1.0f } } },
		.Layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};

	const RenderPassInfo passInfo
	{
		.ColorAttachments = { &color, 1 },
		.RenderArea       = { {0, 0}, swap.GetExtent() },
	};

	DynamicRendering::BeginRendering(cmd, passInfo);

	m_TrianglePipeline.Bind(cmd);

	const VkBuffer     vertexBuffer = m_VertexBuffer.GetBuffer();
	const VkDeviceSize offset       = 0;
	vkCmdBindVertexBuffers(cmd.GetHandle(), 0, 1, &vertexBuffer, &offset);

	vkCmdDraw(cmd.GetHandle(), 3, 1, 0, 0);

	DynamicRendering::EndRendering(cmd);

	const VkImageMemoryBarrier2 toPresent
	{
		.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
		.srcStageMask        = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask       = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		.dstStageMask        = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
		.dstAccessMask       = VK_ACCESS_2_NONE,
		.oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image               = swap.GetCurrentImage(),
		.subresourceRange    =
		{
			.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel   = 0,
			.levelCount     = 1,
			.baseArrayLayer = 0,
			.layerCount     = 1,
		},
	};

	const VkDependencyInfo toPresentDep
	{
		.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.imageMemoryBarrierCount = 1,
		.pImageMemoryBarriers    = &toPresent,
	};

	vkCmdPipelineBarrier2(cmd.GetHandle(), &toPresentDep);
}

void EditorApplication::OnShutdown()
{
	Renderer::WaitForGPU();

	m_VertexBuffer.Destroy();
	m_TrianglePipeline.Shutdown();
	m_TriangleShader->Shutdown();

	Descriptor::Shutdown();
}
