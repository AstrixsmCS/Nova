#include "MaterialSystem.hpp"

#include <cassert>

StorageBuffer MaterialSystem::s_Buffer;

std::vector<MaterialSystem::MaterialSlot> MaterialSystem::s_Slots;
std::vector<uint32_t> MaterialSystem::s_FreeIndices;

void MaterialSystem::Initialize()
{
	s_Slots.resize(MAX_MATERIALS);

	s_FreeIndices.reserve(MAX_MATERIALS);

	for (uint32_t i = MAX_MATERIALS; i > 0; --i)
	{
		s_FreeIndices.push_back(i - 1);
	}

	const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(MAX_MATERIALS) * MATERIAL_SIZE;

	s_Buffer.Create(bufferSize);
}

void MaterialSystem::Shutdown()
{
	s_Slots.clear();
	s_FreeIndices.clear();

	s_Buffer.Destroy();
}

uint32_t MaterialSystem::RegisterMaterial(const std::shared_ptr<Material>& material)
{
	assert(material);

	if (s_FreeIndices.empty())
	{
		assert(false && "MaterialSystem: Out of material slots!");
		return UINT32_MAX;
	}

	const uint32_t index = s_FreeIndices.back();

	s_FreeIndices.pop_back();

	s_Slots[index].material = material;

	material->MarkDirty();

	return index;
}

void MaterialSystem::UnregisterMaterial(uint32_t index)
{
	if (index >= MAX_MATERIALS)
		return;

	MaterialSlot& slot = s_Slots[index];

	if (!slot.material)
		return;

	slot.material.reset();

	const GPUMaterialData emptyMaterial{};

	const VkDeviceSize offset = static_cast<VkDeviceSize>(index) * MATERIAL_SIZE;

	s_Buffer.SetData(&emptyMaterial, sizeof(GPUMaterialData), offset);

	s_FreeIndices.push_back(index);
}

void MaterialSystem::Update()
{
	for (uint32_t index = 0; index < MAX_MATERIALS; ++index)
	{
		MaterialSlot& slot = s_Slots[index];

		if (!slot.material)
			continue;

		if (!slot.material->IsGpuDirty())
			continue;

		slot.material->UpdateGPUData();

		const GPUMaterialData& data = slot.material->GetGPUData();

		const VkDeviceSize offset = static_cast<VkDeviceSize>(index) * MATERIAL_SIZE;

		s_Buffer.SetData(&data, sizeof(GPUMaterialData), offset);

		slot.material->ClearGpuDirty();
	}
}
