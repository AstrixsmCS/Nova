struct UBCameraData
{
	float4x4 ViewProjection;
	float4x4 InverseViewProjection;
	float3   CameraPosition;
	float    _Pad0;
};

struct GPUMaterialData
{
	float4 Albedo;

	uint AlbedoIndex;
	uint NormalIndex;
	uint MetallicRoughnessIndex;
	uint OcclusionIndex;
	uint EmissiveIndex;

	float Metalness;
	float Roughness;
	float AlphaCutoff;
	uint  Flags;

	float4 emissive;

	float Transmission;
};

[[vk::binding(0, 0)]] Texture2D<float4> g_Textures[];
[[vk::binding(1, 0)]] SamplerState      g_Samplers[];

static const uint ANISOTROPIC_REPEAT_SAMPLER = 3;

[[vk::push_constant]]
cbuffer PushConstants
{
	float4x4         Model;
	UBCameraData*    UBCamera;
	GPUMaterialData* SBMaterials;
	uint             MaterialIndex;

	float4 LightDirection;
	float4 LightColorIntensity;
};

struct VertexInput
{
	float3 Position : POSITION;
	float3 Normal   : NORMAL;
	float2 TexCoord : TEXCOORD0;
	float4 Tangent  : TANGENT;
};

struct VertexOutput
{
	float4 Position : SV_Position;
	float3 WorldPos : TEXCOORD2;
	float3 Normal   : NORMAL;
	float2 TexCoord : TEXCOORD0;
	float4 Tangent  : TANGENT;
};

[shader("vertex")]
VertexOutput main(VertexInput input)
{
	const float4   worldPosition = mul(Model, float4(input.Position, 1.0));
	const float3x3 model3x3      = (float3x3)Model;

	const float3 N = normalize(mul(model3x3, input.Normal));
	const float3 T = normalize(mul(model3x3, input.Tangent.xyz));

	VertexOutput output;
	output.Position = mul(UBCamera->ViewProjection, worldPosition);
	output.WorldPos = worldPosition.xyz;
	output.Normal   = N;
	output.TexCoord = input.TexCoord;
	output.Tangent  = float4(T, input.Tangent.w);

	return output;
}

[shader("fragment")]
float4 main(VertexOutput input) : SV_Target
{
	const GPUMaterialData mat = SBMaterials[MaterialIndex];
	const float2          uv  = input.TexCoord;

	// Albedo
	float4 albedo = mat.Albedo;

	if (mat.AlbedoIndex != 0)
		albedo *= g_Textures[mat.AlbedoIndex].Sample(g_Samplers[ANISOTROPIC_REPEAT_SAMPLER], uv);

	if (albedo.a < mat.AlphaCutoff)
		discard;

	const float3 baseColor = max(albedo.rgb, float3(0.0, 0.0, 0.0));

	// Metallic / roughness
	float metallic  = mat.Metalness;
	float roughness = mat.Roughness;

	if (mat.MetallicRoughnessIndex != 0)
	{
		const float4 metallicRoughness = g_Textures[mat.MetallicRoughnessIndex].Sample(g_Samplers[ANISOTROPIC_REPEAT_SAMPLER], uv);

		metallic  *= metallicRoughness.b;
		roughness *= metallicRoughness.g;
	}

	metallic  = saturate(metallic);
	roughness = saturate(roughness);

	// Normal
	float3 N = normalize(input.Normal);

	if (mat.NormalIndex != 0)
	{
		float3 T = normalize(input.Tangent.xyz);
		T = normalize(T - N * dot(N, T));

		const float3 B             = cross(N, T) * input.Tangent.w;
		const float3 tangentNormal = g_Textures[mat.NormalIndex].Sample(g_Samplers[ANISOTROPIC_REPEAT_SAMPLER], uv).xyz * 2.0 - 1.0;

		N = normalize(tangentNormal.x * T + tangentNormal.y * B + tangentNormal.z * N);
	}

	// Ambient occlusion
	float occlusion = 1.0;

	if (mat.OcclusionIndex != 0)
	{
		occlusion = g_Textures[mat.OcclusionIndex].Sample(g_Samplers[ANISOTROPIC_REPEAT_SAMPLER], uv).r;
	}

	occlusion = saturate(occlusion);

	// Emissive: rgb = linear color factor, a = HDR strength.
	float3 emission = max(mat.emissive.rgb, float3(0.0, 0.0, 0.0)) * max(mat.emissive.a, 0.0);

	if (mat.EmissiveIndex != 0)
	{
		emission *= g_Textures[mat.EmissiveIndex].Sample(g_Samplers[ANISOTROPIC_REPEAT_SAMPLER], uv).rgb;
	}

	// Directional light: LightDirection points along light travel.
	const float3 V = normalize(UBCamera->CameraPosition - input.WorldPos);
	const float3 L = -normalize(LightDirection.xyz);

	const float3 lightColor = max(LightColorIntensity.rgb, float3(0.0, 0.0, 0.0));
	const float  intensity  = max(LightColorIntensity.w, 0.0);

	const float NdotL = saturate(dot(N, L));
	const float NdotV = saturate(dot(N, V));

	// Blinn–Phong material approximation.
	const float smoothness = 1.0 - roughness;
	const float shininess  = lerp(4.0, 256.0, smoothness * smoothness);

	const float3 diffuseColor  = baseColor * (1.0 - metallic);
	const float3 specularColor = lerp(float3(0.04, 0.04, 0.04), baseColor, metallic);

	float3 directLighting = float3(0.0, 0.0, 0.0);

	if (NdotL > 0.0)
	{
		const float3 diffuse = diffuseColor * NdotL;

		float specularFactor = 0.0;

		if (NdotV > 0.0)
		{
			const float3 halfVector = V + L;
			const float halfLengthSquared = dot(halfVector, halfVector);

			if (halfLengthSquared > 0.000001)
			{
				const float3 H = halfVector * rsqrt(halfLengthSquared);

				specularFactor = pow(saturate(dot(N, H)), shininess);
			}
		}

		const float3 specular = specularColor * specularFactor * NdotL;

		directLighting = (diffuse + specular) * lightColor * intensity;
	}

	const float3 ambient = 0.03 * baseColor * occlusion;

	float3 color = directLighting + ambient + emission;

	// Temporary Reinhard tone mapping for direct swapchain rendering
	color = color / (color + 1.0);

	return float4(color, 1.0);
}
