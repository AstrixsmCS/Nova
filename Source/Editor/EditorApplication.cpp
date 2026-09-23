#include "EditorApplication.hpp"

#include "Renderer/Vulkan/Descriptors.hpp"

#include <glm/glm.hpp>

#include <array>
#include <cassert>

static const float CAMERA_FOV  = glm::radians(60.0f);
static const float CAMERA_NEAR = 0.1f;
static const float CAMERA_FAR  = 1000.0f;

static constexpr VkImageAspectFlags    DEPTH_STENCIL_ASPECTS = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
static constexpr VkPipelineStageFlags2 DEPTH_STAGES          = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;

EditorApplication::EditorApplication(const ApplicationSpecification& specification)
	: Application(specification)
{
}

EditorApplication::~EditorApplication() = default;

void EditorApplication::OnInitialize()
{
	MaterialSystem::Initialize();

	CreateRendererResources();

	// ==== Scene ====
	const bool loaded = m_Mesh.Load("Assets/Meshes/FlightHelmet/FlightHelmet.gltf");
	assert(loaded);

	for (const std::shared_ptr<Material>& material : m_Mesh.GetMaterials())
		MaterialSystem::PrepareMaterial(material);

	const VkExtent2D extent      = Renderer::GetSwapChain().GetExtent();
	const float      aspectRatio = static_cast<float>(extent.width) / static_cast<float>(extent.height);

	m_Camera = Camera(CAMERA_FOV, aspectRatio, CAMERA_NEAR, CAMERA_FAR);
	m_Camera.SetPosition({ 0.0f, 0.0f, 3.0f });

	CreateRenderTargets(extent.width, extent.height);
}

void EditorApplication::OnShutdown()
{
	Renderer::WaitForGPU();

	MaterialSystem::Shutdown();
	m_Mesh.Destroy();

	DestroyRenderTargets();
	DestroyRendererResources();
}

void EditorApplication::CreateRendererResources()
{
	// ==== Geometry pass ====
	m_GBufferShader = std::make_shared<Shader>();
	m_GBufferShader->Load("Assets/Shaders/Mesh.slang");
	assert(m_GBufferShader->IsValid());

	m_GBufferMaterial.SetShader(m_GBufferShader);

	m_GBufferState.VertexLayout =
	{
		{ ShaderDataType::Float3, "Position" },
		{ ShaderDataType::Float3, "Normal"   },
		{ ShaderDataType::Float2, "TexCoord" },
		{ ShaderDataType::Float4, "Tangent"  },
	};
	m_GBufferState.DepthTest  = true;
	m_GBufferState.DepthWrite = true;
	m_GBufferState.CullMode   = CullMode::Back;

	// ==== Lighting pass ====
	m_LightingShader = std::make_shared<Shader>();
	m_LightingShader->Load("Assets/Shaders/Lighting.slang");
	assert(m_LightingShader->IsValid());

	m_LightingMaterial.SetShader(m_LightingShader);

	// ==== Composite pass ====
	m_CompositeShader = std::make_shared<Shader>();
	m_CompositeShader->Load("Assets/Shaders/Composite.slang");
	assert(m_CompositeShader->IsValid());

	m_CompositeMaterial.SetShader(m_CompositeShader);

	m_CompositeState.VertexLayout = {};
	m_CompositeState.DepthTest    = false;
	m_CompositeState.DepthWrite   = false;
	m_CompositeState.CullMode     = CullMode::None;

	// ==== Per-frame camera data ====
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
}

void EditorApplication::DestroyRendererResources()
{
	for (Buffer& buffer : m_CameraBuffers)
		buffer.Destroy();

	m_CompositeMaterial.SetShader(nullptr);
	m_LightingMaterial.SetShader(nullptr);
	m_GBufferMaterial.SetShader(nullptr);

	m_CompositeShader->Shutdown();
	m_LightingShader->Shutdown();
	m_GBufferShader->Shutdown();

	m_CompositeShader.reset();
	m_LightingShader.reset();
	m_GBufferShader.reset();
}

void EditorApplication::CreateRenderTargets(uint32_t width, uint32_t height)
{
	DestroyRenderTargets();

	const Dimensions            size         = { width, height, 1 };
	constexpr TextureUsageFlags gbufferUsage = TextureUsageBits_Attachment | TextureUsageBits_Sampled;

	m_GBuffer.Albedo.Create(      { .Format = Format::RGBA8_SRGB,        .Size = size, .Usage = gbufferUsage, .DebugName = "Albedo/AO"          });
	m_GBuffer.Normal.Create(      { .Format = Format::RG16_UNorm,        .Size = size, .Usage = gbufferUsage, .DebugName = "Normal"             });
	m_GBuffer.Material.Create(    { .Format = Format::RGBA8_UNorm,       .Size = size, .Usage = gbufferUsage, .DebugName = "Roughness/Metallic" });
	m_GBuffer.Emissive.Create(    { .Format = Format::RGBA16_Float,      .Size = size, .Usage = gbufferUsage, .DebugName = "Emissive"     });
	m_GBuffer.DepthStencil.Create({ .Format = Format::D32_Float_S8_UInt, .Size = size, .Usage = gbufferUsage, .DebugName = "Depth/Stencil"      });

	m_HDRColor.Create(
	{
		.Format    = Format::RGBA16_Float,
		.Size      = size,
		.Usage     = TextureUsageBits_Storage | TextureUsageBits_Sampled,
		.DebugName = "HDR Color"
	});
}

void EditorApplication::DestroyRenderTargets()
{
	m_HDRColor.Destroy();

	m_GBuffer.DepthStencil.Destroy();
	m_GBuffer.Emissive.Destroy();
	m_GBuffer.Material.Destroy();
	m_GBuffer.Normal.Destroy();
	m_GBuffer.Albedo.Destroy();
}

// ==== Frame ====

void EditorApplication::OnUpdate(Timestep ts)
{
	const VkExtent2D extent = Renderer::GetSwapChain().GetExtent();

	if (m_GBuffer.DepthStencil.GetWidth() != extent.width || m_GBuffer.DepthStencil.GetHeight() != extent.height)
	{
		Renderer::WaitForGPU();
		CreateRenderTargets(extent.width, extent.height);

		const float aspectRatio = static_cast<float>(extent.width) / static_cast<float>(extent.height);
		m_Camera.SetPerspective(CAMERA_FOV, aspectRatio, CAMERA_NEAR, CAMERA_FAR);
	}

	// ==== Per-frame GPU data ====
	UpdateCamera(ts);
	MaterialSystem::UploadPendingMaterials(Renderer::GetFrameSlot());

	// ==== Passes ====
	CommandBuffer& commandBuffer = Renderer::AcquireCommandBuffer();

	GeometryPass (commandBuffer, extent);
	LightingPass (commandBuffer, extent);
	CompositePass(commandBuffer, extent);
}

void EditorApplication::UpdateCamera(Timestep ts)
{
	m_Camera.OnUpdate(ts);

	const glm::mat4 viewProjection = m_Camera.GetViewProjection();

	const CameraUniforms cameraData
	{
		.ViewProjection        = viewProjection,
		.InverseViewProjection = glm::inverse(viewProjection),
		.Position              = m_Camera.GetPosition()
	};

	m_CameraBuffers[Renderer::GetFrameSlot()].SetData(&cameraData, sizeof(CameraUniforms));
}

// ==== Geometry pass: mesh -> G-buffer ====

void EditorApplication::GeometryPass(CommandBuffer& commandBuffer, VkExtent2D extent)
{
	for (const Texture* texture : { &m_GBuffer.Albedo, &m_GBuffer.Normal, &m_GBuffer.Material, &m_GBuffer.Emissive })
	{
		commandBuffer.ImageBarrier(texture->GetHandle(), VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
	}

	commandBuffer.ImageBarrier(m_GBuffer.DepthStencil.GetHandle(), DEPTH_STENCIL_ASPECTS,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, DEPTH_STAGES);

	auto colorAttachment = [](Texture& texture) -> RenderingAttachmentInfo
	{
		return
		{
			.ImageView  = texture.GetAttachmentView(),
			.LoadOp     = LoadOp::Clear,
			.StoreOp    = StoreOp::Store,
			.ClearValue = { .Color = { .Float32 = { 0.0f, 0.0f, 0.0f, 0.0f } } },
			.Layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		};
	};

	const std::array<RenderingAttachmentInfo, 4> colors
	{
		colorAttachment(m_GBuffer.Albedo),
		colorAttachment(m_GBuffer.Normal),
		colorAttachment(m_GBuffer.Material),
		colorAttachment(m_GBuffer.Emissive),
	};

	const RenderingAttachmentInfo depth
	{
		.ImageView  = m_GBuffer.DepthStencil.GetAttachmentView(),
		.LoadOp     = LoadOp::Clear,
		.StoreOp    = StoreOp::Store,
		.ClearValue = { .DepthStencil = { 1.0f, 0 } },
		.Layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
	};

	const RenderingInfo passInfo
	{
		.ColorAttachments = { colors.data(), colors.size() },
		.DepthAttachment  = &depth,
		.RenderArea       = { .X = 0, .Y = 0, .Width = extent.width, .Height = extent.height },
	};

	commandBuffer.BeginRendering(passInfo);
	{
		DebugLabelScope label(commandBuffer, "Geometry Pass", 0xAEC6CFFF);

		m_GBufferShader->Bind(commandBuffer);
		commandBuffer.SetGraphicsState(m_GBufferState, 4);

		const VkDescriptorSet descriptorSet = Descriptor::GetSet();
		vkCmdBindDescriptorSets(commandBuffer.GetHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_GBufferShader->GetPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

		DrawMesh(commandBuffer, m_Mesh);
	}
	commandBuffer.EndRendering();
}

void EditorApplication::DrawMesh(CommandBuffer& commandBuffer, const Mesh& mesh)
{
	const uint32_t           frameSlot = Renderer::GetFrameSlot();
	const PushConstantRange& range     = m_GBufferShader->GetPushConstantRanges()[0];

	commandBuffer.BindVertexBuffer(mesh.GetVertexBuffer());
	commandBuffer.BindIndexBuffer(mesh.GetIndexBuffer(), IndexFormat::UInt32);

	m_GBufferMaterial.Set("UBCamera",    m_CameraBuffers[frameSlot].GetDeviceAddress());
	m_GBufferMaterial.Set("SBMaterials", MaterialSystem::GetBuffer(frameSlot).GetDeviceAddress());

	const std::vector<std::shared_ptr<Material>>& materials = mesh.GetMaterials();

	for (const Submesh& submesh : mesh.GetSubmeshes())
	{
		const bool     hasMaterial   = submesh.MaterialIndex < materials.size();
		const uint32_t materialIndex = hasMaterial ? MaterialSystem::PrepareMaterial(materials[submesh.MaterialIndex]) : MaterialSystem::FallbackIndex;

		m_GBufferMaterial.Set("Model",         submesh.Transform);
		m_GBufferMaterial.Set("MaterialIndex", materialIndex);

		const std::vector<uint8_t>& storage = m_GBufferMaterial.GetUniformStorage();
		commandBuffer.PushConstants(m_GBufferShader->GetPipelineLayout(), range.StageFlags, storage.data() + range.Offset, range.Size, range.Offset);

		commandBuffer.DrawIndexed(submesh.IndexCount, 1, submesh.BaseIndex, static_cast<int32_t>(submesh.BaseVertex));
	}
}

// ==== Lighting pass: G-buffer -> HDR color ====

void EditorApplication::LightingPass(CommandBuffer& commandBuffer, VkExtent2D extent)
{
	for (const Texture* texture : { &m_GBuffer.Albedo, &m_GBuffer.Normal, &m_GBuffer.Material, &m_GBuffer.Emissive })
	{
		commandBuffer.ImageBarrier(texture->GetHandle(), VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
	}

	commandBuffer.ImageBarrier(m_GBuffer.DepthStencil.GetHandle(), DEPTH_STENCIL_ASPECTS,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		DEPTH_STAGES, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

	commandBuffer.ImageBarrier(m_HDRColor.GetHandle(), VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

	DebugLabelScope label(commandBuffer, "Lighting Pass", 0xFFD27FFF);

	m_LightingShader->Bind(commandBuffer);

	const VkDescriptorSet descriptorSet = Descriptor::GetSet();
	vkCmdBindDescriptorSets(commandBuffer.GetHandle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_LightingShader->GetPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

	m_LightingMaterial.Set("UBCamera",       m_CameraBuffers[Renderer::GetFrameSlot()].GetDeviceAddress());
	m_LightingMaterial.Set("AlbedoIndex",    m_GBuffer.Albedo.GetBindlessIndex());
	m_LightingMaterial.Set("NormalIndex",    m_GBuffer.Normal.GetBindlessIndex());
	m_LightingMaterial.Set("MaterialIndex",  m_GBuffer.Material.GetBindlessIndex());
	m_LightingMaterial.Set("EmissiveIndex",  m_GBuffer.Emissive.GetBindlessIndex());
	m_LightingMaterial.Set("DepthIndex",     m_GBuffer.DepthStencil.GetBindlessIndex());
	m_LightingMaterial.Set("OutputIndex",    m_HDRColor.GetStorageIndex());
	m_LightingMaterial.Set("Extent",         glm::uvec2(extent.width, extent.height));
	m_LightingMaterial.Set("LightDirection", glm::vec4(glm::normalize(m_DirectionalLight.Direction), 0.0f));
	m_LightingMaterial.Set("LightRadiance",  m_DirectionalLight.Radiance);
	m_LightingMaterial.Set("LightIntensity", m_DirectionalLight.Intensity);

	const PushConstantRange&    range   = m_LightingShader->GetPushConstantRanges()[0];
	const std::vector<uint8_t>& storage = m_LightingMaterial.GetUniformStorage();
	commandBuffer.PushConstants(m_LightingShader->GetPipelineLayout(), range.StageFlags, storage.data() + range.Offset, range.Size, range.Offset);

	commandBuffer.Dispatch((extent.width  + 8 - 1) / 8, (extent.height + 8 - 1) / 8);
}

// ==== Composite pass: HDR color -> swapchain ====

void EditorApplication::CompositePass(CommandBuffer& commandBuffer, VkExtent2D extent)
{
	SwapChain& swapChain = Renderer::GetSwapChain();

	commandBuffer.ImageBarrier(m_HDRColor.GetHandle(), VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

	commandBuffer.ImageBarrier(swapChain.GetCurrentImage(), VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

	const RenderingAttachmentInfo color
	{
		.ImageView = swapChain.GetCurrentImageView(),
		.LoadOp    = LoadOp::DontCare,
		.StoreOp   = StoreOp::Store,
		.Layout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};

	const RenderingInfo passInfo
	{
		.ColorAttachments = { &color, 1 },
		.RenderArea       = { .X = 0, .Y = 0, .Width = extent.width, .Height = extent.height },
	};

	commandBuffer.BeginRendering(passInfo);
	{
		DebugLabelScope label(commandBuffer, "Composite Pass", 0x9FE2BFFF);

		m_CompositeShader->Bind(commandBuffer);
		commandBuffer.SetGraphicsState(m_CompositeState, 1);

		const VkDescriptorSet descriptorSet = Descriptor::GetSet();
		vkCmdBindDescriptorSets(commandBuffer.GetHandle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_CompositeShader->GetPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

		m_CompositeMaterial.Set("InputIndex", m_HDRColor.GetBindlessIndex());

		const PushConstantRange&    range   = m_CompositeShader->GetPushConstantRanges()[0];
		const std::vector<uint8_t>& storage = m_CompositeMaterial.GetUniformStorage();
		commandBuffer.PushConstants(m_CompositeShader->GetPipelineLayout(), range.StageFlags, storage.data() + range.Offset, range.Size, range.Offset);

		commandBuffer.Draw(3);
	}
	commandBuffer.EndRendering();

	commandBuffer.ImageBarrier(swapChain.GetCurrentImage(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_NONE);
}
