TextureCube EnvironmentCubeMap : register(t0);

SamplerState MinMagLinearSampler : register(s0);

cbuffer ConstantBuffer : register(b0)
{
    float4x4 modelMatrix;
    float4x4 vpMatrix;
}


struct VSIn
{
    float3 position : POSITION;
    float4 color    : COLOR;
    float3 normal   : NORMAL;
};

struct VSOut
{
    float4 position : SV_POSITION;
    float3 texCoord : TEXCOORD;
};


VSOut VS(VSIn input)
{
    VSOut output;
    output.position = mul(mul(float4(input.position, 1.0f), modelMatrix), vpMatrix).xyww;
    output.texCoord = input.position;

    return output;
}

float4 PS(VSOut input) : SV_TARGET
{
    return EnvironmentCubeMap.Sample(MinMagLinearSampler, input.texCoord);
}
