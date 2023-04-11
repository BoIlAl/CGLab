static const float3 RGB_C = { 0.2126f, 0.7151f, 0.0722f };

Texture2D SrcTexture : register(t0);
Texture2D EmmisiveTexture : register(t1);

SamplerState MinMagLinearSampler : register(s0);

cbuffer BloomConstantBuffer : register(b0)
{
    float4 pixelSizeThreshold; // rg - pixel size, b - brightness threshold
}


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
    float4 emissive = EmmisiveTexture.Sample(MinMagLinearSampler, input.texCoord);
    float3 color = SrcTexture.Sample(MinMagLinearSampler, input.texCoord).rgb;
    
    if (any(emissive.rgb != float3(0.0f, 0.0f, 0.0f)))
    {
        emissive.rgb += color;
        emissive.rgb *= 3.0f;
        return emissive;
    }
    
    
    if (dot(color, RGB_C) > pixelSizeThreshold.b)
    {
        return float4(color, 1.0f);
    }
    
    return float4(0.0f, 0.0f, 0.0f, 0.0f);
}