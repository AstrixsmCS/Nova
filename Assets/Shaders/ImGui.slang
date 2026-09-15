[[vk::binding(0, 0)]] Texture2D<float4> g_Textures[];
[[vk::binding(1, 0)]] SamplerState      g_Samplers[];

static const uint LINEAR_CLAMP_SAMPLER = 1;

[[vk::push_constant]]
cbuffer PushConstants
{
	float2 Scale;
	float2 Translate;
	uint   Texture;
};

struct VertexInput
{
	float2 Position : POSITION;
	float2 TexCoord : TEXCOORD0;
	uint   Color    : COLOR0;
};

struct VertexOutput
{
	float4 Position : SV_Position;
	float4 Color    : COLOR0;
	float2 TexCoord : TEXCOORD0;
};

float4 UnpackColor(uint packed)
{
	return float4(
		float((packed >>  0) & 0xFF) / 255.0,
		float((packed >>  8) & 0xFF) / 255.0,
		float((packed >> 16) & 0xFF) / 255.0,
		float((packed >> 24) & 0xFF) / 255.0
	);
}

[shader("vertex")]
VertexOutput main(VertexInput input)
{
	VertexOutput output;
	output.Position = float4(input.Position * Scale + Translate, 0.0, 1.0);
	output.Color    = UnpackColor(input.Color);
	output.TexCoord = input.TexCoord;
	return output;
}

[shader("fragment")]
float4 main(VertexOutput input) : SV_Target
{
	return input.Color * g_Textures[Texture].Sample(g_Samplers[LINEAR_CLAMP_SAMPLER], input.TexCoord);
}
