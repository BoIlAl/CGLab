static const uint MaxLightNum = 3;

static const float PI = 3.14159265f;

cbuffer ConstantBuffer : register(b0)
{
    float4x4 modelMatrix;
    float4x4 vpMatrix;
    
    float3 cameraPosition;
}

struct Light
{
    float3 position;
    float4 color;
    float4 brightnessScaleFactor; // r
};

cbuffer LightBuffer : register(b1)
{
    uint4 lightsCount;
    Light lights[MaxLightNum];
}

cbuffer PBRParams : register(b2)
{
    float3 albedo;
    float4 roughnessMetalness; // r - roughness, g - metalness
};


struct VSIn
{
    float3 position : POSITION;
    float4 color    : COLOR;
    float3 normal   : NORMAL;
};


struct VSOut
{
    float4 position         : SV_POSITION;
    float4 color            : COLOR;
    float4 worldPosition    : POSITION1;
    float3 worldNormal      : NORMAL;
};


VSOut VS(VSIn input)
{
    VSOut output;
    output.worldPosition = mul(float4(input.position, 1.0f), modelMatrix);
    output.position = mul(output.worldPosition, vpMatrix);
    output.color = input.color;
    output.worldNormal = normalize(mul(float4(input.normal, 0.0f), modelMatrix).xyz);
    
    return output;
}


float NormalDistributionFunction(float3 normal, float3 halfVector, float roughness)
{
    roughness = clamp(roughness, 0.0001f, 1.0f);
    
    float alpha2 = roughness * roughness;
    float normalDotHalfVector = dot(normal, halfVector);
    float tmp = normalDotHalfVector * normalDotHalfVector * (alpha2 - 1.0f) + 1.0f;
    
    return alpha2 / (PI * tmp * tmp);
}


float SchlickGGX(float3 normal, float3 dir, float k)
{
    float normalDotDir = dot(normal, dir);

    return normalDotDir / (normalDotDir * (1 - k) + k);
}

float GeometryFunction(float3 normal, float3 dirToView, float3 dirToLight, float roughness)
{
    roughness = clamp(roughness, 0.0001f, 1.0f);
    
    float k = (roughness + 1.0f) * (roughness + 1.0f) / 8.0f;

    return SchlickGGX(normal, dirToView, k) * SchlickGGX(normal, dirToLight, k);
}


float FresnelFunction(float3 dirToView, float3 halfVector, float3 metalF0, float metalness)
{
    metalness = saturate(metalness);
    
    float3 F0 = (0.4f, 0.4f, 0.4f) * (1 - metalness) + metalF0 * metalness;
    
    return F0 + (1 - F0) * pow(1 - dot(dirToView, halfVector), 5.0f);
}


float3 BRDF(float3 position, float3 lightPosition, float3 normal)
{
    float3 dirToView = normalize(cameraPosition - position);
    float3 dirToLight = normalize(lightPosition - position);
    float3 halfVector = normalize((dirToView + dirToLight) / 2.0f);
    
    float roughness = roughnessMetalness.r;
    float metalness = roughnessMetalness.g;

    float D = NormalDistributionFunction(normal, halfVector, roughness);
    float F = FresnelFunction(dirToView, halfVector, albedo, metalness);
    float G = GeometryFunction(normal, dirToView, dirToLight, roughness);
    
    float Lambert = (1 - F) * albedo / PI;
    float CookTorrance = (D) / (4 * dot(dirToLight, normal) * dot(dirToView, normal));

    return Lambert * (1 - metalness) + CookTorrance;
}


float4 PS(VSOut input) : SV_TARGET
{
    float3 resultColor = float3(0.0f, 0.0f, 0.0f);
    
    for (uint i = 0; i < lightsCount.r; ++i)
    {
        float3 dirToLight = lights[i].position - input.worldPosition.xyz;
        float lengthToLight2 = dot(dirToLight, dirToLight);
        float lengthToLight = sqrt(lengthToLight2);
    
        float attenuation = 1 / (1 + lengthToLight + lengthToLight2);
    
        //float3 lightInpact = attenuation * saturate(dot(dirToLight / lengthToLight, input.worldNormal)) * lights[i].color.xyz;
        //lightInpact *= lights[i].brightnessScaleFactor.r;
    
        //resultColor += lightInpact;
        
        float3 lightInpact = attenuation * lights[i].color.xyz * lights[i].brightnessScaleFactor.r;
        lightInpact *= saturate(dot(dirToLight / lengthToLight, input.worldNormal));
        
        resultColor += BRDF(input.worldPosition.xyz, lights[i].position, input.worldNormal);
    }
    
    return float4(resultColor, 1.0f);
}
