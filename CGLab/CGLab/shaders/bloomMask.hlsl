Texture2D SrcTexture : register(t0);

Texture2D EmmisiveTexture : register(t1);

cbuffer BrightnessThresholdBuffer : register(b0)
{
    float4 brightnessThreshold; //r
}

static const float3 RGB_C = { 0.2126f, 0.7151f, 0.0722f };

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
    float3 color = SrcTexture.Load(int3(input.position.xy, 0)).rgb;
    
    float4 emissive = EmmisiveTexture.Load(int3(input.position.xy, 0));
    
    if (any(emissive != float4(0.0f, 0.0f, 0.0f, 0.0f)))
    {
        return emissive;
    }
    
    if (dot(color, RGB_C) > brightnessThreshold.r)
    {
        return float4(color, 1.0f);
    }
    
    return float4(0.0f, 0.0f, 0.0f, 0.0f);
}