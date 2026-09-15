#pragma once

#include "Vulkan.hpp"
#include "Allocator.hpp"

enum class ShaderDataType
{
	None = 0,
	Float, Float2, Float3, Float4,
	Mat3, Mat4,
	Int, Int2, Int3, Int4,
	UInt, UInt2, UInt3, UInt4,
	Bool
};

inline uint32_t ShaderDataTypeSize(ShaderDataType type)
{
	switch (type)
	{
		case ShaderDataType::Float:  return 4;
		case ShaderDataType::Float2: return 4 * 2;
		case ShaderDataType::Float3: return 4 * 3;
		case ShaderDataType::Float4: return 4 * 4;

		case ShaderDataType::Mat3:   return 4 * 3 * 3;
		case ShaderDataType::Mat4:   return 4 * 4 * 4;

		case ShaderDataType::Int:    return 4;
		case ShaderDataType::Int2:   return 4 * 2;
		case ShaderDataType::Int3:   return 4 * 3;
		case ShaderDataType::Int4:   return 4 * 4;

		case ShaderDataType::UInt:   return 4;
		case ShaderDataType::UInt2:  return 4 * 2;
		case ShaderDataType::UInt3:  return 4 * 3;
		case ShaderDataType::UInt4:  return 4 * 4;

		case ShaderDataType::Bool:   return 1;

		case ShaderDataType::None:
			break;
	}

	return 0;
}

struct VertexBufferElement
{
	std::string Name;

	ShaderDataType Type = ShaderDataType::None;

	uint32_t Size = 0;
	uint32_t Offset = 0;

	bool Normalized = false;

	VertexBufferElement() = default;

	VertexBufferElement(ShaderDataType type, const std::string& name, bool normalized = false)
		: Name(name), Type(type), Size(ShaderDataTypeSize(type)), Normalized(normalized)
	{
	}

	uint32_t GetComponentCount() const
	{
		switch (Type)
		{
			case ShaderDataType::Float:
			case ShaderDataType::Int:
			case ShaderDataType::UInt:
			case ShaderDataType::Bool:
				return 1;

			case ShaderDataType::Float2:
			case ShaderDataType::Int2:
			case ShaderDataType::UInt2:
				return 2;

			case ShaderDataType::Float3:
			case ShaderDataType::Int3:
			case ShaderDataType::UInt3:
				return 3;

			case ShaderDataType::Float4:
			case ShaderDataType::Int4:
			case ShaderDataType::UInt4:
				return 4;

			case ShaderDataType::Mat3:
				return 3 * 3;

			case ShaderDataType::Mat4:
				return 4 * 4;

			case ShaderDataType::None:
				break;
		}

		return 0;
	}
};

class VertexBufferLayout
{
public:
	VertexBufferLayout() = default;

	VertexBufferLayout(std::initializer_list<VertexBufferElement> elements) : m_Elements(elements)
	{
		CalculateOffsetsAndStride();
	}

	uint32_t GetStride() const { return m_Stride; }
	uint32_t GetElementCount() const { return static_cast<uint32_t>(m_Elements.size()); }
	const std::vector<VertexBufferElement>& GetElements() const { return m_Elements; }

	auto begin() { return m_Elements.begin(); }
	auto end()   { return m_Elements.end(); }

	auto begin() const { return m_Elements.begin(); }
	auto end()   const { return m_Elements.end(); }

private:
	void CalculateOffsetsAndStride()
	{
		uint32_t offset = 0;

		m_Stride = 0;

		for (VertexBufferElement& element : m_Elements)
		{
			element.Offset = offset;

			offset += element.Size;
			m_Stride += element.Size;
		}
	}

private:
	std::vector<VertexBufferElement> m_Elements;

	uint32_t m_Stride = 0;
};

enum class VertexBufferUsage
{
	None = 0,
	Static,
	Dynamic
};

class VertexBuffer
{
public:
	void Create(const void* data, uint64_t size, VertexBufferUsage usage = VertexBufferUsage::Static);
	void Create(uint64_t size, VertexBufferUsage usage = VertexBufferUsage::Dynamic);

	void Destroy();

	void SetData(const void* data, uint64_t size, uint64_t offset = 0);

	void SetLayout(const VertexBufferLayout& layout) { m_Layout = layout; }

	VkBuffer GetBuffer() const { return m_Buffer; }
	VmaAllocation GetAllocation() const { return m_Allocation; }
	uint64_t GetSize() const { return m_Size; }
	VertexBufferUsage GetUsage() const { return m_Usage; }
	const VertexBufferLayout& GetLayout() const { return m_Layout; }

private:
	VkBuffer m_Buffer = VK_NULL_HANDLE;
	VmaAllocation m_Allocation = VK_NULL_HANDLE;

	uint64_t m_Size = 0;

	VertexBufferUsage m_Usage = VertexBufferUsage::Static;

	VertexBufferLayout m_Layout;
};

class IndexBuffer
{
public:
	void Create(const void* data, uint64_t size);
	void Create(uint64_t size);

	void Destroy();

	void SetData(const void* data, uint64_t size, uint64_t offset = 0);

	uint32_t GetCount() const { return static_cast<uint32_t>(m_Size / sizeof(uint32_t)); }
	uint64_t GetSize() const { return m_Size; }

	VkBuffer GetBuffer() const { return m_Buffer; }
	VmaAllocation GetAllocation() const { return m_Allocation; }
private:
	uint64_t m_Size = 0;

	VkBuffer m_Buffer = VK_NULL_HANDLE;
	VmaAllocation m_Allocation = VK_NULL_HANDLE;
};

class UniformBuffer
{
public:
	void Create(uint32_t size);
	void Destroy();

	void SetData(const void* data, uint32_t size, uint32_t offset = 0);

	VkBuffer GetBuffer() const { return m_Buffer; }
	VkDeviceAddress GetDeviceAddress() const { return m_DeviceAddress; }

	uint64_t GetSize() const { return m_Size; }
private:
	VkBuffer m_Buffer = VK_NULL_HANDLE;
	VmaAllocation m_Allocation = VK_NULL_HANDLE;

	VkDeviceAddress m_DeviceAddress = 0;

	uint32_t m_Size = 0;
};

class StorageBuffer
{
public:
	void Create(VkDeviceSize size);
	void Destroy();

	void SetData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

	VkBuffer GetBuffer() const { return m_Buffer; }
	VkDeviceAddress GetDeviceAddress() const { return m_DeviceAddress; }

	VkDeviceSize GetSize() const { return m_Size; }

	bool IsValid() const { return m_Buffer != VK_NULL_HANDLE; }
private:
	VkBuffer m_Buffer = VK_NULL_HANDLE;
	VmaAllocation m_Allocation = VK_NULL_HANDLE;

	VkDeviceAddress m_DeviceAddress = 0;

	VkDeviceSize m_Size = 0;
};
