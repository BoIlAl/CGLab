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
    float3 position : POSITION;
};

struct VSOut
{
    float4 position         : SV_POSITION;
    float4 worldPosition    : POSITION1; 
};


VSOut VS(VSIn input)
{
    VSOut output;
    output.worldPosition = mul(float4(input.position, 1.0f), modelMatrix);
    output.position = mul(output.worldPosition, vpMatrix);
    
    return output;
}


float4 PS(VSOut input) : SV_TARGET
{
    float3 worldPos = normalize(input.worldPosition.xyz);
    
    float u = 1.0f - atan(worldPos.z / worldPos.x) / (2 * PI);
    float v = asin(worldPos.y) / PI - 0.5f;
    
    return EquirectangulaSource.Sample(MinMagLinearSampler, float2(u, v));
}
