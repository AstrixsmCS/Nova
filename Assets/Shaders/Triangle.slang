struct VertexInput
{
	float2 Position : POSITION;
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
	output.Position = float4(input.Position, 0.0, 1.0);
	output.Color    = input.Color;
	return output;
}

[shader("fragment")]
float4 main(VertexOutput input) : SV_Target
{
	return float4(input.Color, 1.0);
}
