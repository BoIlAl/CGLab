static const uint MaxLightNum = 3;

cbuffer ConstantBuffer : register(b0)
{
    float4x4 modelMatrix;
    float4x4 vpMatrix;
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
    output.worldNormal = mul(float4(input.normal, 0.0f), modelMatrix).xyz;
    
    return output;
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
    
        float3 lightInpact = attenuation * saturate(dot(dirToLight / lengthToLight, input.worldNormal)) * lights[i].color.xyz;
        lightInpact *= lights[i].brightnessScaleFactor.r;
    
        resultColor += lightInpact;
    }
    
    return float4(resultColor, 1.0f);
}
