// Copyright 2010 Intel Corporation
// All Rights Reserved
//
// Permission is granted to use, copy, distribute and prepare derivative works of this
// software for any purpose and without fee, provided, that the above copyright notice
// and this statement appear in all copies.  Intel makes no representations about the
// suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED "AS IS."
// INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
// INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
// INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  Intel does not
// assume any responsibility for any errors which may appear in this software nor any
// responsibility to update it.
#pragma once
#include "Colony.h"
#include "SDKmesh.h"
#include "SDKmisc.h"
#include "Game.h"

// The maximum objects rendered per draw call
static const unsigned int   gs_nMaxPerDraw = max( gs_nWorldSizeSq, gs_nMaxUnits );

enum ColorFilter
{
	WHITE = 20,
	RED,
	GREEN,
	BLUE,
	PURPLE,
	YELLOW,
	CYAN,
	BLACK
};

enum MeshType
{
    UNIT_MESH = 0,
    FACTORY_MESH,
    TREE_MESH,
    CONCRETE_MESH,
    GRASS_MESH,
    SKY_MESH,
    MESH_COUNT,
};

enum TextureType
{
    UNIT_DIFFUSE = 0,
    FACTORY_DIFFUSE,
    TREE_DIFFUSE,
    CONCRETE_DIFFUSE,
    GRASS_DIFFUSE,
    SKY_DIFFUSE,
    SKY_DARK_DIFFUSE,
    TEXTURE_COUNT,
};

typedef struct _STANDARD_VERTEX
{
    XMFLOAT3 Position;
    XMFLOAT3 Normal;
    XMFLOAT2 TexCoord;
}                           STANDARD_VERTEX;

// ColonyMesh class
// Uses an CDXUTSDKMesh internally to extract only the data we require to draw
// and lower the memory footprint
class ColonyMesh
{
private:
    XMVECTOR m_vCenter;
    XMVECTOR m_vBBoxExtents;
    ID3D11Device* m_pDevice;
    ID3D11Buffer* m_pVertexBuffer;
    ID3D11Buffer* m_pIndexBuffer;
    unsigned int m_nVertexStride;
    unsigned int m_nVertexCount;
    DXGI_FORMAT m_nIndexFormat;
    unsigned int m_nIndexCount;

public:
    static CDXUTSDKMesh m_SDKMesh;

    ColonyMesh( ID3D11Device* pDevice,
                LPCWSTR sMeshFilename );
    ColonyMesh( ID3D11Device* pDevice,
                const void* pVertices,
                unsigned int nVertexCount,
                unsigned int nVertexStride,
                const void* pIndices,
                unsigned int nIndexCount,
                DXGI_FORMAT nIndexFormat );
    ~ColonyMesh( void );
    ID3D11Buffer* GetVertexBuffer( void )
    {
        return m_pVertexBuffer;
    }
    ID3D11Buffer* GetIndexBuffer( void )
    {
        return m_pIndexBuffer;
    }
    unsigned int GetVertexStride( void )
    {
        return m_nVertexStride;
    }
    unsigned int GetVertexCount( void )
    {
        return m_nVertexCount;
    }
    DXGI_FORMAT GetIndexFormat( void )
    {
        return m_nIndexFormat;
    }
    unsigned int GetIndexCount( void )
    {
        return m_nIndexCount;
    }
    XMVECTOR GetCenter( void )
    {
        return m_vCenter;
    }
    XMVECTOR GetBBoxExtents( void )
    {
        return m_vBBoxExtents;
    }
};

// Render class
class Render
{
private:
    static XMMATRIX m_pVisible[ gs_nMaxPerDraw ];
    static XMVECTOR m_vCamPos;
    static XMVECTOR m_vCamDir;
    static float m_fCamFOV;
    static unsigned int m_nObjectsRendered;
    static ID3D11ShaderResourceView* m_pMeshTextures[TEXTURE_COUNT];
    static ColonyMesh* m_pMeshes[MESH_COUNT];
    static ID3D11Buffer* m_pInstanceBuffers[MESH_COUNT];
    static ID3D11SamplerState* m_pSamplerState;
    static ID3D11Device* m_pDevice;
    static ID3D11DeviceContext* m_pContext;
    static ID3D11InputLayout* m_pInputLayout;
    static ID3D11VertexShader* m_pVertexShader;
    static ID3D11PixelShader* m_pPixelShader;
	static ID3D11PixelShader* m_pPixelShaderRED;
	static ID3D11PixelShader* m_pPixelShaderGREEN;
	static ID3D11PixelShader* m_pPixelShaderBLUE;
	static ID3D11PixelShader* m_pPixelShaderPURPLE;
	static ID3D11PixelShader* m_pPixelShaderYELLOW;
	static ID3D11PixelShader* m_pPixelShaderCYAN;
	static ID3D11PixelShader* m_pPixelShaderBLACK;
    static ID3D11PixelShader* m_pSkyPixelShader;

public:
    static HRESULT Init( void );
    static void SetCamera( const XMVECTOR& vCamPos,
                           const XMVECTOR& vCamDir,
                           const float fCamFOV = XM_PIDIV4 );
    static void DrawInstanced( XMMATRIX* pTransforms,
                               MeshType Type,
                               unsigned int nInstanceCount,
                               bool bUpdateTransforms = true,
                               bool bCullObjects = true, 
							   ColorFilter filter = ColorFilter::WHITE);
    static void DrawTerrain( XMMATRIX* pTransforms,
                             unsigned int nInstanceCount );
    static void DrawSky( void );
    static void Destroy( void );
};
