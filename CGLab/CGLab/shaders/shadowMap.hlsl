static const uint MaxSplitsNum = 4u;


cbuffer PSSMConstantBuffer : register(b0)
{
    float4x4 modelMatrix;
    
    float4x4 vpMatrix[MaxSplitsNum];
}


struct VSIn
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 tangent  : TANGENT;
    float2 texCoord : TEXCOORD;
    
    uint instanceId : SV_INSTANCEID;
};


struct VSOut
{
    float4 position : SV_POSITION;
    uint instanceId : SV_INSTANCEID;
};


struct GSOut
{
    float4 position : SV_POSITION;
    uint rtIdx      : SV_RENDERTARGETARRAYINDEX;
};


VSOut VS(VSIn input)
{
    VSOut output = (VSOut)0;
    output.position = mul(float4(input.position, 1.0f), modelMatrix);
    output.position = mul(output.position, vpMatrix[input.instanceId]);
    output.instanceId = input.instanceId;
    
    return output;
}


[maxvertexcount(3)]
void GS(triangle VSOut input[3], inout TriangleStream<GSOut> triStream)
{
    GSOut p = (GSOut)0;
    p.rtIdx = input[0].instanceId;
    
    p.position = input[0].position;
    triStream.Append(p);
    
    p.position = input[1].position;
    triStream.Append(p);
    
    p.position = input[2].position;
    triStream.Append(p);
    
    triStream.RestartStrip();
}
