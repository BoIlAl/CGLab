static const float PI = 3.14159265f;

TextureCube EnvironmentMap : register(t0);

cbuffer ConstBuffer : register(b0)
{
    float4x4 modelMatrix;
    float4x4 vpMatrix;
}

cbuffer RoughnessBuffer: register(b1)
{
    float4 roughness;
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


float DistributionGGX(float3 normal, float3 halfVector, float roughness)
{
    roughness = clamp(roughness, 0.0001f, 1.0f);

    float alpha2 = roughness * roughness;
    float normalDotHalfVector = dot(normal, halfVector);
    float tmp = normalDotHalfVector * normalDotHalfVector * (alpha2 - 1.0f) + 1.0f;

    return alpha2 / (PI * tmp * tmp);
}


float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}


float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}


float3 ImportanceSampleGGX(float2 Xi, float3 norm, float roughness)
{
    float a = roughness * roughness;
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    float3 H;
    H.x = cos(phi) * sinTheta;
    H.z = sin(phi) * sinTheta;
    H.y = cosTheta;

    float3 up = abs(norm.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent = normalize(cross(up, norm));
    float3 bitangent = cross(norm, tangent);
    float3 sampleVec = tangent * H.x + bitangent * H.z + norm * H.y;

    return normalize(sampleVec);
}


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
    float R = roughness.r;
    float3 norm = normalize(input.worldPosition.xyz);
    float3 view = norm;
    float totalWeight = 0.0;
    float3 prefilteredColor = float3(0.0, 0.0, 0.0);
    static const uint SAMPLE_COUNT = 1024u;

    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, norm, R);
        float3 L = normalize(2.0 * dot(view, H) * H - view);

        float ndotl = max(dot(norm, L), 0.0);
        float ndoth = max(dot(norm, H), 0.0);
        float hdotv = max(dot(H, view), 0.0);

        float D = DistributionGGX(norm, H, R);
        float pdf = (D * ndoth / (4.0 * hdotv)) + 0.0001;
        float resolution = 512.0;
        float saTexel = 4.0 * PI / (6.0 * resolution * resolution);
        float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);
        float mipLevel = R == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel);

        if (ndotl > 0.0)
        {
            prefilteredColor += EnvironmentMap.SampleLevel(MinMagLinearSampler, L, mipLevel) * ndotl;
            totalWeight += ndotl;
        }
    }
    prefilteredColor = prefilteredColor / totalWeight;

    return float4(prefilteredColor.r, prefilteredColor.g, prefilteredColor.b, 0.0);
}

