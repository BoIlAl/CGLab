#include "shaderCompiler.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <vector>
#include <string>
#include <cassert>

#include "common.h"


bool LoadShaderSource(
	const char* shaderFileName,
	bool isDebug,
	const char* defines,
	std::string& shaderSource
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

	shaderSource.resize(fileSize + 1u);

	fread(shaderSource.data(), fileSize, 1, pFile);
	shaderSource[fileSize] = 0;

	fclose(pFile);

	ID3DBlob* pErrors = nullptr;

	UINT flags = isDebug ? D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION : 0;

	std::string sDefines = defines;
	std::string addDefines = "";

	if (!sDefines.empty())
	{
		sDefines.append(" ");
	}

	size_t startPos = 0;
	size_t pos = 0;
	while ((pos = sDefines.find(' ', startPos)) != std::string::npos)
	{
		size_t equalPos = sDefines.find('=', startPos);
		assert(equalPos != std::string::npos);
		assert(equalPos < pos);

		sDefines[equalPos] = 0;
		sDefines[pos] = 0;

		const char* defineName = sDefines.c_str() + startPos;
		const char* definition = sDefines.c_str() + equalPos + 1;

		addDefines.append(std::string("#define ") + defineName + " " + definition + "\n");

		startPos = pos + 1;
	}

	shaderSource.insert(0, addDefines);
}


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
	ID3D11PixelShader** ppPS,
	const char* defines
)
{
	std::string shaderSource;

	if (!LoadShaderSource(shaderFileName, m_isDebug, defines, shaderSource))
	{
		return false;
	}

	ID3DBlob* pErrors = nullptr;
	UINT flags = m_isDebug ? D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION : 0;

	HRESULT hr = D3DCompile(
		shaderSource.c_str(), shaderSource.size(), shaderFileName, nullptr, nullptr,
		"VS", "vs_5_0", flags, 0, ppVSBinaryBlob, &pErrors
	);

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

		hr = D3DCompile(
			shaderSource.c_str(), shaderSource.size(), shaderFileName, nullptr, nullptr,
			"PS", "ps_5_0", flags, 0, &pBlob, &pErrors
		);

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

	return SUCCEEDED(hr);
}


bool ShaderCompiler::CreateVertexShader(
	const char* shaderFileName,
	ID3D11VertexShader** ppVS,
	ID3D10Blob** ppVSBinaryBlob,
	const char* defines
)
{
	std::string shaderSource;

	if (!LoadShaderSource(shaderFileName, m_isDebug, defines, shaderSource))
	{
		return false;
	}

	ID3DBlob* pErrors = nullptr;
	UINT flags = m_isDebug ? D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION : 0;

	HRESULT hr = D3DCompile(
		shaderSource.c_str(), shaderSource.size(), shaderFileName, nullptr, nullptr,
		"VS", "vs_5_0", flags, 0, ppVSBinaryBlob, &pErrors
	);

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

	return SUCCEEDED(hr);
}


bool ShaderCompiler::CreateGeometryShader(
	const char* shaderFileName,
	ID3D11GeometryShader** ppGS,
	ID3D10Blob** ppGSBinaryBlob,
	const char* defines
)
{
	std::string shaderSource;

	if (!LoadShaderSource(shaderFileName, m_isDebug, defines, shaderSource))
	{
		return false;
	}

	ID3DBlob* pErrors = nullptr;
	UINT flags = m_isDebug ? D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION : 0;

	HRESULT hr = D3DCompile(
		shaderSource.c_str(), shaderSource.size(), shaderFileName, nullptr, nullptr,
		"GS", "gs_5_0", flags, 0, ppGSBinaryBlob, &pErrors
	);

	if (SUCCEEDED(hr))
	{
		hr = m_pDevice->CreateGeometryShader(
			(*ppGSBinaryBlob)->GetBufferPointer(),
			(*ppGSBinaryBlob)->GetBufferSize(),
			nullptr,
			ppGS
		);
	}
	else
	{
		OutputDebugStringA((char*)pErrors->GetBufferPointer());
	}

	SafeRelease(pErrors);

	return SUCCEEDED(hr);
}
