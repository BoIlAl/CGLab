Texture2D ColorTexture : register(t0);

SamplerState MinMagLinearSampler : register(s0);

cbuffer ExposureBuffer : register(b0)
{
    float4 exposure; // r
}


static const float A = 0.10f;
static const float B = 0.50f;
static const float C = 0.10f;
static const float D = 0.20f;
static const float E = 0.02f;
static const float F = 0.30f;
static const float W = 11.2f;


struct VSIn
{
    uint vertexId : SV_VERTEXID;
};

struct VSOut
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};


float3 Uncharted2Tonemap(float3 clr)
{
    return ((clr * (A * clr + C * B) + D * E) / (clr * (A * clr + B) + D * F)) - E / F;
}


VSOut VS(VSIn input)
{
    VSOut vertices[4] =
    {
        { -1.0f, 1.0f, 0.0f, 1.0f },    { 0.0f, 0.0f },
        { 1.0f, 1.0f, 0.0f, 1.0f },     { 1.0f, 0.0f },
        { -1.0f, -1.0f, 0.0f, 1.0f },   { 0.0f, 1.0f },
        { 1.0f, -1.0f, 0.0f, 1.0f },    { 1.0f, 1.0f }
    };
    
    return vertices[input.vertexId];
}


float4 PS(VSOut input) : SV_TARGET
{
    float3 color = ColorTexture.Sample(MinMagLinearSampler, input.texCoord).rgb;
    
    float3 curr = Uncharted2Tonemap(exposure.r * color);
    float3 whiteScale = 1.0f / Uncharted2Tonemap(W);
    
    return float4(curr * whiteScale, 1.0f);
}
