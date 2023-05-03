static const uint MaxLightNum = 3;

static const float PI = 3.14159265f;

TextureCube DiffuseIrradianceMap    : register(t0);
TextureCube EnvMap                  : register(t1);
Texture2D BRDFLut                   : register(t2);

Texture2D ShadowMap                 : register(t20);

cbuffer ConstantBuffer : register(b0)
{
    float4x4 modelMatrix;
    float4x4 vpMatrix;
    
    float3 cameraPosition;
}

struct DirectionalLight
{
    float4 direction;
    float4 color;
    
    float4x4 dirLightVpMatrix;
};

struct Light
{
    float3 position;
    float4 color;
    float4 brightnessScaleFactor; // r
};

cbuffer LightBuffer : register(b1)
{
    DirectionalLight directionalLight;
    
    uint4 lightsCount;
    Light lights[MaxLightNum];
}

cbuffer PBRParams : register(b2)
{
    float3 albedo;
    float4 roughnessMetalness; // r - roughness, g - metalness
    
    uint4 pbrMode; // r - mode : 1 - Normal Distribution, 2 - Geometry, 3 - Fresnel, Overwise - All
};

SamplerState MinMagMipLinearSampler             : register(s0);
SamplerState MinMagLinearSamplerClamp           : register(s1);
SamplerComparisonState MinMagMipNearestSampler  : register(s2);


Texture2D BaseColorTexture          : register(t10);
Texture2D NormalTexture             : register(t11);
Texture2D MetalicRoughnessTexture   : register(t12);
Texture2D EmmisiveTexture           : register(t13);

SamplerState MeshTextureSampler     : register(s10);


struct VSIn
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 tangent  : TANGENT;
    float2 texCoord : TEXCOORD;
};


struct VSOut
{
    float4 position         : SV_POSITION;
    float4 worldPosition    : POSITION1;
    float3 worldNormal      : NORMAL;
    float3 worldTangent     : TANGENT;
    float2 texCoord         : TEXCOORD;
};


struct PSOut
{
    float4 color    : SV_TARGET0;
    float4 emissive  : SV_TARGET1;
};


VSOut VS(VSIn input)
{
    VSOut output;
    output.worldPosition = mul(float4(input.position, 1.0f), modelMatrix);
    output.position = mul(output.worldPosition, vpMatrix);
    output.worldNormal = normalize(mul(float4(input.normal, 0.0f), modelMatrix).xyz);
    
#if HAS_COLOR_TEXTURE
    output.worldTangent = normalize(mul(input.tangent, modelMatrix).xyz);
    output.texCoord = input.texCoord;
#endif
    
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


float3 FresnelSchlickRoughnessFunction(float3 F0, float3 dirToView, float3 normal, float roughness)
{
    float invRoughness = max(1 - roughness, 0.0f);
    
    return F0 + (max(float3(invRoughness, invRoughness, invRoughness), F0) - F0) * pow(1 - dot(dirToView, normal), 5.0f);
}

float3 FresnelFunction(float3 dirToView, float3 halfVector, float3 metalF0, float metalness)
{
    metalness = saturate(metalness);

    float3 F0 = max(float3(0.04f, 0.04f, 0.04f) * (1 - metalness) + metalF0 * metalness, float3(0.0f, 0.0f, 0.0f));

    return F0 + (1 - F0) * pow(1 - dot(dirToView, halfVector), 5.0f);
}


float3 BRDF(
    float3 position,
    float3 dirToLight,
    float3 normal,
    float3 metalF0,
    float roughness,
    float metalness
)
{
    float3 dirToView = normalize(cameraPosition - position);
    float3 halfVector = normalize((dirToView + dirToLight) / 2.0f);
    
    float D = NormalDistributionFunction(normal, halfVector, roughness);
    float3 F = FresnelFunction(dirToView, halfVector, metalF0, metalness);
    float G = clamp(GeometryFunction(normal, dirToView, dirToLight, roughness), 0.0f, 5.0f);
    
    if (pbrMode.r == 1u)
    {
        return float3(D, D, D);
    }
    else if (pbrMode.r == 2u)
    {
        return float3(G, G, G);
    }
    else if (pbrMode.r == 3u)
    {
        return F;
    }
    
    float3 Lambert = (1 - F) * metalF0 / PI;
    float3 CookTorrance = (D * F * G) / (4 * dot(dirToLight, normal) * dot(dirToView, normal));
    
    return max(Lambert * (1 - metalness) + CookTorrance, float3(0.0f, 0.0f, 0.0f));
}

//float SampleShadowMap(float3 position)
//{
//    float4 lightProjPos = mul(float4(position, 1.0f), directionalLight.dirLightVpMatrix);
//    float2 texCoord = lightProjPos.xy * float2(0.5, -0.5) + float2(0.5f, 0.5f);

//    float depth = ShadowMap.Sample(MinMagMipNearestSampler, texCoord);

//    return depth;
//}


PSOut PS(VSOut input)
{
    float3 resultColor = float3(0.0f, 0.0f, 0.0f);
    
    float3 normal = normalize(input.worldNormal);
    float3 metalF0 = albedo;
    float2 rm = roughnessMetalness.rg;
    
#if HAS_COLOR_TEXTURE
    metalF0 *= BaseColorTexture.Sample(MeshTextureSampler, input.texCoord).rgb;
    
    float3 n = NormalTexture.Sample(MeshTextureSampler, input.texCoord).rgb;
    
    float3 tangent = normalize(input.worldTangent);
    float3 binormal = cross(normal, tangent);
    
    n = normalize(n * 2.0f - float3(1.0f, 1.0f, 1.0f));
    normal = normalize(mul(n, float3x3(tangent, binormal, normal)));
    
    rm *= MetalicRoughnessTexture.Sample(MinMagMipLinearSampler, input.texCoord).gb;
#endif
    
    float roughness = max(rm.r, 0.001f);
    float metalness = max(rm.g, 0.001f);
    
#if HAS_EMISSIVE_TEXTURE
    float4 emissive = EmmisiveTexture.Sample(MeshTextureSampler, input.texCoord);
#else
    float4 emissive = float4(0.0f, 0.0f, 0.0f, 0.0f);  
#endif
    
    metalF0 += emissive;
    
    float3 v = normalize(cameraPosition - input.worldPosition.xyz);
    
    {
        float3 dirToLight = -normalize(directionalLight.direction.xyz);
        float3 lightImpact = directionalLight.color.rgb;
        lightImpact *= saturate(dot(dirToLight, normal));
        
        float4 lightProjPos = mul(float4(input.worldPosition.xyz, 1.0f), directionalLight.dirLightVpMatrix);
        float2 texCoord = lightProjPos.xy * float2(0.5, -0.5) + float2(0.5f, 0.5f);

        float depth = ShadowMap.SampleCmp(MinMagMipNearestSampler, texCoord, lightProjPos.z);
        
        if (lightProjPos.z <= depth)
        {
            resultColor += lightImpact * BRDF(
                input.worldPosition.xyz,
                dirToLight,
                normal,
                metalF0,
                roughness,
                metalness
            );
        }
    }
    
    for (uint i = 0; i < lightsCount.r; ++i)
    {
        float3 dirToLight = lights[i].position - input.worldPosition.xyz;
        float lengthToLight2 = dot(dirToLight, dirToLight);
        float lengthToLight = sqrt(lengthToLight2);
    
        float attenuation = 1 / (1 + lengthToLight + lengthToLight2);
    
        float3 lightInpact = attenuation * lights[i].color.xyz * lights[i].brightnessScaleFactor.r;
        lightInpact *= saturate(dot(dirToLight / lengthToLight, normal));
        
        resultColor += lightInpact * BRDF(
            input.worldPosition.xyz,
            normalize(dirToLight),
            normal,
            metalF0,
            roughness,
            metalness
        );
    }
    
    static const float MAX_REFLECTION_LOD = 4.0f;
    float3 prefilteredColor = EnvMap.SampleLevel(MinMagMipLinearSampler, normalize(2.0f * dot(v, normal) * normal - v), roughness * MAX_REFLECTION_LOD);
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), metalF0, float3(metalness, metalness, metalness));
    float2 envBRDF = BRDFLut.Sample(MinMagLinearSamplerClamp, float2(max(dot(normal, v), 0.0f), roughness));
    float3 specular = prefilteredColor * (F0 * envBRDF.x + float3(envBRDF.y, envBRDF.y, envBRDF.y));

    float3 kS = FresnelSchlickRoughnessFunction(F0, v, normal, roughness);
    float3 kD = float3(1.0f, 1.0f, 1.0f) - saturate(kS);
    kD *= 1.0 - metalness;
    float3 irradiance = DiffuseIrradianceMap.Sample(MinMagMipLinearSampler, normal).rgb;
    float3 diffuse = irradiance * metalF0.xyz;
    float3 ambient = (kD * diffuse + specular);
    
    PSOut output = (PSOut)0;
    output.color = float4(resultColor + ambient, 1.0f);
    output.emissive = emissive;

    return output;
}