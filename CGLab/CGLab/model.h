#pragma once
#include "rendererContext.h"

struct Mesh
{
	ID3D11Buffer* pVertexBuffer = nullptr;
	ID3D11Buffer* pIndexBuffer = nullptr;
	UINT indexCount = 0;
	DirectX::XMMATRIX modelMatrix = DirectX::XMMatrixIdentity();

	bool hasShadow = true;

	~Mesh()
	{
		SafeRelease(pIndexBuffer);
		SafeRelease(pVertexBuffer);
	}
};

class Model
{
public:
	struct Primitive
	{
		Mesh* pMesh = nullptr;

		ID3D11ShaderResourceView* pColorTextureSRV = nullptr;
		ID3D11ShaderResourceView* pNormalTextureSRV = nullptr;
		ID3D11ShaderResourceView* pMetalicRoughnessTextureSRV = nullptr;
		ID3D11ShaderResourceView* pEmissiveTextureSRV = nullptr;

		ID3D11SamplerState* pSamplerState = nullptr;

		D3D11_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
	};

public:
	static Model* CreateModel(
		RendererContext* pContext,
		const tinygltf::Model& model,
		const std::string& pathToModel,
		const DirectX::XMMATRIX& initMatrix = DirectX::XMMatrixIdentity()
	);

	~Model();

	inline UINT PrimitiveNum() const { return static_cast<UINT>(m_primitives.size()); };

	Primitive GetPrimitive(UINT idx) const;

private:
	Model(const std::string& pathToModel);

	bool Init(RendererContext* pContext, const tinygltf::Model& model, const DirectX::XMMATRIX& initMatrix);

	HRESULT ParseNode(
		RendererContext* pContext,
		const tinygltf::Model& model,
		UINT nodeIdx,
		const DirectX::XMMATRIX& prevMatrix
	);

	HRESULT LoadTextures(RendererContext* pContext, const tinygltf::Model& model);
	HRESULT LoadSamplers(RendererContext* pContext, const tinygltf::Model& model);
	HRESULT LoadMesh(
		RendererContext* pContext,
		const tinygltf::Model& model,
		UINT meshidx,
		const DirectX::XMMATRIX& modelMatrix
	);

	void SetUpPrimitives(const tinygltf::Model& model);

	std::vector<float> LoadFloatData(const tinygltf::BufferView& bufferView, const tinygltf::Accessor& accessor) const;
	std::vector<UINT16> LoadUInt16Data(const tinygltf::BufferView& bufferView, const tinygltf::Accessor& accessor) const;

	Mesh* CreateMesh(RendererContext* pContext, const std::vector<Vertex>& vertexData, const std::vector<UINT16>& indexData) const;

private:
	struct Texture
	{
		ID3D11Texture2D* pTexture;
		ID3D11ShaderResourceView* pTextureSRV;
	};

private:
	std::string m_pathToModel;

	std::vector<Texture> m_modelTextures;
	std::vector<ID3D11SamplerState*> m_modelSampelers;
	std::vector<Mesh*> m_modelMeshes;

	std::vector<Primitive> m_primitives;

	char* m_pModelData;
};
