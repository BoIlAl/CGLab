static const float weight1D[5] = { 0.447892, 0.238827, 0.035753, 0.001459, 0.000016 };
static const float3 RGB_C = { 0.2126f, 0.7151f, 0.0722f };

Texture2D SrcTexture: register(t0);
#ifdef BLOOM_MASK
Texture2D EmmisiveTexture : register(t1);
#endif

SamplerState NoMipSampler : register(s0);
SamplerState MinMagLinearSampler : register(s1);

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


#if GAUSS_BLUR_SEPARATED_VERTICAL
float4 PS(VSOut input) : SV_TARGET
{
    float uvOffset = pixelSizeThreshold.g;
    float4 result = float4(0.0f ,0.0f , 0.0f, 0.0f);
    for (int i = -4; i <= 4; i++)
    {
        float2 uv = float2(input.texCoord.x, input.texCoord.y + i * uvOffset);
        result += weight1D[abs(i)] * SrcTexture.Sample(NoMipSampler, uv);
    }
    return result;
}
#elif GAUSS_BLUR_SEPARATED_HORIZONTAL
float4 PS(VSOut input) : SV_TARGET
{
    float uvOffset = pixelSizeThreshold.r;
    float4 result = float4(0.0f ,0.0f , 0.0f, 0.0f);
    for (int i = -4; i <= 4; i++)
    {
        float2 uv = float2(input.texCoord.x + i * uvOffset, input.texCoord.y);
        result += weight1D[abs(i)] * SrcTexture.Sample(NoMipSampler, uv);
    }
    return result;
}
#elif BLOOM_MASK
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
#else
float4 PS(VSOut input) : SV_TARGET
{
    return float4(SrcTexture.Sample(MinMagLinearSampler, input.texCoord).rgb, 1.0f);
}
#endif


