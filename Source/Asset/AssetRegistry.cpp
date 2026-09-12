#include "AssetRegistry.hpp"

#include "Core/Log.hpp"

const AssetMetadata& AssetRegistry::Get(AssetHandle handle) const
{
	const auto it = m_Assets.find(handle);
	if (it == m_Assets.end())
	{
		NV_WARN("AssetRegistry::Get called with unknown handle");
		return s_NullMetadata;
	}
	return it->second;
}

void AssetRegistry::Set(AssetHandle handle, const AssetMetadata& metadata)
{
	if (static_cast<uint64_t>(handle) == 0)
	{
		NV_ERROR("Cannot register an asset with an invalid handle");
		return;
	}

	m_Assets[handle] = metadata;
}

bool AssetRegistry::Contains(AssetHandle handle) const
{
	return m_Assets.contains(handle);
}

size_t AssetRegistry::Remove(AssetHandle handle)
{
	return m_Assets.erase(handle);
}

void AssetRegistry::Clear()
{
	m_Assets.clear();
}
