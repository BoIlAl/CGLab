Texture2D HDRTexture : register(t0);
Texture2D BloomTexture: register(t1);

struct VSIn
{
    uint vertexId : SV_VERTEXID;
};

struct VSOut
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

VSOut VS(VSIn input)
{
    VSOut vertices[4] =
    {
        { -1.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f },
        { 1.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f },
        { -1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f },
        { 1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }
    };

    return vertices[input.vertexId];
}

float4 PS(VSOut input) : SV_TARGET
{ 
    float3 colorHDR = HDRTexture.Load(int3(input.position.xy, 0)).rgb;
    float3 colorBloom = BloomTexture.Load(int3(input.position.xy, 0)).rgb;
   
    return float4(colorHDR, 1.0f);
}
