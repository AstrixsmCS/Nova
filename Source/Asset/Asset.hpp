#pragma once

#include "Core/UUID.hpp"

// Asset types planned for Nova.
//
// A type appearing here does not mean its importer, serializer, or runtime
// representation has been implemented yet.
enum class AssetType : uint8_t
{
	None = 0,
	Script
};

enum class AssetFlag : uint8_t { None = 0, Missing = 1 << 0, Invalid = 1 << 1 };

using AssetHandle = UUID;

class Asset
{
public:
	virtual ~Asset() = default;

	AssetHandle Handle { 0 };
	uint16_t Flags = static_cast<uint16_t>(AssetFlag::None);

	static AssetType GetStaticType() { return AssetType::None; }
	virtual AssetType GetAssetType() const { return AssetType::None; }

	virtual bool operator==(const Asset& other) const { return Handle == other.Handle; }
	virtual bool operator!=(const Asset& other) const { return !(*this == other); }
private:
	// If you want to find out whether assets are valid or missing, use AssetManager::IsAssetValid(handle), IsAssetMissing(handle)
	// This cleans up and removes inconsistencies from rest of the code.
	// You simply go AssetManager::GetAsset<Whatever>(handle), and so long as you get a non-null pointer back, you're good to go.
	// No IsValid(), IsFlagSet(AssetFlag::Missing) etc. etc. all throughout the code.
	bool IsValid() const { return ((Flags & (uint16_t)AssetFlag::Missing) | (Flags & (uint16_t)AssetFlag::Invalid)) == 0; }

	bool IsFlagSet(AssetFlag flag) const { return (uint16_t)flag & Flags; }
	void SetFlag(AssetFlag flag, bool value = true)
	{
		if (value)
			Flags |= (uint16_t)flag;
		else
			Flags &= ~(uint16_t)flag;
	}

};
