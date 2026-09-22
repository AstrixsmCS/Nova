#include "EditorApplication.hpp"

#include "Renderer/Vulkan/Descriptors.hpp"

#include <glm/glm.hpp>

#include <cassert>

static const float CAMERA_FOV  = glm::radians(60.0f);
static const float CAMERA_NEAR = 0.1f;
static const float CAMERA_FAR  = 1000.0f;

EditorApplication::EditorApplication(const ApplicationSpecification& specification)
	: Application(specification)
{
}

EditorApplication::~EditorApplication() = default;

void EditorApplication::OnInitialize()
{
	MaterialSystem::Initialize();

	// ==== Shader / material ====
	m_GeometryShader = std::make_shared<Shader>();
	m_GeometryShader->Load("Assets/Shaders/Mesh.slang");
	assert(m_GeometryShader->IsValid());

	m_GeometryMaterial.SetShader(m_GeometryShader);

	m_GeometryState.VertexLayout =
	{
		{ ShaderDataType::Float3, "Position" },
		{ ShaderDataType::Float3, "Normal"   },
		{ ShaderDataType::Float2, "TexCoord" },
		{ ShaderDataType::Float4, "Tangent"  },
	};
	m_GeometryState.DepthTest  = true;
	m_GeometryState.DepthWrite = true;
	m_GeometryState.CullMode   = CullMode::Back;

	// ==== Mesh ====
	const bool loaded = m_Mesh.Load("Assets/Meshes/DamagedHelmet/DamagedHelmet.glb");
	assert(loaded);

	for (const std::shared_ptr<Material>& material : m_Mesh.GetMaterials())
		MaterialSystem::PrepareMaterial(material);

	// ==== Camera ====
	for (Buffer& buffer : m_CameraBuffers)
	{
		buffer.Create(
		{
			.DebugName = "Camera Buffer",
			.Usage     = BufferUsage::Uniform,
			.Memory    = BufferMemory::HostVisible,
			.Size      = sizeof(CameraUniforms)
		});
	}

	const VkExtent2D extent      = Renderer::GetSwapChain().GetExtent();
	const float      aspectRatio = static_cast<float>(extent.width) / static_cast<float>(extent.height);

	m_Camera = Camera(CAMERA_FOV, aspectRatio, CAMERA_NEAR, CAMERA_FAR);
	m_Camera.SetPosition({ 0.0f, 0.0f, 3.0f });

	CreateDepthImage(extent.width, extent.height);
}

void EditorApplication::CreateDepthImage(uint32_t width, uint32_t height)
{
	m_DepthImage.Destroy();
	m_DepthImage.Create(
	{
		.Type      = TextureType::Texture2D,
		.Format    = Format::D32_Float,
		.Size      = { width, height, 1 },
		.Usage     = TextureUsageBits_Attachment,
		.DebugName = "Depth Image"
	});
}

void EditorApplication::DrawMesh(CommandBuffer& commandBuffer, const Mesh& mesh)
{
	const uint32_t frameSlot = Renderer::GetFrameSlot();

	assert(m_GeometryMaterial.GetShader() && !m_GeometryMaterial.GetShader()->GetPushConstantRanges().empty());

	const Shader&            shader = *m_GeometryMaterial.GetShader();
	const VkPipelineLayout   layout = shader.GetPipelineLayout();
	const PushConstantRange& range  = shader.GetPushConstantRanges()[0];

	commandBuffer.BindVertexBuffer(mesh.GetVertexBuffer());
	commandBuffer.BindIndexBuffer(mesh.GetIndexBuffer(), IndexFormat::UInt32);

	const float     lengthSquared  = glm::dot(m_DirectionalLight.Direction, m_DirectionalLight.Direction);
	const glm::vec3 lightDirection = lengthSquared > 0.000001f ? glm::normalize(m_DirectionalLight.Direction) : glm::vec3(0.0f, -1.0f, 0.0f);

	const VkDeviceAddress cameraAddress    = m_CameraBuffers[frameSlot].GetDeviceAddress();
	const VkDeviceAddress materialsAddress = MaterialSystem::GetBuffer(frameSlot).GetDeviceAddress();

	m_GeometryMaterial.Set("UBCamera",            cameraAddress);
	m_GeometryMaterial.Set("SBMaterials",         materialsAddress);
	m_GeometryMaterial.Set("LightDirection",      glm::vec4(lightDirection, 0.0f));
	m_GeometryMaterial.Set("LightColorIntensity", glm::vec4(m_DirectionalLight.Color, m_DirectionalLight.Intensity));

	const std::vector<std::shared_ptr<Material>>& materials = mesh.GetMaterials();

	for (const Submesh& submesh : mesh.GetSubmeshes())
	{
		const bool hasMaterial = submesh.MaterialIndex < materials.size();
		const uint32_t materialIndex = hasMaterial ? MaterialSystem::PrepareMaterial(materials[submesh.MaterialIndex]) : MaterialSystem::FallbackIndex;

		m_GeometryMaterial.Set("Model",         submesh.Transform);
		m_GeometryMaterial.Set("MaterialIndex", materialIndex);

		const std::vector<uint8_t>& storage = m_GeometryMaterial.GetUniformStorage();
		assert(storage.size() == range.Offset + range.Size);

		commandBuffer.PushConstants(layout, range.StageFlags, storage.data() + range.Offset, range.Size, range.Offset);

		commandBuffer.DrawIndexed(submesh.IndexCount, 1, submesh.BaseIndex, static_cast<int32_t>(submesh.BaseVertex));
	}
}

void EditorApplication::OnUpdate(Timestep ts)
{
	SwapChain&       swapChain = Renderer::GetSwapChain();
	const VkExtent2D extent    = swapChain.GetExtent();
	const uint32_t   frameSlot = Renderer::GetFrameSlot();

	if (m_DepthImage.GetWidth() != extent.width || m_DepthImage.GetHeight() != extent.height)
	{
		Renderer::WaitForGPU();
		CreateDepthImage(extent.width, extent.height);

		const float aspectRatio = static_cast<float>(extent.width) / static_cast<float>(extent.height);
		m_Camera.SetPerspective(CAMERA_FOV, aspectRatio, CAMERA_NEAR, CAMERA_FAR);
	}

	// ==== Per-frame GPU data ====
	m_Camera.OnUpdate(ts);

	const glm::mat4 viewProjection = m_Camera.GetViewProjection();

	const CameraUniforms cameraData
	{
		.ViewProjection        = viewProjection,
		.InverseViewProjection = glm::inverse(viewProjection),
		.Position              = m_Camera.GetPosition()
	};

	m_CameraBuffers[frameSlot].SetData(&cameraData, sizeof(CameraUniforms));

	MaterialSystem::UploadPendingMaterials(frameSlot);

	// ==== Barriers: undefined → attachments ====
	CommandBuffer& commandBuffer = Renderer::AcquireCommandBuffer();

	commandBuffer.ImageBarrier(swapChain.GetCurrentImage(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	commandBuffer.ImageBarrier(m_DepthImage.GetHandle(),    VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	// ==== Geometry pass ====
	const RenderingAttachmentInfo color
	{
		.ImageView  = swapChain.GetCurrentImageView(),
		.LoadOp     = LoadOp::Clear,
		.StoreOp    = StoreOp::Store,
		.ClearValue = { .Color = { .Float32 = { 0.03f, 0.02f, 0.08f, 1.0f } } },
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
		DebugLabelScope label(commandBuffer, "Geometry Pass", 0xAEC6CFFF);

		m_GeometryShader->Bind(commandBuffer);
		commandBuffer.SetGraphicsState(m_GeometryState, 1);

		const VkDescriptorSet descriptorSet = Descriptor::GetSet();
		vkCmdBindDescriptorSets(commandBuffer.GetHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_GeometryShader->GetPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

		DrawMesh(commandBuffer, m_Mesh);
	}
	commandBuffer.EndRendering();

	// ==== Barrier: color attachment → present ====
	commandBuffer.ImageBarrier(swapChain.GetCurrentImage(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

void EditorApplication::OnShutdown()
{
	Renderer::WaitForGPU();

	MaterialSystem::Shutdown();
	m_Mesh.Destroy();

	m_DepthImage.Destroy();

	for (Buffer& buffer : m_CameraBuffers)
		buffer.Destroy();

	m_GeometryMaterial.SetShader(nullptr);
	m_GeometryShader->Shutdown();
	m_GeometryShader.reset();
}
