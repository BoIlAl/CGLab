static const float PI = 3.14159265f;

struct VSIn
{
    uint vertexId : SV_VERTEXID;
};

struct VSOut
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};


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


float SchlickGGX(float3 n, float3 v, float k)
{
    float normalDotDir = dot(n, v);

    return normalDotDir / (normalDotDir * (1 - k) + k);
}


float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    roughness = clamp(roughness, 0.0001f, 1.0f);

    float k = roughness * roughness / 2.0f;

    return SchlickGGX(N, V, k) * SchlickGGX(N, L, k);
}


float2 integrateBRDF(float NdotV, float roughness)
{
    float3 V;
    V.x = sqrt(1.0 - NdotV);
    V.z = 0.0;
    V.y = NdotV;

    float A = 0.0;
    float B = 0.0;
    float3 N = float3(0.0, 1.0, 0.0);

    static const uint SAMPLE_COUNT = 1024u;
    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.0 * dot(V, H) * H - V);
        float NdotL = max(L.y, 0.0);
        float NdotH = max(H.y, 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if (NdotL > 0.0)
        {
            float G = GeometrySmith(N, V, L, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1.0 - VdotH, 5.0);
            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);

    return float2(A, B);
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


float2 PS(VSOut input) : SV_TARGET
{

    return integrateBRDF(input.texCoord.r, input.texCoord.g);
}
