#include "rendererContext.h"

#include <d3d11_1.h>
#include <dxgi.h>

#include "common.h"
#include "WICTextureLoader.h"
#include "HDRITextureLoader.h"


RendererContext* RendererContext::CreateContext(IDXGIFactory* pFactory)
{
	RendererContext* pContext = new RendererContext();

	if (pContext->Init(pFactory))
	{
		return pContext;
	}

	delete pContext;
	return nullptr;
}


RendererContext::RendererContext()
	: m_pDevice(nullptr)
	, m_pContext(nullptr)
	, m_pAnnotation(nullptr)
	, m_pShaderCompiler(nullptr)
	, m_pHDRITextureLoader(nullptr)
#if _DEBUG
	, m_isDebug(true)
#else
	, m_isDebug(false)
#endif
{}

RendererContext::~RendererContext()
{
	delete m_pHDRITextureLoader;
	delete m_pShaderCompiler;

	SafeRelease(m_pAnnotation);
	SafeRelease(m_pContext);

	if (m_isDebug)
	{
		ID3D11Debug* pDebug = nullptr;
		HRESULT hr = m_pDevice->QueryInterface(IID_PPV_ARGS(&pDebug));
		if (SUCCEEDED(hr))
		{
			UINT references = m_pDevice->Release();
			if (references > 1)
			{
				pDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
			}
			SafeRelease(pDebug);
		}
	}
	else
	{
		SafeRelease(m_pDevice);
	}
}


bool RendererContext::Init(IDXGIFactory* pFactory)
{
	HRESULT hr = S_OK;

	IDXGIAdapter* pAdapter = nullptr;
	UINT adapterIdx = 0;

	while (SUCCEEDED(pFactory->EnumAdapters(adapterIdx, &pAdapter)))
	{
		DXGI_ADAPTER_DESC adapterDesc = {};
		pAdapter->GetDesc(&adapterDesc);

		if (wcscmp(adapterDesc.Description, L"Microsoft Basic Render Driver") != 0)
		{
			break;
		}

		pAdapter->Release();
		++adapterIdx;
	}

	if (pAdapter == nullptr)
	{
		hr = E_FAIL;
	}

	if (SUCCEEDED(hr))
	{
		D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };

		UINT flags = m_isDebug ? D3D11_CREATE_DEVICE_DEBUG : 0;

		hr = D3D11CreateDevice(
			pAdapter,
			D3D_DRIVER_TYPE_UNKNOWN,
			nullptr,
			flags,
			levels,
			1,
			D3D11_SDK_VERSION,
			&m_pDevice,
			nullptr,
			&m_pContext
		);
	}

	if (SUCCEEDED(hr))
	{
		hr = m_pContext->QueryInterface(IID_PPV_ARGS(&m_pAnnotation));
	}

	SafeRelease(pAdapter);

	if (SUCCEEDED(hr))
	{
		m_pShaderCompiler = new ShaderCompiler(m_pDevice, m_isDebug);

		if (m_pShaderCompiler == nullptr)
		{
			hr = E_FAIL;
		}
	}

	if (SUCCEEDED(hr))
	{
		m_pHDRITextureLoader = HDRITextureLoader::CreateHDRITextureLoader(this);

		if (m_pHDRITextureLoader == nullptr)
		{
			hr = E_FAIL;
		}
	}

	return SUCCEEDED(hr);
}

void RendererContext::BeginEvent(LPCWSTR eventName) const
{
	if (m_pAnnotation != nullptr)
	{
		m_pAnnotation->BeginEvent(eventName);
	}
}

void RendererContext::EndEvent() const
{
	if (m_pAnnotation != nullptr)
	{
		m_pAnnotation->EndEvent();
	}
}


HRESULT RendererContext::LoadTextureCube(
	const std::string& pathToCubeSrc,
	ID3D11Texture2D** ppTextureCube,
	ID3D11ShaderResourceView** ppTextureCubeSRV
) const
{
	std::string edges[6] = { "posx", "negx", "posy", "negy", "posz", "negz" };
	HRESULT hr = S_OK;

	ID3D11Texture2D* pSrcTexture = nullptr;

	if (SUCCEEDED(hr))
	{
		std::string edgeName = pathToCubeSrc + "/" + edges[0] + ".jpg";
		std::wstring wEdgeName(edgeName.begin(), edgeName.end());

		hr = CreateWICTextureFromFile(m_pDevice, m_pContext, wEdgeName.c_str(), (ID3D11Resource**)&pSrcTexture, nullptr);
	}

	D3D11_TEXTURE2D_DESC cubeMapDesc = {};
	if (SUCCEEDED(hr))
	{
		D3D11_TEXTURE2D_DESC srcTextureDesc = {};
		pSrcTexture->GetDesc(&srcTextureDesc);

		cubeMapDesc.Width = srcTextureDesc.Width;
		cubeMapDesc.Height = srcTextureDesc.Height;
		cubeMapDesc.Format = srcTextureDesc.Format;
		cubeMapDesc.MipLevels = 1;
		cubeMapDesc.ArraySize = 6;
		cubeMapDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		cubeMapDesc.Usage = D3D11_USAGE_DEFAULT;
		cubeMapDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
		cubeMapDesc.CPUAccessFlags = 0;
		cubeMapDesc.SampleDesc.Count = 1;
		cubeMapDesc.SampleDesc.Quality = 0;

		hr = m_pDevice->CreateTexture2D(&cubeMapDesc, nullptr, ppTextureCube);
	}

	if (SUCCEEDED(hr) && ppTextureCubeSRV != nullptr)
	{
		hr = m_pDevice->CreateShaderResourceView(*ppTextureCube, nullptr, ppTextureCubeSRV);
	}

	if (SUCCEEDED(hr))
	{
		m_pContext->CopySubresourceRegion(*ppTextureCube, 0, 0, 0, 0, pSrcTexture, 0, nullptr);
	}

	SafeRelease(pSrcTexture);

	for (UINT i = 1; i < _countof(edges) && SUCCEEDED(hr); ++i)
	{
		std::string edgeName = pathToCubeSrc + "/" + edges[i] + ".jpg";
		std::wstring wEdgeName(edgeName.begin(), edgeName.end());

		hr = CreateWICTextureFromFile(m_pDevice, m_pContext, wEdgeName.c_str(), (ID3D11Resource**)&pSrcTexture, nullptr);

		if (FAILED(hr))
		{
			break;
		}

		if (m_isDebug)
		{
			D3D11_TEXTURE2D_DESC srcTextureDesc = {};
			pSrcTexture->GetDesc(&srcTextureDesc);

			if (srcTextureDesc.Width != cubeMapDesc.Width
				|| srcTextureDesc.Height != cubeMapDesc.Height
				|| srcTextureDesc.Format != cubeMapDesc.Format)
			{
				hr = E_FAIL;
				break;
			}
		}

		m_pContext->CopySubresourceRegion(*ppTextureCube, i, 0, 0, 0, pSrcTexture, 0, nullptr);

		SafeRelease(pSrcTexture);
	}

	SafeRelease(pSrcTexture);

	return hr;
}


HRESULT RendererContext::LoadTextureCubeFromHDRI(
	const std::string& fileName,
	ID3D11Texture2D** ppTextureCube,
	UINT cubeTextureSize,
	ID3D11ShaderResourceView** ppTextureCubeSRV
) const
{
	return m_pHDRITextureLoader->LoadTextureCubeFromHDRI(fileName, ppTextureCube, cubeTextureSize, ppTextureCubeSRV);
}

HRESULT RendererContext::CalculateIrradianceMap(
	ID3D11ShaderResourceView* pTextureCubeSRV,
	ID3D11Texture2D** ppIrradianceMap,
	UINT cubeTextureSize,
	ID3D11ShaderResourceView** ppIrradianceMapSRV
) const
{
	return m_pHDRITextureLoader->CalculateIrradianceMap(pTextureCubeSRV, ppIrradianceMap, cubeTextureSize, ppIrradianceMapSRV);
}


HRESULT RendererContext::CreateSphereMesh(UINT16 latitudeBands, UINT16 longitudeBands, Mesh*& sphereMesh) const
{
	std::vector<Vertex> vertices;
	std::vector<UINT16> indices;

	for (UINT16 latNumber = 0; latNumber <= latitudeBands; ++latNumber)
	{
		float theta = latNumber * PI / latitudeBands;
		float sinTheta = sin(theta);
		float cosTheta = cos(theta);

		for (UINT16 longNumber = 0; longNumber <= longitudeBands; ++longNumber)
		{
			float phi = longNumber * 2 * PI / longitudeBands;
			float sinPhi = sin(phi);
			float cosPhi = cos(phi);

			float normalX = cosPhi * sinTheta;
			float normalY = cosTheta;
			float normalZ = sinPhi * sinTheta;

			Vertex vs = { { normalX, normalY, normalZ },
						  { 0.5f, 0.1f, 0.1f, 1.0f },
						  { normalX, normalY, normalZ } };

			vertices.push_back(vs);
		}
	}

	for (UINT16 latNumber = 0; latNumber < latitudeBands; ++latNumber)
	{
		for (UINT16 longNumber = 0; longNumber < longitudeBands; ++longNumber)
		{
			UINT16 first = (latNumber * (longitudeBands + 1)) + longNumber;
			UINT16 second = first + longitudeBands + 1;

			indices.push_back(first);
			indices.push_back(first + 1);
			indices.push_back(second);

			indices.push_back(second + 1);
			indices.push_back(second);
			indices.push_back(first + 1);
		}
	}

	Mesh* mesh = new Mesh();
	mesh->indexCount = (UINT)indices.size();

	D3D11_BUFFER_DESC vertexBufferDesc = CreateDefaultBufferDesc((UINT)vertices.size() * sizeof(Vertex), D3D11_BIND_VERTEX_BUFFER);
	D3D11_SUBRESOURCE_DATA vertexBufferData = CreateDefaultSubresourceData(vertices.data());

	HRESULT hr = m_pDevice->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &mesh->pVertexBuffer);
	if (SUCCEEDED(hr))
	{
		D3D11_BUFFER_DESC indexBufferDesc = CreateDefaultBufferDesc(mesh->indexCount * sizeof(UINT16), D3D11_BIND_INDEX_BUFFER);
		D3D11_SUBRESOURCE_DATA indexBufferData = CreateDefaultSubresourceData(indices.data());

		hr = m_pDevice->CreateBuffer(&indexBufferDesc, &indexBufferData, &mesh->pIndexBuffer);
	}

	if (SUCCEEDED(hr))
	{
		sphereMesh = mesh;
	}
	else
	{
		delete mesh;
	}

	return hr;
}
