static const float PI = 3.14159265f;

static const uint N1 = 800;
static const uint N2 = 200;

TextureCube Cubemap : register(t0);

cbuffer ConstBuffer : register(b0)
{
    float4x4 modelMatrix;
    float4x4 vpMatrix;
}

SamplerState MinMagLinearSampler : register(s0);


struct VSIn
{
    uint vertexId : SV_VERTEXID;
};

struct VSOut
{
    float4 position : SV_POSITION;
    float4 worldPosition : POSITION1;
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
    float3 normal = normalize(input.worldPosition.xyz);
    
    float3 dir = abs(normal.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    
    float3 tangent = normalize(cross(dir, normal));
    float3 binormal = cross(normal, tangent);
    
    float4 irradiance = float4(0.0, 0.0, 0.0, 0.0);
    
    for (int i = 0; i < N1; ++i)
    {
        for (int j = 0; j < N2; ++j)
        {
            float phi = i * (2.0f * PI / N1);
            float theta = j * (PI / 2.0f / N2);
            
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            float3 sampleVec = tangentSample.x * tangent + tangentSample.y * binormal + tangentSample.z * normal;
            
            irradiance += Cubemap.Sample(MinMagLinearSampler, sampleVec) * cos(theta) * sin(theta);
        }
    }
    
    irradiance = PI * irradiance / (N1 * N2);
    
    return irradiance;
}