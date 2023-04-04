#include "model.h"
#include "WICTextureLoader.h"

#include <fstream>

D3D11_TEXTURE_ADDRESS_MODE defineAddressMode(int gltfAddressMode)
{
	switch (gltfAddressMode)
	{
	case TINYGLTF_TEXTURE_WRAP_REPEAT:
		return D3D11_TEXTURE_ADDRESS_WRAP;

	case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
		return D3D11_TEXTURE_ADDRESS_CLAMP;

	case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
		return D3D11_TEXTURE_ADDRESS_MIRROR;

	default:
		break;
	}

	return D3D11_TEXTURE_ADDRESS_WRAP;
}

char* loadBinaryFile(const std::string& fileName)
{
	std::ifstream inFile;
	inFile.open(fileName, std::ios::binary);

	if (!inFile.is_open())
	{
		return nullptr;
	}

	inFile.seekg(0, std::ios::end);
	size_t fileSize = inFile.tellg();
	inFile.seekg(0, std::ios::beg);

	char* pData = static_cast<char*>(std::malloc(fileSize));

	if (pData == nullptr)
	{
		inFile.close();
		return nullptr;
	}

	inFile.read(pData, fileSize);
	inFile.close();

	return pData;
}


Model* Model::CreateModel(
	RendererContext* pContext,
	const tinygltf::Model& model,
	const std::string& pathToModel,
	const DirectX::XMMATRIX& initMatrix
)
{
	Model* pModel = new Model(pathToModel);

	if (pModel->Init(pContext, model, initMatrix))
	{
		return pModel;
	}

	delete pModel;
	return nullptr;
}


Model::Model(const std::string& pathToModel)
	: m_pathToModel(pathToModel)
	, m_pModelData(nullptr)
{}

Model::~Model()
{
	for (auto& pMesh : m_modelMeshes)
	{
		delete pMesh;
	}
	m_modelMeshes.clear();

	for (auto& pSampler : m_modelSampelers)
	{
		SafeRelease(pSampler);
	}
	m_modelSampelers.clear();

	for (auto& texture : m_modelTextures)
	{
		SafeRelease(texture.pTexture);
		SafeRelease(texture.pTextureSRV);
	}
}


Model::Primitive Model::GetPrimitive(UINT idx) const
{
	Primitive primitive;

	if (idx >= PrimitiveNum())
	{
		assert(false);
		return primitive;
	}

	return m_primitives[idx];
}


bool Model::Init(RendererContext* pContext, const tinygltf::Model& model, const DirectX::XMMATRIX& initMatrix)
{
	HRESULT hr = S_OK;

	m_pModelData = loadBinaryFile(m_pathToModel + "/" + model.buffers[0].uri);

	if (m_pModelData == nullptr)
	{
		hr = E_FAIL;
	}

	if (SUCCEEDED(hr))
	{
		LoadTextures(pContext, model);
	}

	if (SUCCEEDED(hr))
	{
		hr = LoadSamplers(pContext, model);
	}

	if (SUCCEEDED(hr))
	{
		ParseNode(pContext, model, 0, initMatrix);
	}

	std::free(m_pModelData);

	if (SUCCEEDED(hr))
	{
		SetUpPrimitives(model);
	}

	return SUCCEEDED(hr);
}

HRESULT Model::ParseNode(
	RendererContext* pContext,
	const tinygltf::Model& model,
	UINT nodeIdx,
	const DirectX::XMMATRIX& prevMatrix
)
{
	const tinygltf::Node& currentNode = model.nodes[nodeIdx];

	float translation[3] = { 0.0f, 0.0f, 0.0f };
	if (!currentNode.translation.empty())
	{
		for (UINT i = 0; i < _countof(translation); ++i)
		{
			translation[i] = static_cast<float>(currentNode.translation[i]);
		}
	}

	float rotation[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	if (!currentNode.rotation.empty())
	{
		for (UINT i = 0; i < _countof(rotation); ++i)
		{
			rotation[i] = static_cast<float>(currentNode.rotation[i]);
		}
	}

	float scale[3] = { 1.0f, 1.0f, 1.0f };
	if (!currentNode.scale.empty())
	{
		for (UINT i = 0; i < _countof(scale); ++i)
		{
			scale[i] = static_cast<float>(currentNode.scale[i]);
		}
	}

	float matrix[16] = {};
	memset(matrix, 0, sizeof(matrix));
	matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.0f;
	if (!currentNode.matrix.empty())
	{
		for (UINT i = 0; i < _countof(matrix); ++i)
		{
			matrix[i] = static_cast<float>(currentNode.matrix[i]);
		}
	}

	DirectX::XMMATRIX currentNodeMatrix =
		DirectX::XMMatrixScaling(scale[0], scale[1], scale[2]) *
		DirectX::XMMatrixRotationQuaternion({ rotation[0], rotation[1], rotation[2], rotation[3] }) *
		DirectX::XMMatrixTranslation(translation[0], translation[1], translation[2]) *
		DirectX::XMMATRIX(matrix) *
		prevMatrix;

	if (currentNode.mesh != -1)
	{
		if (FAILED(LoadMesh(
			pContext,
			model,
			currentNode.mesh,
			currentNodeMatrix
		)))
		{
			return E_FAIL;
		}
	}

	for (UINT childIdx : currentNode.children)
	{
		if (FAILED(ParseNode(pContext, model, childIdx, currentNodeMatrix)))
		{
			return E_FAIL;
		}
	}

	return S_OK;
}


HRESULT Model::LoadTextures(RendererContext* pContext, const tinygltf::Model& model)
{
	HRESULT hr = S_OK;

	for (const auto& imageData : model.images)
	{
		std::string pathToTexture = m_pathToModel + "/" + imageData.uri;
		Texture texture = { nullptr, nullptr };

		hr = CreateWICTextureFromFile(
			pContext->GetDevice(),
			pContext->GetContext(),
			std::wstring(pathToTexture.begin(), pathToTexture.end()).c_str(),
			(ID3D11Resource**)&texture.pTexture,
			&texture.pTextureSRV
		);

		if (FAILED(hr))
		{
			break;
		}

		m_modelTextures.push_back(texture);
	}

	return hr;
}

HRESULT Model::LoadSamplers(RendererContext* pContext, const tinygltf::Model& model)
{
	HRESULT hr = S_OK;

	for (const auto& samplerData : model.samplers)
	{
		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.MipLODBias = 0;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
		samplerDesc.AddressU = defineAddressMode(samplerData.wrapS);
		samplerDesc.AddressV = defineAddressMode(samplerData.wrapT);
		samplerDesc.AddressW = samplerDesc.AddressU;

		switch (samplerData.minFilter)
		{
		case TINYGLTF_TEXTURE_FILTER_NEAREST:
		case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
			samplerDesc.Filter = samplerData.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST ?
				D3D11_FILTER_MIN_MAG_MIP_POINT :
				D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
			break;

		case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
		case TINYGLTF_TEXTURE_FILTER_LINEAR:
			samplerDesc.Filter = samplerData.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST ?
				D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT :
				D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			break;

		case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
			samplerDesc.Filter = samplerData.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST ?
				D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR :
				D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;
			break;

		case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
			samplerDesc.Filter = samplerData.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST ?
				D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR :
				D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			break;

		default:
			samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			break;
		}

		ID3D11SamplerState* pSamplerState = nullptr;

		hr = pContext->GetDevice()->CreateSamplerState(&samplerDesc, &pSamplerState);

		if (FAILED(hr))
		{
			break;
		}

		m_modelSampelers.push_back(pSamplerState);
	}

	return hr;
}

HRESULT Model::LoadMesh(
	RendererContext* pContext,
	const tinygltf::Model& model,
	UINT meshIdx,
	const DirectX::XMMATRIX& modelMatrix
)
{
	const tinygltf::Mesh& mesh = model.meshes[meshIdx];

	if (mesh.primitives.size() > 1)
	{
		assert(false);
	}

	UINT positionIdx = mesh.primitives[0].attributes.at("POSITION");
	UINT normalIdx = mesh.primitives[0].attributes.at("NORMAL");
	UINT texCoordIdx = mesh.primitives[0].attributes.at("TEXCOORD_0");
	UINT indicesIdx = mesh.primitives[0].indices;

	const tinygltf::Accessor& positionDataAccessor = model.accessors[positionIdx];
	const tinygltf::Accessor& normalDataAccessor = model.accessors[normalIdx];
	const tinygltf::Accessor& texCoordDataAccessor = model.accessors[texCoordIdx];
	const tinygltf::Accessor& indicesDataAccessor = model.accessors[indicesIdx];

	const std::vector<UINT16>& indicesData = LoadUInt16Data(model.bufferViews[indicesDataAccessor.bufferView], indicesDataAccessor);

	const std::vector<float>& positionData = LoadFloatData(model.bufferViews[positionDataAccessor.bufferView], positionDataAccessor);
	const std::vector<float>& normalData = LoadFloatData(model.bufferViews[normalDataAccessor.bufferView], normalDataAccessor);
	const std::vector<float>& texCoordData = LoadFloatData(model.bufferViews[texCoordDataAccessor.bufferView], texCoordDataAccessor);

	{
		assert(positionData.size() % 3 == 0);
		assert(positionData.size() == normalData.size());
		assert(positionData.size() / 3 == texCoordData.size() / 2);
	}

	std::vector<Vertex> vertices;
	vertices.resize(positionData.size() / 3);

	for (UINT i = 0; i < vertices.size(); ++i)
	{
		Vertex& vertex = vertices[i];

		vertex.position = { positionData[3 * i + 0], positionData[3 * i + 1], positionData[3 * i + 2] };
		vertex.normal = { normalData[3 * i + 0], normalData[3 * i + 1], normalData[3 * i + 2] };
		vertex.texCoord = { texCoordData[2 * i + 0], texCoordData[2 * i + 1] };
		vertex.color = { 0.0f, 0.0f, 0.0f, 0.0f };
	}

	Mesh* pMesh = CreateMesh(pContext, vertices, indicesData);

	if (pMesh == nullptr)
	{
		return E_FAIL;
	}

	pMesh->modelMatrix = modelMatrix;
	m_modelMeshes.push_back(pMesh);

	return S_OK;
}


void Model::SetUpPrimitives(const tinygltf::Model& model)
{
	assert(m_modelMeshes.size() == model.meshes.size());

	m_primitives.resize(m_modelMeshes.size());

	for (UINT i = 0; i < m_primitives.size(); ++i)
	{
		const tinygltf::Material& material = model.materials[model.meshes[i].primitives[0].material];
		Primitive& primitive = m_primitives[i];

		primitive.pMesh = m_modelMeshes[i];

		int idx = -1;
		if ((idx = material.pbrMetallicRoughness.baseColorTexture.index) != -1)
		{
			primitive.pColorTextureSRV = m_modelTextures[model.textures[idx].source].pTextureSRV;
			primitive.pSamplerState = m_modelSampelers[model.textures[idx].sampler];
		}
		else
		{
			// TODO
			// material without textures
			assert(false);
			continue;
		}

		if ((idx = material.normalTexture.index) != -1)
		{
			primitive.pNormalTextureSRV = m_modelTextures[model.textures[idx].source].pTextureSRV;
		}

		if ((idx = material.pbrMetallicRoughness.metallicRoughnessTexture.index) != -1)
		{
			primitive.pMetalicRoughnessTextureSRV = m_modelTextures[model.textures[idx].source].pTextureSRV;
		}
		if ((idx = material.emissiveTexture.index) != -1)
		{
			primitive.pEmissiveTextureSRV = m_modelTextures[model.textures[idx].source].pTextureSRV;
		}
	}
}


std::vector<float> Model::LoadFloatData(const tinygltf::BufferView& bufferView, const tinygltf::Accessor& accessor) const
{
	UINT numPerVert = 0;

	switch (accessor.type)
	{
	case TINYGLTF_TYPE_VEC4:
		numPerVert = 4;
		break;

	case TINYGLTF_TYPE_VEC3:
		numPerVert = 3;
		break;

	case TINYGLTF_TYPE_VEC2:
		numPerVert = 2;
		break;

	case TINYGLTF_TYPE_SCALAR:
		numPerVert = 1;
		break;

	default:
		assert(false);
		return std::vector<float>();
	}

	size_t dataOffset = bufferView.byteOffset + accessor.byteOffset;
	size_t dataLength = sizeof(float) * numPerVert * accessor.count;

	std::vector<float> floatVector;
	floatVector.resize(numPerVert * accessor.count);

	std::memcpy(floatVector.data(), m_pModelData + dataOffset, floatVector.size() * sizeof(float));

	return floatVector;
}

std::vector<UINT16> Model::LoadUInt16Data(const tinygltf::BufferView& bufferView, const tinygltf::Accessor& accessor) const
{
	size_t dataOffset = bufferView.byteOffset + accessor.byteOffset;

	std::vector<UINT16> uint16Vector;
	uint16Vector.resize(accessor.count);

	switch (accessor.componentType)
	{
	case TINYGLTF_COMPONENT_TYPE_SHORT:
		for (UINT i = 0; i < uint16Vector.size(); ++i)
		{
			short value = 0;
			std::memcpy(&value, m_pModelData + dataOffset + i * sizeof(short), sizeof(short));

			uint16Vector[i] = static_cast<UINT16>(value);
		}
		break;


	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
		for (UINT i = 0; i < uint16Vector.size(); ++i)
		{
			unsigned short value = 0;
			std::memcpy(&value, m_pModelData + dataOffset + i * sizeof(unsigned short), sizeof(unsigned short));

			uint16Vector[i] = static_cast<UINT16>(value);
		}
		break;

	case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
		for (UINT i = 0; i < uint16Vector.size(); ++i)
		{
			unsigned int value = 0;
			std::memcpy(&value, m_pModelData + dataOffset + i * sizeof(unsigned int), sizeof(unsigned int));

			uint16Vector[i] = static_cast<UINT16>(value);
		}
		break;

	default:
		assert(false);
		return std::vector<UINT16>();
	}

	return uint16Vector;
}


Mesh* Model::CreateMesh(RendererContext* pContext, const std::vector<Vertex>& vertexData, const std::vector<UINT16>& indexData) const
{
	ID3D11Device* pDevice = pContext->GetDevice();
	Mesh* pMesh = new Mesh();

	D3D11_BUFFER_DESC vertexBufferDesc = CreateDefaultBufferDesc(
		static_cast<UINT>(vertexData.size()) * sizeof(Vertex),
		D3D11_BIND_VERTEX_BUFFER
	);
	D3D11_SUBRESOURCE_DATA vertexBufferData = CreateDefaultSubresourceData(vertexData.data());

	HRESULT hr = pDevice->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &pMesh->pVertexBuffer);

	if (SUCCEEDED(hr))
	{
		D3D11_BUFFER_DESC indexBufferDesc = CreateDefaultBufferDesc(
			static_cast<UINT>(indexData.size()) * sizeof(UINT16),
			D3D11_BIND_INDEX_BUFFER
		);
		D3D11_SUBRESOURCE_DATA indexBufferData = CreateDefaultSubresourceData(indexData.data());

		pMesh->indexCount = static_cast<UINT>(indexData.size());

		hr = pDevice->CreateBuffer(&indexBufferDesc, &indexBufferData, &pMesh->pIndexBuffer);
	}

	if (FAILED(hr))
	{
		delete pMesh;
		pMesh = nullptr;
	}

	return pMesh;
}
