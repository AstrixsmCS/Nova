#include "EditorApplication.hpp"

#include "Renderer/Descriptors.hpp"
#include "Renderer/DynamicRendering.hpp"
#include "Renderer/Renderer.hpp"

#include "Core/Log.hpp"

#include <glm/glm.hpp>

#include <stack>

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
	m_GeometryState.CullMode     = VK_CULL_MODE_BACK_BIT;
	m_GeometryState.DepthTest    = true;
	m_GeometryState.DepthWrite   = true;
	m_GeometryState.DepthCompare = CompareOp::Less;

	m_Mesh.Load("Assets/Meshes/DamagedHelmet/DamagedHelmet.glb");

	m_MaterialIndices.reserve(m_Mesh.GetMaterials().size());
	for (const Material& material : m_Mesh.GetMaterials())
	{
		auto shared = std::shared_ptr<Material>(const_cast<Material*>(&material), [](Material*){});
		m_MaterialIndices.push_back(MaterialSystem::RegisterMaterial(shared));
	}

	for (UniformBuffer& buffer : m_CameraBuffers)
		buffer.Create(sizeof(CameraUniforms));

	const VkExtent2D extent = Renderer::GetSwapChain().GetExtent();
	const float      aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);

	m_Camera = Camera(glm::radians(60.0f), aspect, 0.1f, 1000.0f);
	m_Camera.SetPosition({ 0.0f, 0.0f, 3.0f });

	CreateDepthImage(extent.width, extent.height);

	// ==== Texture pipeline test ====

	m_PNGHandle  = AssetManager::RegisterAsset("Textures/Test.png");
	m_PNGTexture = AssetManager::GetAsset<Texture2D>(m_PNGHandle);
	assert(static_cast<uint64_t>(m_PNGHandle) != 0);
	assert(m_PNGTexture && m_PNGTexture->IsValid());
	NV_TRACE("PNG loaded: {}x{}, {} mips, bindless index {}", m_PNGTexture->GetWidth(), m_PNGTexture->GetHeight(), m_PNGTexture->GetMipCount(), m_PNGTexture->GetBindlessIndex());

	m_HDRHandle  = AssetManager::RegisterAsset("Textures/Test.hdr");
	m_HDRTexture = AssetManager::GetAsset<Texture2D>(m_HDRHandle);
	assert(static_cast<uint64_t>(m_HDRHandle) != 0);
	assert(m_HDRTexture && m_HDRTexture->IsValid());
	NV_TRACE("HDR loaded: {}x{}, {} mips, bindless index {}", m_HDRTexture->GetWidth(), m_HDRTexture->GetHeight(), m_HDRTexture->GetMipCount(), m_HDRTexture->GetBindlessIndex());
}

void EditorApplication::DrawMesh(CommandBuffer& commandBuffer, const Mesh& mesh)
{
	const VkBuffer     vb     = mesh.GetVertexBuffer();
	const VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(commandBuffer.GetHandle(), 0, 1, &vb, &offset);
	vkCmdBindIndexBuffer(commandBuffer.GetHandle(), mesh.GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

	const uint64_t cameraAddress    = m_CameraBuffers[Renderer::GetCurrentFrameIndex()].GetDeviceAddress();
	const uint64_t materialsAddress = MaterialSystem::GetBuffer().GetDeviceAddress();

	const auto& shader = m_GeometryMaterial.GetShader();
	const auto& ranges = shader->GetPushConstantRanges();
	assert(!ranges.empty());
	const auto& range = ranges[0];

	m_GeometryMaterial.Set("UBCamera",    cameraAddress);
	m_GeometryMaterial.Set("SBMaterials", materialsAddress);

	for (const Submesh& submesh : mesh.GetSubmeshes())
	{
		const uint32_t materialIndex = (submesh.MaterialIndex != UINT32_MAX && submesh.MaterialIndex < m_MaterialIndices.size()) ? m_MaterialIndices[submesh.MaterialIndex] : 0;

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

	MaterialSystem::Update();

	CommandBuffer& cmd  = Renderer::GetCurrentCommandBuffer();
	SwapChain&     swap = Renderer::GetSwapChain();

	{
		const VkExtent2D extent = swap.GetExtent();

		if (m_DepthImage.GetWidth()  != extent.width || m_DepthImage.GetHeight() != extent.height)
		{
			Renderer::WaitForGPU();
			CreateDepthImage(extent.width, extent.height);

			const float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
			m_Camera.SetPerspective(glm::radians(60.0f), aspect, 0.1f, 1000.0f);
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

		m_CameraBuffers[Renderer::GetCurrentFrameIndex()].SetData(&uniforms, sizeof(CameraUniforms));
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

	m_PNGTexture.reset();
	m_HDRTexture.reset();

	for (auto& buffer : m_CameraBuffers)
		buffer.Destroy();

	m_DepthImage.Destroy();
	m_Mesh.Destroy();
	m_GeometryShader->Shutdown();

	MaterialSystem::Shutdown();
	Descriptor::Shutdown();
}
