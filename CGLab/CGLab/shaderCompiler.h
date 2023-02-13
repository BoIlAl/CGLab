#pragma once


struct ID3D11Device;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D10Blob;


class ShaderCompiler
{
public:
	ShaderCompiler(ID3D11Device* pDevice);
	~ShaderCompiler();

	bool CreateVertexAndPixelShaders(
		const char* shaderFileName,
		ID3D11VertexShader** ppVS,
		ID3D10Blob** ppVSBinaryBlob,
		ID3D11PixelShader** ppPS,
		bool isDebug
	);

private:
	ID3D11Device* m_pDevice;
	
};
