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
	uint   AlbedoIndex;
	uint   NormalIndex;
	uint   MetallicRoughnessIndex;
	uint   OcclusionIndex;
	float  Metalness;
	float  Roughness;
	float  AlphaCutoff;
	uint   Flags;
	float  Transmission;
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

	// Visualise world-space normal as color — replace with lighting once G-buffer is wired up
	return float4(N * 0.5 + 0.5, 1.0);
}
