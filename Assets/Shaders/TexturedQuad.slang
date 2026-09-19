[[vk::binding(0, 0)]] Texture2D<float4> g_Textures[];
[[vk::binding(1, 0)]] SamplerState      g_Samplers[];

static const uint LINEAR_CLAMP_SAMPLER = 1;

[[vk::push_constant]]
cbuffer PushConstants
{
	uint TextureIndex;
};

struct VertexInput
{
	float2 Position : POSITION;
	float2 TexCoord : TEXCOORD0;
};

struct VertexOutput
{
	float4 Position : SV_Position;
	float2 TexCoord : TEXCOORD0;
};

[shader("vertex")]
VertexOutput main(VertexInput input)
{
	VertexOutput output;
	output.Position = float4(input.Position, 0.0, 1.0);
	output.TexCoord = input.TexCoord;
	return output;
}

[shader("fragment")]
float4 main(VertexOutput input) : SV_Target
{
	return g_Textures[TextureIndex].Sample(g_Samplers[LINEAR_CLAMP_SAMPLER], input.TexCoord);
}
