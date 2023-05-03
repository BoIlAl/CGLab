cbuffer ConstantBuffer : register(b0)
{
    float4x4 modelMatrix;
    float4x4 vpMatrix;
    
    float3 cameraPosition;
}


struct VSIn
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 tangent  : TANGENT;
    float2 texCoord : TEXCOORD;
};


float4 VS(VSIn input) : SV_POSITION
{
    float4 worldPosition = mul(float4(input.position, 1.0f), modelMatrix);
    float4 svPosition = mul(worldPosition, vpMatrix);
    
    return svPosition;
}
