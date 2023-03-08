static const float PI = 3.14159265f;

Texture2D EquirectangulaSource : register(t0);

cbuffer ConstBuffer : register(b0)
{
    float4x4 modelMatrix;
    float4x4 vpMatrix;
}

SamplerState MinMagLinearSampler : register(s0);


struct VSIn
{
    uint vertexId   : SV_VERTEXID;
};

struct VSOut
{
    float4 position         : SV_POSITION;
    float4 worldPosition    : POSITION1; 
};


VSOut VS(VSIn input)
{
    float4 positions[4] =
    {
        { -1.0f, 1.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f },
        { -1.0f, -1.0f, 1.0f, 1.0f },
        { 1.0f, -1.0f, 1.0f, 1.0f }
    };
    
    VSOut output;
    output.worldPosition = mul(positions[input.vertexId], modelMatrix);
    output.position = mul(output.worldPosition, vpMatrix);
    
    return output;
}


float4 PS(VSOut input) : SV_TARGET
{
    float3 worldPos = normalize(input.worldPosition.xyz);
    
    float u = 1.0f - atan2(worldPos.z, worldPos.x) / (2 * PI);
    float v = 1.0f - (0.5f + asin(worldPos.y) / PI);
    
    return EquirectangulaSource.Sample(MinMagLinearSampler, float2(u, v));
}
