Texture2D<float> ColorTexture : register(t0);

SamplerState MinMagLinearSampler : register(s0);

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
        { -1.0f, 1.0f, 0.0f, 1.0f },    { 0.0f, 0.0f },
        { 1.0f, 1.0f, 0.0f, 1.0f },     { 1.0f, 0.0f },
        { -1.0f, -1.0f, 0.0f, 1.0f },   { 0.0f, 1.0f },
        { 1.0f, -1.0f, 0.0f, 1.0f },    { 1.0f, 1.0f }
    };

    return vertices[input.vertexId];
}


float PS(VSOut input) : SV_TARGET
{
    float color = ColorTexture.Sample(MinMagLinearSampler, input.texCoord).r;
    return color;
}