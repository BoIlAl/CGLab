#pragma once

static constexpr FLOAT PI = 3.14159265359f;

template <class T>
void SafeRelease(T*& ptr)
{
	if (ptr != nullptr)
	{
		ptr->Release();
		ptr = nullptr;
	}
}


inline D3D11_BUFFER_DESC CreateDefaultBufferDesc(UINT size, UINT bindFlag)
{
	D3D11_BUFFER_DESC desc = {};
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.ByteWidth = size;
	desc.BindFlags = bindFlag;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	desc.StructureByteStride = 0;

	return desc;
}

inline D3D11_SUBRESOURCE_DATA CreateDefaultSubresourceData(const void* pData)
{
	D3D11_SUBRESOURCE_DATA data = {};
	data.pSysMem = pData;
	data.SysMemPitch = 0;
	data.SysMemSlicePitch = 0;

	return data;
}

inline D3D11_INPUT_ELEMENT_DESC CreateInputElementDesc(
	const char* semanticName,
	DXGI_FORMAT format,
	UINT offset
)
{
	D3D11_INPUT_ELEMENT_DESC elemDesc = {};
	elemDesc.SemanticName = semanticName;
	elemDesc.SemanticIndex = 0;
	elemDesc.Format = format;
	elemDesc.InputSlot = 0;
	elemDesc.AlignedByteOffset = offset;
	elemDesc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	elemDesc.InstanceDataStepRate = 0;

	return elemDesc;
}

inline UINT MinPower2(UINT width, UINT height)
{
	UINT min = width > height ? height : width;
	UINT n = 1;
	for (; n <= min; n <<= 1) {}
	return n >> 1;
}

inline bool IsPowerOf2(UINT x)
{
	return (x & (x - 1)) == 0;
}

inline UINT GetPowerOf2(UINT x)
{
	UINT res = 0;
	while (x >>= 1)
	{
		++res;
	}

	return res;
}
