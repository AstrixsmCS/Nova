#pragma once

#include "Material.hpp"
#include "Buffer.hpp"

#include "Vulkan.hpp"

class MaterialSystem
{
public:
	static void Initialize();
	static void Shutdown();

	static uint32_t RegisterMaterial(const std::shared_ptr<Material>& material);
	static void UnregisterMaterial(uint32_t index);

	static void Update();

	static StorageBuffer& GetBuffer() { return s_Buffer; }
private:
	static constexpr uint32_t MAX_MATERIALS = 16384;
	static constexpr uint32_t MATERIAL_SIZE = sizeof(GPUMaterialData);

	struct MaterialSlot
	{
		std::shared_ptr<Material> material;
	};

	static StorageBuffer s_Buffer;

	static std::vector<MaterialSlot> s_Slots;
	static std::vector<uint32_t> s_FreeIndices;
};
