[[vk::binding(0, 0)]] Texture2D<float4> g_Textures[];

[[vk::push_constant]]
cbuffer PushConstants
{
	uint InputIndex;
};

struct VertexOutput
{
	float4 Position : SV_Position;
};

float3 ACESFilm(float3 color)
{
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;

	return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

[shader("vertex")]
VertexOutput main(uint vertexID : SV_VertexID)
{
	const float2 uv = float2((vertexID << 1) & 2, vertexID & 2);

	VertexOutput output;
	output.Position = float4(uv * 2.0 - 1.0, 0.0, 1.0);

	return output;
}

[shader("fragment")]
float4 main(VertexOutput input) : SV_Target
{
	float3 color = g_Textures[InputIndex].Load(int3(int2(input.Position.xy), 0)).rgb;

	color *= 0.6;
	color = ACESFilm(color);

	return float4(color, 1.0);
}
