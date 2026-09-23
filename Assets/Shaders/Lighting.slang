import Common;
import BRDF;

struct UBCameraData
{
	float4x4 ViewProjection;
	float4x4 InverseViewProjection;

	float3 CameraPosition;
	float  _Pad0;
};

[[vk::binding(0, 0)]] Texture2D<float4> g_Textures[];
[[vk::binding(2, 0)]] [format("rgba16f")] RWTexture2D<float4> g_StorageImages[];

[[vk::push_constant]]
cbuffer PushConstants
{
	uint AlbedoIndex;
	uint NormalIndex;
	uint MaterialIndex;
	uint EmissiveIndex;
	uint DepthIndex;
	uint OutputIndex;

	uint2 Extent;

	UBCameraData* UBCamera;

	float4 LightDirection;
	float3 LightRadiance;
	float  LightIntensity;
};

// ==== G-buffer decoding ====

float2 SignNotZero(float2 value)
{
	return float2(value.x >= 0.0 ? 1.0 : -1.0, value.y >= 0.0 ? 1.0 : -1.0);
}

float3 DecodeOctahedralNormal(float2 encoded)
{
	const float2 octahedron = encoded * 2.0 - 1.0;

	float3 normal = float3(octahedron, 1.0 - abs(octahedron.x) - abs(octahedron.y));

	if (normal.z < 0.0)
		normal.xy = (1.0 - abs(normal.yx)) * SignNotZero(normal.xy);

	return normalize(normal);
}

float3 ReconstructWorldPosition(uint2 pixel, float depth)
{
	const float2 uv = (float2(pixel) + 0.5) / float2(Extent);

	float2 ndcXY = uv * 2.0 - 1.0;
	ndcXY.y = -ndcXY.y;

	const float4 ndc = float4(ndcXY, depth, 1.0);
	const float4 world = mul(UBCamera->InverseViewProjection, ndc);

	return world.xyz / world.w;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	const uint2 pixel = dispatchThreadID.xy;

	if (any(pixel >= Extent))
		return;

	const int3 texel = int3(pixel, 0);

	// ==== Depth ====

	const float depth = g_Textures[DepthIndex].Load(texel).r;

	if (depth >= 1.0)
	{
		g_StorageImages[OutputIndex][pixel] = float4(0.3, 0.2, 0.8, 1.0);
		return;
	}

	// ==== G-buffer ====

	const float4 albedoAO = g_Textures[AlbedoIndex].Load(texel);
	const float2 encodedNormal = g_Textures[NormalIndex].Load(texel).rg;
	const float4 material = g_Textures[MaterialIndex].Load(texel);
	const float3 emissive = g_Textures[EmissiveIndex].Load(texel).rgb;

	const float3 albedo = albedoAO.rgb;
	const float occlusion = saturate(albedoAO.a);
	const float3 N = DecodeOctahedralNormal(encodedNormal);
	const float linearRoughness = clamp(material.g, 0.045, 1.0);
	const float metalness = saturate(material.b);

	// Frostbite GGX roughness / alpha.
	const float roughness = linearRoughness * linearRoughness;

	// ==== World-space reconstruction ====

	const float3 worldPosition = ReconstructWorldPosition(pixel, depth);

	// ==== Directional light ====

	const float3 L = -normalize(LightDirection.xyz);
	const float3 V = normalize(UBCamera->CameraPosition - worldPosition);
	const float3 H = normalize(L + V);

	const float3 radiance = max(LightRadiance, float3(0.0)) * max(LightIntensity, 0.0);

	// ==== BRDF inputs ====

	const float NdotL = saturate(dot(N, L));
	const float NdotV = max(dot(N, V), 0.00001);
	const float NdotH = saturate(dot(N, H));
	const float LdotH = saturate(dot(L, H));

	// Dielectric F0 -> metallic F0
	const float3 dielectricF0 = float3(0.04);
	const float3 F0           = lerp(dielectricF0, albedo, metalness);

	// Schlick Fresnel
	const float3 F = FresnelSchlick(F0, 1.0, LdotH);

	// Trowbridge-Reitz GGX
	const float D = DistributionGGX(NdotH, roughness);

	// Height-correlated Smith GGX
	const float Vis = VisibilitySmithGGXCorrelated(NdotV, NdotL, roughness);

	// Cook-Torrance specular BRDF
	const float3 specular = SpecularCookTorrance(F, D, Vis);

	// Frostbite / Disney diffuse
	const float Fd = DiffuseDisney(NdotV, NdotL, LdotH, linearRoughness);

	// Energy conservation / metallic workflow
	const float3 kD      = (1.0 - F) * (1.0 - metalness);
	const float3 diffuse = kD * albedo * Fd / PI;

	// Lo = (diffuse BRDF + specular BRDF) * incoming radiance * NdotL
	const float3 directLighting = (diffuse + specular) * radiance * NdotL;

	// ==== Ambient (placeholder for IBL) ====

	const float3 ambient = 0.3 * albedo * (1.0 - metalness) * occlusion;

	const float3 lighting = directLighting + ambient + emissive;

	g_StorageImages[OutputIndex][pixel] = float4(lighting, 1.0);
	// g_StorageImages[OutputIndex][pixel] = float4(frac(worldPosition), 1.0);
	// g_StorageImages[OutputIndex][pixel] = float4(N * 0.5 + 0.5, 1.0);
}
