#include "EditorApplication.hpp"

#include "Renderer/Descriptors.hpp"
#include "Renderer/DynamicRendering.hpp"
#include "Renderer/Renderer.hpp"

#include "Core/Log.hpp"

#include <glm/glm.hpp>

#include <cassert>

EditorApplication::EditorApplication(const ApplicationSpecification& specification)
	: Application(specification)
{
}

EditorApplication::~EditorApplication() = default;

void EditorApplication::CreateDepthImage(uint32_t width, uint32_t height)
{
	m_DepthImage.Create(
	{
		.DebugName = "Depth",
		.Format    = Format::D32_Float,
		.Usage     = ImageUsage::Attachment,
		.Width     = width,
		.Height    = height,
		.Mips      = 1,
	});
}

void EditorApplication::OnInitialize()
{
	Descriptor::Initialize();
	MaterialSystem::Initialize();

	m_GeometryShader = std::make_shared<Shader>();
	m_GeometryShader->Load("Assets/Shaders/Mesh.slang");

	m_GeometryMaterial.SetShader(m_GeometryShader);

	m_GeometryState.VertexLayout =
	{
		{ ShaderDataType::Float3, "Position" },
		{ ShaderDataType::Float3, "Normal"   },
		{ ShaderDataType::Float2, "TexCoord" },
		{ ShaderDataType::Float4, "Tangent"  },
	};
	m_GeometryState.CullMode     = CullMode::Back;
	m_GeometryState.DepthTest    = true;
	m_GeometryState.DepthWrite   = true;
	m_GeometryState.DepthCompare = CompareOp::Less;

	m_Mesh.Load("Assets/Meshes/Sponza/Sponza.gltf");

	for (const std::shared_ptr<Material>& material : m_Mesh.GetMaterials())
		MaterialSystem::PrepareMaterial(material);

	for (UniformBuffer& buffer : m_CameraBuffers)
		buffer.Create(sizeof(CameraUniforms));

	const uint32_t width  = Renderer::GetSwapChain().GetWidth();
	const uint32_t height = Renderer::GetSwapChain().GetHeight();
	const float aspectRatio = static_cast<float>(width) / static_cast<float>(height);

	m_Camera = Camera(glm::radians(60.0f), aspectRatio, 0.1f, 1000.0f);
	m_Camera.SetPosition({ 0.0f, 0.0f, 3.0f });

	CreateDepthImage(width, height);
}

void EditorApplication::DrawMesh(CommandBuffer& commandBuffer, const Mesh& mesh)
{
	const VkBuffer     vb     = mesh.GetVertexBuffer();
	const VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(commandBuffer.GetHandle(), 0, 1, &vb, &offset);
	vkCmdBindIndexBuffer(commandBuffer.GetHandle(), mesh.GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

	const uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

	const uint64_t cameraAddress   = m_CameraBuffers[frameIndex].GetDeviceAddress();
	const uint64_t materialsAddress = MaterialSystem::GetBuffer(frameIndex).GetDeviceAddress();

	const auto& shader = m_GeometryMaterial.GetShader();
	const auto& ranges = shader->GetPushConstantRanges();
	assert(!ranges.empty());
	const auto& range = ranges[0];

	const float     directionLengthSquared = glm::dot(m_DirectionalLight.Direction, m_DirectionalLight.Direction);
	const glm::vec3 lightDirection         = directionLengthSquared > 0.000001f ? glm::normalize(m_DirectionalLight.Direction) : glm::vec3(0.0f, -1.0f, 0.0f);

	m_GeometryMaterial.Set("UBCamera",            cameraAddress);
	m_GeometryMaterial.Set("SBMaterials",         materialsAddress);
	m_GeometryMaterial.Set("LightDirection",      glm::vec4(lightDirection, 0.0f));
	m_GeometryMaterial.Set("LightColorIntensity", glm::vec4(m_DirectionalLight.Color, m_DirectionalLight.Intensity));

	const auto& materials = mesh.GetMaterials();

	for (const Submesh& submesh : mesh.GetSubmeshes())
	{
		const std::shared_ptr<Material>& material = (submesh.MaterialIndex != UINT32_MAX && submesh.MaterialIndex < materials.size()) ? materials[submesh.MaterialIndex] : nullptr;

		const uint32_t materialIndex = MaterialSystem::PrepareMaterial(material);

		m_GeometryMaterial.Set("Model",         submesh.Transform);
		m_GeometryMaterial.Set("MaterialIndex", materialIndex);

		const auto& storage = m_GeometryMaterial.GetUniformStorage();
		assert(storage.size() == range.Size);

		vkCmdPushConstants(commandBuffer.GetHandle(), shader->GetPipelineLayout(), range.StageFlags, range.Offset, static_cast<uint32_t>(storage.size()), storage.data());

		vkCmdDrawIndexed(commandBuffer.GetHandle(), submesh.IndexCount, 1, submesh.BaseIndex, static_cast<int32_t>(submesh.BaseVertex), 0);
	}
}

void EditorApplication::OnUpdate(Timestep ts)
{
	m_Camera.OnUpdate(ts);

	const uint32_t frameIndex = Renderer::GetCurrentFrameIndex();

	MaterialSystem::UploadPendingMaterials(frameIndex);

	CommandBuffer& cmd  = Renderer::GetCurrentCommandBuffer();
	SwapChain&     swap = Renderer::GetSwapChain();

	{
		const uint32_t width  = swap.GetWidth();
		const uint32_t height = swap.GetHeight();

		if (m_DepthImage.GetWidth()  != width || m_DepthImage.GetHeight() != height)
		{
			Renderer::WaitForGPU();
			CreateDepthImage(width, height);

			const float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
			m_Camera.SetPerspective(glm::radians(60.0f), aspectRatio, 0.1f, 1000.0f);
		}
	}

	{
		const glm::mat4 vp    = m_Camera.GetViewProjection();
		const glm::mat4 invVP = glm::inverse(vp);

		const CameraUniforms uniforms
		{
			.ViewProjection        = vp,
			.InverseViewProjection = invVP,
			.Position              = m_Camera.GetPosition(),
		};

		m_CameraBuffers[frameIndex].SetData(&uniforms, sizeof(CameraUniforms));
	}

	// ==== Barriers: undefined → attachments ====

	SetImageLayout(cmd.GetHandle(),
		swap.GetCurrentImage(),
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	SetImageLayout(cmd.GetHandle(),
		m_DepthImage.GetHandle(),
		VK_IMAGE_ASPECT_DEPTH_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	// ==== Geometry pass ====

	const AttachmentInfo color
	{
		.ImageView  = swap.GetCurrentImageView(),
		.Format     = swap.GetColorFormat(),
		.LoadOp     = LoadOp::Clear,
		.StoreOp    = StoreOp::Store,
		.ClearValue = { .color = { .float32 = { 0.03f, 0.02f, 0.08f, 1.0f } } },
		.Layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};

	const AttachmentInfo depth
	{
		.ImageView  = m_DepthImage.GetAttachmentView(),
		.Format     = Format::D32_Float,
		.LoadOp     = LoadOp::Clear,
		.StoreOp    = StoreOp::DontCare,
		.ClearValue = { .depthStencil = { 1.0f, 0 } },
		.Layout     = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
	};

	const RenderPassInfo passInfo
	{
		.ColorAttachments = { &color, 1 },
		.DepthAttachment  = &depth,
		.RenderArea       = { {0, 0}, swap.GetExtent() },
	};

	DynamicRendering::BeginRendering(cmd, passInfo);

	m_GeometryShader->Bind(cmd);
	m_GeometryState.Apply(cmd, 1);

	const VkDescriptorSet descriptorSet = Descriptor::GetSet();
	vkCmdBindDescriptorSets(cmd.GetHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_GeometryShader->GetPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

	DrawMesh(cmd, m_Mesh);

	DynamicRendering::EndRendering(cmd);

	// ==== Barrier: color attachment → present ====

	SetImageLayout(cmd.GetHandle(),
		swap.GetCurrentImage(),
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

void EditorApplication::OnShutdown()
{
	Renderer::WaitForGPU();

	for (UniformBuffer& buffer : m_CameraBuffers)
		buffer.Destroy();

	MaterialSystem::Clear();
	MaterialSystem::Shutdown();

	m_DepthImage.Destroy();
	m_Mesh.Destroy();
	m_GeometryShader->Shutdown();

	Descriptor::Shutdown();
}
