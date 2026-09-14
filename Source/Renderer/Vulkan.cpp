#include "Vulkan.hpp"

VkPipelineBindPoint ToVulkan(PipelineBindPoint bindPoint)
{
	switch (bindPoint)
	{
		case PipelineBindPoint::Graphics: return VK_PIPELINE_BIND_POINT_GRAPHICS;
		case PipelineBindPoint::Compute:  return VK_PIPELINE_BIND_POINT_COMPUTE;
		default:                          return VK_PIPELINE_BIND_POINT_GRAPHICS;
	}
}

VkAttachmentLoadOp ToVulkan(LoadOp op)
{
	switch (op)
	{
		case LoadOp::Load:     return VK_ATTACHMENT_LOAD_OP_LOAD;
		case LoadOp::Clear:    return VK_ATTACHMENT_LOAD_OP_CLEAR;
		case LoadOp::DontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		case LoadOp::None:     return VK_ATTACHMENT_LOAD_OP_NONE;
		case LoadOp::Invalid:  break;
	}

	return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

VkAttachmentStoreOp ToVulkan(StoreOp op)
{
	switch (op)
	{
		case StoreOp::Store:    return VK_ATTACHMENT_STORE_OP_STORE;
		case StoreOp::DontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
		case StoreOp::None:     return VK_ATTACHMENT_STORE_OP_NONE;
	}

	return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

VkFormat ToVulkan(Format format)
{
	switch (format)
	{
		case Format::Invalid: return VK_FORMAT_UNDEFINED;

		// 8-bit
		case Format::R8_UNorm: return VK_FORMAT_R8_UNORM;
		case Format::R8_SNorm: return VK_FORMAT_R8_SNORM;
		case Format::R8_UInt:  return VK_FORMAT_R8_UINT;
		case Format::R8_SInt:  return VK_FORMAT_R8_SINT;

		case Format::RG8_UNorm: return VK_FORMAT_R8G8_UNORM;
		case Format::RG8_SNorm: return VK_FORMAT_R8G8_SNORM;
		case Format::RG8_UInt:  return VK_FORMAT_R8G8_UINT;
		case Format::RG8_SInt:  return VK_FORMAT_R8G8_SINT;

		case Format::RGBA8_UNorm: return VK_FORMAT_R8G8B8A8_UNORM;
		case Format::RGBA8_SNorm: return VK_FORMAT_R8G8B8A8_SNORM;
		case Format::RGBA8_UInt:  return VK_FORMAT_R8G8B8A8_UINT;
		case Format::RGBA8_SInt:  return VK_FORMAT_R8G8B8A8_SINT;
		case Format::RGBA8_SRGB:  return VK_FORMAT_R8G8B8A8_SRGB;

		case Format::BGRA8_UNorm: return VK_FORMAT_B8G8R8A8_UNORM;
		case Format::BGRA8_SRGB:  return VK_FORMAT_B8G8R8A8_SRGB;

		// 16-bit
		case Format::R16_UNorm: return VK_FORMAT_R16_UNORM;
		case Format::R16_SNorm: return VK_FORMAT_R16_SNORM;
		case Format::R16_UInt:  return VK_FORMAT_R16_UINT;
		case Format::R16_SInt:  return VK_FORMAT_R16_SINT;
		case Format::R16_Float: return VK_FORMAT_R16_SFLOAT;

		case Format::RG16_UNorm: return VK_FORMAT_R16G16_UNORM;
		case Format::RG16_SNorm: return VK_FORMAT_R16G16_SNORM;
		case Format::RG16_UInt:  return VK_FORMAT_R16G16_UINT;
		case Format::RG16_SInt:  return VK_FORMAT_R16G16_SINT;
		case Format::RG16_Float: return VK_FORMAT_R16G16_SFLOAT;

		case Format::RGBA16_UNorm: return VK_FORMAT_R16G16B16A16_UNORM;
		case Format::RGBA16_SNorm: return VK_FORMAT_R16G16B16A16_SNORM;
		case Format::RGBA16_UInt:  return VK_FORMAT_R16G16B16A16_UINT;
		case Format::RGBA16_SInt:  return VK_FORMAT_R16G16B16A16_SINT;
		case Format::RGBA16_Float: return VK_FORMAT_R16G16B16A16_SFLOAT;

		// 32-bit
		case Format::R32_UInt:  return VK_FORMAT_R32_UINT;
		case Format::R32_SInt:  return VK_FORMAT_R32_SINT;
		case Format::R32_Float: return VK_FORMAT_R32_SFLOAT;

		case Format::RG32_UInt:  return VK_FORMAT_R32G32_UINT;
		case Format::RG32_SInt:  return VK_FORMAT_R32G32_SINT;
		case Format::RG32_Float: return VK_FORMAT_R32G32_SFLOAT;

		case Format::RGB32_UInt:  return VK_FORMAT_R32G32B32_UINT;
		case Format::RGB32_SInt:  return VK_FORMAT_R32G32B32_SINT;
		case Format::RGB32_Float: return VK_FORMAT_R32G32B32_SFLOAT;

		case Format::RGBA32_UInt:  return VK_FORMAT_R32G32B32A32_UINT;
		case Format::RGBA32_SInt:  return VK_FORMAT_R32G32B32A32_SINT;
		case Format::RGBA32_Float: return VK_FORMAT_R32G32B32A32_SFLOAT;

		// Packed
		case Format::RGB10A2_UNorm:    return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
		case Format::BGR10A2_UNorm:    return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
		case Format::R11G11B10_UFloat: return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
		case Format::RGB9E5_UFloat:    return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;

		// Depth / Stencil
		case Format::D16_UNorm:          return VK_FORMAT_D16_UNORM;
		case Format::D24_UNorm_S8_UInt:  return VK_FORMAT_D24_UNORM_S8_UINT;
		case Format::D32_Float:          return VK_FORMAT_D32_SFLOAT;
		case Format::D32_Float_S8_UInt:  return VK_FORMAT_D32_SFLOAT_S8_UINT;
		case Format::S8_UInt:            return VK_FORMAT_S8_UINT;

		// BC
		case Format::BC1_RGB_UNorm:  return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
		case Format::BC1_RGB_SRGB:   return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
		case Format::BC1_RGBA_UNorm: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
		case Format::BC1_RGBA_SRGB:  return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;

		case Format::BC2_RGBA_UNorm: return VK_FORMAT_BC2_UNORM_BLOCK;
		case Format::BC2_RGBA_SRGB:  return VK_FORMAT_BC2_SRGB_BLOCK;

		case Format::BC3_RGBA_UNorm: return VK_FORMAT_BC3_UNORM_BLOCK;
		case Format::BC3_RGBA_SRGB:  return VK_FORMAT_BC3_SRGB_BLOCK;

		case Format::BC4_R_UNorm: return VK_FORMAT_BC4_UNORM_BLOCK;
		case Format::BC4_R_SNorm: return VK_FORMAT_BC4_SNORM_BLOCK;

		case Format::BC5_RG_UNorm: return VK_FORMAT_BC5_UNORM_BLOCK;
		case Format::BC5_RG_SNorm: return VK_FORMAT_BC5_SNORM_BLOCK;

		case Format::BC6H_RGB_UFloat: return VK_FORMAT_BC6H_UFLOAT_BLOCK;
		case Format::BC6H_RGB_SFloat: return VK_FORMAT_BC6H_SFLOAT_BLOCK;

		case Format::BC7_RGBA_UNorm: return VK_FORMAT_BC7_UNORM_BLOCK;
		case Format::BC7_RGBA_SRGB:  return VK_FORMAT_BC7_SRGB_BLOCK;

		// ETC2
		case Format::ETC2_RGB8_UNorm:  return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
		case Format::ETC2_RGB8_SRGB:   return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
		case Format::ETC2_RGBA8_UNorm: return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
		case Format::ETC2_RGBA8_SRGB:  return VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;

		// YUV / Video
		case Format::YUV_NV12: return VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
		case Format::YUV_420P: return VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM;
	}

	return VK_FORMAT_UNDEFINED;
}

VkPolygonMode ToVulkan(PolygonMode mode)
{
	switch (mode)
	{
		case PolygonMode::Fill:  return VK_POLYGON_MODE_FILL;
		case PolygonMode::Line:  return VK_POLYGON_MODE_LINE;
		case PolygonMode::Point: return VK_POLYGON_MODE_POINT;
	}

	return VK_POLYGON_MODE_FILL;
}

VkCompareOp ToVulkan(CompareOp op)
{
	switch (op)
	{
		case CompareOp::Never:        return VK_COMPARE_OP_NEVER;
		case CompareOp::Less:         return VK_COMPARE_OP_LESS;
		case CompareOp::Equal:        return VK_COMPARE_OP_EQUAL;
		case CompareOp::LessEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
		case CompareOp::Greater:      return VK_COMPARE_OP_GREATER;
		case CompareOp::NotEqual:     return VK_COMPARE_OP_NOT_EQUAL;
		case CompareOp::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
		case CompareOp::Always:       return VK_COMPARE_OP_ALWAYS;
	}

	return VK_COMPARE_OP_ALWAYS;
}

VkStencilOp ToVulkan(StencilOp op)
{
	switch (op)
	{
		case StencilOp::Keep:           return VK_STENCIL_OP_KEEP;
		case StencilOp::Zero:           return VK_STENCIL_OP_ZERO;
		case StencilOp::Replace:        return VK_STENCIL_OP_REPLACE;
		case StencilOp::IncrementClamp: return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
		case StencilOp::DecrementClamp: return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
		case StencilOp::Invert:         return VK_STENCIL_OP_INVERT;
		case StencilOp::IncrementWrap:  return VK_STENCIL_OP_INCREMENT_AND_WRAP;
		case StencilOp::DecrementWrap:  return VK_STENCIL_OP_DECREMENT_AND_WRAP;
	}

	return VK_STENCIL_OP_KEEP;
}

VkBlendOp ToVulkan(BlendOp op)
{
	switch (op)
	{
		case BlendOp::Add:             return VK_BLEND_OP_ADD;
		case BlendOp::Subtract:        return VK_BLEND_OP_SUBTRACT;
		case BlendOp::ReverseSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
		case BlendOp::Min:             return VK_BLEND_OP_MIN;
		case BlendOp::Max:             return VK_BLEND_OP_MAX;
	}

	return VK_BLEND_OP_ADD;
}

VkBlendFactor ToVulkan(BlendFactor factor)
{
	switch (factor)
	{
		case BlendFactor::Zero:                return VK_BLEND_FACTOR_ZERO;
		case BlendFactor::One:                 return VK_BLEND_FACTOR_ONE;
		case BlendFactor::SrcColor:            return VK_BLEND_FACTOR_SRC_COLOR;
		case BlendFactor::OneMinusSrcColor:    return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		case BlendFactor::SrcAlpha:            return VK_BLEND_FACTOR_SRC_ALPHA;
		case BlendFactor::OneMinusSrcAlpha:    return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		case BlendFactor::DstColor:            return VK_BLEND_FACTOR_DST_COLOR;
		case BlendFactor::OneMinusDstColor:    return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
		case BlendFactor::DstAlpha:            return VK_BLEND_FACTOR_DST_ALPHA;
		case BlendFactor::OneMinusDstAlpha:    return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		case BlendFactor::SrcAlphaSaturated:   return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
		case BlendFactor::BlendColor:          return VK_BLEND_FACTOR_CONSTANT_COLOR;
		case BlendFactor::OneMinusBlendColor:  return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
		case BlendFactor::BlendAlpha:          return VK_BLEND_FACTOR_CONSTANT_ALPHA;
		case BlendFactor::OneMinusBlendAlpha:  return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
		case BlendFactor::Src1Color:           return VK_BLEND_FACTOR_SRC1_COLOR;
		case BlendFactor::OneMinusSrc1Color:   return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
		case BlendFactor::Src1Alpha:           return VK_BLEND_FACTOR_SRC1_ALPHA;
		case BlendFactor::OneMinusSrc1Alpha:   return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
	}

	return VK_BLEND_FACTOR_ONE;
}

VkCullModeFlags ToVulkan(CullMode mode)
{
	switch (mode)
	{
		case CullMode::None:  return VK_CULL_MODE_NONE;
		case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
		case CullMode::Back:  return VK_CULL_MODE_BACK_BIT;
	}

	return VK_CULL_MODE_NONE;
}

VkFrontFace ToVulkan(WindingMode mode)
{
	switch (mode)
	{
		case WindingMode::CCW: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
		case WindingMode::CW:  return VK_FRONT_FACE_CLOCKWISE;
	}

	return VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

VkImageType ToVulkan(TextureType type)
{
	switch (type)
	{
		case TextureType::Texture2D: return VK_IMAGE_TYPE_2D;
		case TextureType::Texture3D: return VK_IMAGE_TYPE_3D;
		case TextureType::Cube:      return VK_IMAGE_TYPE_2D;
	}

	return VK_IMAGE_TYPE_2D;
}

VkImageViewType ToVulkanImageViewType(TextureType type)
{
	switch (type)
	{
		case TextureType::Texture2D: return VK_IMAGE_VIEW_TYPE_2D;
		case TextureType::Texture3D: return VK_IMAGE_VIEW_TYPE_3D;
		case TextureType::Cube:      return VK_IMAGE_VIEW_TYPE_CUBE;
	}

	return VK_IMAGE_VIEW_TYPE_2D;
}

VkFilter ToVulkan(SamplerFilter filter)
{
	switch (filter)
	{
		case SamplerFilter::Nearest: return VK_FILTER_NEAREST;
		case SamplerFilter::Linear:  return VK_FILTER_LINEAR;
		case SamplerFilter::Cubic:   return VK_FILTER_CUBIC_EXT;
	}

	return VK_FILTER_LINEAR;
}

VkSamplerMipmapMode ToVulkan(SamplerMip mip)
{
	switch (mip)
	{
		case SamplerMip::Disabled:
		case SamplerMip::Nearest: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
		case SamplerMip::Linear: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
	}

	return VK_SAMPLER_MIPMAP_MODE_NEAREST;
}

VkSamplerAddressMode ToVulkan(SamplerWrap wrap)
{
	switch (wrap)
	{
		case SamplerWrap::Repeat:            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		case SamplerWrap::ClampToEdge:       return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case SamplerWrap::ClampToBorder:     return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		case SamplerWrap::MirrorRepeat:      return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		case SamplerWrap::MirrorClampToEdge: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
	}

	return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

VkPrimitiveTopology ToVulkan(Topology topology)
{
	switch (topology)
	{
		case Topology::Point:         return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		case Topology::Line:          return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		case Topology::LineStrip:     return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
		case Topology::Triangle:      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		case Topology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		case Topology::Patch:         return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
	}

	return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

VkColorSpaceKHR ToVulkan(ColorSpace colorSpace)
{
	switch (colorSpace)
	{
		case ColorSpace::SRGBNonlinear: return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		case ColorSpace::SRGBExtendedLinear: return VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
		case ColorSpace::HDR10: return VK_COLOR_SPACE_HDR10_ST2084_EXT;
		case ColorSpace::BT709Linear: return VK_COLOR_SPACE_BT709_LINEAR_EXT;
	}

	return VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
}
