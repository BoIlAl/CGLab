#include "shaderCompiler.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <cstdio>

#include "common.h"


ShaderCompiler::ShaderCompiler(ID3D11Device* pDevice, bool isDebug)
	: m_pDevice(pDevice)
	, m_isDebug(isDebug)
{}

ShaderCompiler::~ShaderCompiler()
{}


bool ShaderCompiler::CreateVertexAndPixelShaders(
	const char* shaderFileName,
	ID3D11VertexShader** ppVS,
	ID3D10Blob** ppVSBinaryBlob,
	ID3D11PixelShader** ppPS
)
{
	FILE* pFile = nullptr;
	fopen_s(&pFile, shaderFileName, "rb");

	if (pFile == nullptr)
	{
		return false;
	}

	fseek(pFile, 0, SEEK_END);
	size_t fileSize = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);

	char* shaderSource = new char[fileSize + 1u];

	if (shaderSource == nullptr)
	{
		fclose(pFile);
		return false;
	}

	fread(shaderSource, fileSize, 1, pFile);
	shaderSource[fileSize] = 0;

	fclose(pFile);

	ID3DBlob* pErrors = nullptr;
	
	UINT flags = m_isDebug ? D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION : 0;

	HRESULT hr = D3DCompile(shaderSource, fileSize, shaderFileName, nullptr, nullptr, "VS", "vs_5_0", flags, 0, ppVSBinaryBlob, &pErrors);

	if (SUCCEEDED(hr))
	{
		hr = m_pDevice->CreateVertexShader(
			(*ppVSBinaryBlob)->GetBufferPointer(),
			(*ppVSBinaryBlob)->GetBufferSize(),
			nullptr,
			ppVS
		);
	}
	else
	{
		OutputDebugStringA((char*)pErrors->GetBufferPointer());
	}

	SafeRelease(pErrors);

	if (SUCCEEDED(hr))
	{
		ID3DBlob* pBlob = nullptr;

		hr = D3DCompile(shaderSource, fileSize, shaderFileName, nullptr, nullptr, "PS", "ps_5_0", flags, 0, &pBlob, &pErrors);

		if (SUCCEEDED(hr))
		{
			hr = m_pDevice->CreatePixelShader(
				pBlob->GetBufferPointer(),
				pBlob->GetBufferSize(),
				nullptr,
				ppPS
			);
		}
		else
		{
			OutputDebugStringA((char*)pErrors->GetBufferPointer());
		}

		SafeRelease(pBlob);
	}

	SafeRelease(pErrors);
	delete[] shaderSource;

	return SUCCEEDED(hr);
}
