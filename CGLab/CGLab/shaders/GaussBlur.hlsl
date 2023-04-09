Texture2D HDRTexture : register(t0);

SamplerState NoMipSampler : register(s0);

cbuffer ImageSizeBuffer : register(b0)
{
    float4 imageSize; //x - w, y - h
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

static const float weight1D[5] = { 0.447892, 0.238827, 0.035753, 0.001459, 0.000016 };

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
#if GAUSS_BLUR_SEPARATED_VERTICAL
    float uvOffset = 1.0 / imageSize.y;
#else
    float uvOffset = 1.0 / imageSize.x;
#endif
    
    float4 result = float4(0,0,0,0);
    for (int i = -4; i <= 4; i++)
    {
#if GAUSS_BLUR_SEPARATED_VERTICAL
        float2 uv = float2(input.texCoord.x, input.texCoord.y + i * uvOffset);
#else
        float2 uv = float2(input.texCoord.x + i * uvOffset, input.texCoord.y);
#endif
        result += weight1D[abs(i)] * HDRTexture.Sample(NoMipSampler, uv); 
    } 
    return result; 
} 
