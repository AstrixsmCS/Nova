[[vk::binding(2, 0)]] RWTexture2D<float4> g_StorageImages[];

[[vk::push_constant]]
cbuffer PushConstants
{
	float Time;
	uint  Width;
	uint  Height;
	uint  OutputImageIndex;
};

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	if (id.x >= Width || id.y >= Height)
		return;

	const float u = float(id.x) / float(Width);
	const float v = float(id.y) / float(Height);

	const float r = 0.5f + 0.5f * sin(Time + u * 6.2831f);
	const float g = 0.5f + 0.5f * sin(Time + v * 6.2831f + 2.094f);
	const float b = 0.5f + 0.5f * sin(Time + (u + v) * 3.1415f + 4.189f);

	g_StorageImages[OutputImageIndex][int2(id.xy)] = float4(r, g, b, 0.2f);
}
