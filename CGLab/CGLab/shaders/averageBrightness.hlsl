Texture2D ColorTexture : register(t0);

SamplerState MinMagLinearSampler : register(s0);

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


float L(float3 color)
{
    return max(dot(color, RGB_C), 0.0f);
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


float PS(VSOut input) : SV_TARGET
{
    float3 color = ColorTexture.Sample(MinMagLinearSampler, input.texCoord).rgb;
    return log10(L(color) + 1);
}