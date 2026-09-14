struct PushConstants
{
	float4x4 ViewProjection;
};

[[vk::push_constant]]
PushConstants pc;

struct VertexInput
{
	float3 Position : POSITION;
	float3 Color    : COLOR0;
};

struct VertexOutput
{
	float4 Position : SV_Position;
	float3 Color    : COLOR0;
};

[shader("vertex")]
VertexOutput main(VertexInput input)
{
	VertexOutput output;
	output.Position = mul(pc.ViewProjection, float4(input.Position, 1.0));
	output.Color    = input.Color;
	return output;
}

[shader("fragment")]
float4 main(VertexOutput input) : SV_Target
{
	return float4(input.Color, 1.0);
}
