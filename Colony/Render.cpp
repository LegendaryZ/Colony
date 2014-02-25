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
#include "Render.h"
#include "Instrumentation.h"

// Intel GPA 4.0 defines
static __itt_domain*        s_pRenderDomain = __itt_domain_createA( "Colony.Render" );

// Render defines
CDXUTSDKMesh                ColonyMesh::m_SDKMesh;

ColonyMesh*                 Render::m_pMeshes[MESH_COUNT];
ID3D11ShaderResourceView*   Render::m_pMeshTextures[TEXTURE_COUNT];
ID3D11Buffer*               Render::m_pInstanceBuffers[MESH_COUNT];
ID3D11SamplerState*         Render::m_pSamplerState = NULL;
ID3D11Device*               Render::m_pDevice = NULL;
ID3D11DeviceContext*        Render::m_pContext = NULL;
ID3D11InputLayout*          Render::m_pInputLayout = NULL;
ID3D11VertexShader*         Render::m_pVertexShader = NULL;
ID3D11PixelShader*          Render::m_pPixelShader = NULL;
ID3D11PixelShader*          Render::m_pPixelShaderRED = NULL;
ID3D11PixelShader*          Render::m_pPixelShaderGREEN = NULL;
ID3D11PixelShader*          Render::m_pPixelShaderBLUE = NULL;
ID3D11PixelShader*          Render::m_pPixelShaderPURPLE = NULL;
ID3D11PixelShader*          Render::m_pPixelShaderYELLOW = NULL;
ID3D11PixelShader*          Render::m_pPixelShaderCYAN = NULL;
ID3D11PixelShader*          Render::m_pPixelShaderBLACK = NULL;
ID3D11PixelShader*          Render::m_pSkyPixelShader = NULL;
XMVECTOR                    Render::m_vCamPos = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
XMVECTOR                    Render::m_vCamDir = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
float                       Render::m_fCamFOV = 0.0f;
unsigned int                Render::m_nObjectsRendered = 0;
XMMATRIX                    Render::m_pVisible[ gs_nMaxPerDraw ];


////////////////////////////////////////////////////////////////////////////////
// Static data initialization
////////////////////////////////////////////////////////////////////////////////

static D3D11_INPUT_ELEMENT_DESC g_pStandardVertexElements[] =
{   
    { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA,   0 }, 
    { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA,   0 }, 
    { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D11_INPUT_PER_VERTEX_DATA,   0 }, 
    { "Transform", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0,  D3D11_INPUT_PER_INSTANCE_DATA, 1 }, 
    { "Transform", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 }, 
    { "Transform", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 }, 
    { "Transform", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 }, 
};

static unsigned int         g_nElementCount = ARRAYSIZE( g_pStandardVertexElements );

// Utility mesh data //

float fMin = -gs_fWorldSize * 0.2f;
float fMax = gs_fWorldSize + gs_fWorldSize * 0.2f;
static STANDARD_VERTEX gs_pQuadVertices[] =
{   // Position                     // Normal                     // TexCoords
    { XMFLOAT3( fMin, 0.0f, fMax ), XMFLOAT3( 0.0f, 1.0f, 0.0f ), XMFLOAT2( 0.0f,       0.0f ) }, 
    { XMFLOAT3( fMax, 0.0f, fMax ), XMFLOAT3( 0.0f, 1.0f, 0.0f ), XMFLOAT2( gs_fWorldSize, 0.0f ) }, 
    { XMFLOAT3( fMin, 0.0f, fMin ), XMFLOAT3( 0.0f, 1.0f, 0.0f ), XMFLOAT2( 0.0f,          gs_fWorldSize ) }, 
    { XMFLOAT3( fMax, 0.0f, fMin ), XMFLOAT3( 0.0f, 1.0f, 0.0f ), XMFLOAT2( gs_fWorldSize, gs_fWorldSize ) }, 
};
static unsigned int         gs_nQuadVertexCount = ARRAYSIZE( gs_pQuadVertices );

static unsigned int gs_pQuadIndices[] =
{ 
    0, 1, 2, 
    2, 1, 3, 
};
static unsigned int         gs_nQuadIndexCount = ARRAYSIZE( gs_pQuadIndices );

static const float          gs_fBoxSide = gs_fTileSize/2.0f;
static STANDARD_VERTEX gs_pCubeVertices[] = 
{   // Position                                              // Normals                       // Texcoords
    { XMFLOAT3( -gs_fBoxSide, gs_fBoxHeight, -gs_fBoxSide ), XMFLOAT3(  0.0f,  1.0f,  0.0f ), XMFLOAT2( 0.0f, 0.0f ) },
    { XMFLOAT3(  gs_fBoxSide, gs_fBoxHeight, -gs_fBoxSide ), XMFLOAT3(  0.0f,  1.0f,  0.0f ), XMFLOAT2( 1.0f, 0.0f ) },
    { XMFLOAT3(  gs_fBoxSide, gs_fBoxHeight,  gs_fBoxSide ), XMFLOAT3(  0.0f,  1.0f,  0.0f ), XMFLOAT2( 0.0f, 1.0f ) },
    { XMFLOAT3( -gs_fBoxSide, gs_fBoxHeight,  gs_fBoxSide ), XMFLOAT3(  0.0f,  1.0f,  0.0f ), XMFLOAT2( 1.0f, 1.0f ) },
                                             
    { XMFLOAT3( -gs_fBoxSide, 0.0f,           gs_fBoxSide ), XMFLOAT3( -1.0f,  0.0f,  0.0f ), XMFLOAT2( 0.0f, 0.0f ) },
    { XMFLOAT3( -gs_fBoxSide, 0.0f,          -gs_fBoxSide ), XMFLOAT3( -1.0f,  0.0f,  0.0f ), XMFLOAT2( 1.0f, 0.0f ) },
    { XMFLOAT3( -gs_fBoxSide, gs_fBoxHeight, -gs_fBoxSide ), XMFLOAT3( -1.0f,  0.0f,  0.0f ), XMFLOAT2( 0.0f, 1.0f ) },
    { XMFLOAT3( -gs_fBoxSide, gs_fBoxHeight,  gs_fBoxSide ), XMFLOAT3( -1.0f,  0.0f,  0.0f ), XMFLOAT2( 1.0f, 1.0f ) },

    { XMFLOAT3(  gs_fBoxSide, 0.0f,           gs_fBoxSide ), XMFLOAT3(  1.0f,  0.0f,  0.0f ), XMFLOAT2( 0.0f, 0.0f ) },
    { XMFLOAT3(  gs_fBoxSide, 0.0f,          -gs_fBoxSide ), XMFLOAT3(  1.0f,  0.0f,  0.0f ), XMFLOAT2( 1.0f, 0.0f ) },
    { XMFLOAT3(  gs_fBoxSide, gs_fBoxHeight, -gs_fBoxSide ), XMFLOAT3(  1.0f,  0.0f,  0.0f ), XMFLOAT2( 0.0f, 1.0f ) },
    { XMFLOAT3(  gs_fBoxSide, gs_fBoxHeight,  gs_fBoxSide ), XMFLOAT3(  1.0f,  0.0f,  0.0f ), XMFLOAT2( 1.0f, 1.0f ) },

    { XMFLOAT3( -gs_fBoxSide, 0.0f,          -gs_fBoxSide ), XMFLOAT3(  0.0f,  0.0f, -1.0f ), XMFLOAT2( 0.0f, 0.0f ) },
    { XMFLOAT3(  gs_fBoxSide, 0.0f,          -gs_fBoxSide ), XMFLOAT3(  0.0f,  0.0f, -1.0f ), XMFLOAT2( 1.0f, 0.0f ) },
    { XMFLOAT3(  gs_fBoxSide, gs_fBoxHeight, -gs_fBoxSide ), XMFLOAT3(  0.0f,  0.0f, -1.0f ), XMFLOAT2( 0.0f, 1.0f ) },
    { XMFLOAT3( -gs_fBoxSide, gs_fBoxHeight, -gs_fBoxSide ), XMFLOAT3(  0.0f,  0.0f, -1.0f ), XMFLOAT2( 1.0f, 1.0f ) },

    { XMFLOAT3( -gs_fBoxSide, 0.0f,           gs_fBoxSide ), XMFLOAT3(  0.0f,  0.0f,  1.0f ), XMFLOAT2( 0.0f, 0.0f ) },
    { XMFLOAT3(  gs_fBoxSide, 0.0f,           gs_fBoxSide ), XMFLOAT3(  0.0f,  0.0f,  1.0f ), XMFLOAT2( 1.0f, 0.0f ) },
    { XMFLOAT3(  gs_fBoxSide, gs_fBoxHeight,  gs_fBoxSide ), XMFLOAT3(  0.0f,  0.0f,  1.0f ), XMFLOAT2( 0.0f, 1.0f ) },
    { XMFLOAT3( -gs_fBoxSide, gs_fBoxHeight,  gs_fBoxSide ), XMFLOAT3(  0.0f,  0.0f,  1.0f ), XMFLOAT2( 1.0f, 1.0f ) },
};

static unsigned int         gs_nCubeVertexCount = ARRAYSIZE( gs_pCubeVertices );

static unsigned int gs_pCubeIndices[] =
{ 
    3, 1, 0, 
    2, 1, 3,
    7, 5, 4, 
    6, 5, 7, 
    10, 8, 9, 
    11, 8, 10, 
    15, 13, 12, 
    14, 13, 15, 
    18, 16, 17,
    19, 16, 18 
};
static unsigned int         gs_nCubeIndexCount = ARRAYSIZE( gs_pCubeIndices );
////////////////////////////////////////////////////////////////////////////////


// ColonyMesh class definitions //

ColonyMesh::ColonyMesh( ID3D11Device* pDevice,
                        LPCWSTR sMeshFilename ) : m_pDevice( pDevice )
{
    HRESULT hr = S_OK;

    // Create the SDKMesh
    m_SDKMesh.Create( m_pDevice, sMeshFilename );

    // Extract vertex data & create vertex buffer
    m_nVertexStride = m_SDKMesh.GetVertexStride( 0, 0 );
    m_nVertexCount = ( unsigned int )m_SDKMesh.GetNumVertices( 0, 0 );

    D3D11_BUFFER_DESC VertexBufferDesc;
    VertexBufferDesc.ByteWidth = m_nVertexCount * m_nVertexStride;
    VertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    VertexBufferDesc.CPUAccessFlags = 0;
    VertexBufferDesc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA VertexBufferInitialData;
    VertexBufferInitialData.pSysMem = m_SDKMesh.GetRawVerticesAt( 0 );
    VertexBufferInitialData.SysMemPitch = 0;
    VertexBufferInitialData.SysMemSlicePitch = 0;

    V( m_pDevice->CreateBuffer( &VertexBufferDesc, &VertexBufferInitialData, &m_pVertexBuffer ) );

    // Extract index data & create index buffer
    m_nIndexFormat = m_SDKMesh.GetIBFormat11( 0 );
    m_nIndexCount = ( unsigned int )m_SDKMesh.GetNumIndices( 0 );

    D3D11_BUFFER_DESC IndexBufferDesc;
    IndexBufferDesc.ByteWidth = m_nIndexCount * sizeof( unsigned int );
    IndexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    IndexBufferDesc.CPUAccessFlags = 0;
    IndexBufferDesc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA IndexBufferInitialData;
    IndexBufferInitialData.pSysMem = m_SDKMesh.GetRawIndicesAt( 0 );
    IndexBufferInitialData.SysMemPitch = 0;
    IndexBufferInitialData.SysMemSlicePitch = 0;

    V( m_pDevice->CreateBuffer( &IndexBufferDesc, &IndexBufferInitialData, &m_pIndexBuffer ) );

    // Destroy the SDKMesh
    m_SDKMesh.Destroy();
}
ColonyMesh::ColonyMesh( ID3D11Device* pDevice,
                        const void* pVertices,
                        unsigned int nVertexCount,
                        unsigned int nVertexStride,
                        const void* pIndices,
                        unsigned int nIndexCount,
                        DXGI_FORMAT nIndexFormat ) : m_pDevice( pDevice )
{
    HRESULT hr = S_OK;

    // Extract vertex data & create vertex buffer
    m_nVertexStride = nVertexStride;
    m_nVertexCount = nVertexCount;

    D3D11_BUFFER_DESC VertexBufferDesc;
    VertexBufferDesc.ByteWidth = m_nVertexCount * m_nVertexStride;
    VertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    VertexBufferDesc.CPUAccessFlags = 0;
    VertexBufferDesc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA VertexBufferInitialData;
    VertexBufferInitialData.pSysMem = pVertices;
    VertexBufferInitialData.SysMemPitch = 0;
    VertexBufferInitialData.SysMemSlicePitch = 0;

    V( m_pDevice->CreateBuffer( &VertexBufferDesc, &VertexBufferInitialData, &m_pVertexBuffer ) );

    // Extract index data & create index buffer
    m_nIndexFormat = nIndexFormat;
    m_nIndexCount = nIndexCount;

    D3D11_BUFFER_DESC IndexBufferDesc;
    IndexBufferDesc.ByteWidth = m_nIndexCount * sizeof( unsigned int );
    IndexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    IndexBufferDesc.CPUAccessFlags = 0;
    IndexBufferDesc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA IndexBufferInitialData;
    IndexBufferInitialData.pSysMem = pIndices;
    IndexBufferInitialData.SysMemPitch = 0;
    IndexBufferInitialData.SysMemSlicePitch = 0;

    V( m_pDevice->CreateBuffer( &IndexBufferDesc, &IndexBufferInitialData, &m_pIndexBuffer ) );
}
ColonyMesh::~ColonyMesh( void )
{
    SAFE_RELEASE( m_pVertexBuffer );
    SAFE_RELEASE( m_pIndexBuffer );
}


// Render class definitions //

HRESULT Render::Init()
{
    HRESULT hr = S_OK;
    ID3DBlob* pVSBlob;
    ID3DBlob* pErrBlob;
    ID3DBlob* pPSBlob;
    WCHAR sFilename[ MAX_PATH ];

    m_pDevice = DXUTGetD3D11Device();
    m_pContext = DXUTGetD3D11DeviceContext();

    // Colony meshes
    m_pMeshes[ UNIT_MESH ] = new ColonyMesh( m_pDevice, L"drone.sdkmesh" );
    m_pMeshes[ FACTORY_MESH ] = new ColonyMesh( m_pDevice, L"factory.sdkmesh" );
    m_pMeshes[ TREE_MESH ] = new ColonyMesh( m_pDevice, L"tree.sdkmesh" );
    m_pMeshes[ CONCRETE_MESH ] = new ColonyMesh( m_pDevice, gs_pCubeVertices,
                                                 gs_nCubeVertexCount, sizeof( STANDARD_VERTEX ), gs_pCubeIndices,
                                                 gs_nCubeIndexCount, DXGI_FORMAT_R32_UINT );
    m_pMeshes[ GRASS_MESH ] = new ColonyMesh( m_pDevice, gs_pQuadVertices,
                                              gs_nQuadVertexCount, sizeof( STANDARD_VERTEX ), gs_pQuadIndices,
                                              gs_nQuadIndexCount, DXGI_FORMAT_R32_UINT );
    m_pMeshes[ SKY_MESH ] = new ColonyMesh( m_pDevice, L"sky.sdkmesh" );

    // Init textures & texture sampler
    V_RETURN( DXUTFindDXSDKMediaFileCch( sFilename, MAX_PATH, L"dronediffuse.dds" ) );
    V_RETURN( D3DX11CreateShaderResourceViewFromFile( m_pDevice, sFilename, 0, 0, &m_pMeshTextures[UNIT_DIFFUSE], 0 ) );

    V_RETURN( DXUTFindDXSDKMediaFileCch( sFilename, MAX_PATH, L"factorydiffuse.jpg" ) );
    V_RETURN( D3DX11CreateShaderResourceViewFromFile( m_pDevice, sFilename, 0, 0, &m_pMeshTextures[FACTORY_DIFFUSE], 0 ) );

    V_RETURN( DXUTFindDXSDKMediaFileCch( sFilename, MAX_PATH, L"treediffuse.dds" ) );
    V_RETURN( D3DX11CreateShaderResourceViewFromFile( m_pDevice, sFilename, 0, 0, &m_pMeshTextures[TREE_DIFFUSE], 0 ) );

    V_RETURN( DXUTFindDXSDKMediaFileCch( sFilename, MAX_PATH, L"concrete.dds" ) );
    V_RETURN( D3DX11CreateShaderResourceViewFromFile( m_pDevice, sFilename, 0, 0, &m_pMeshTextures[CONCRETE_DIFFUSE], 0 ) );

    V_RETURN( DXUTFindDXSDKMediaFileCch( sFilename, MAX_PATH, L"grass.dds" ) );
    V_RETURN( D3DX11CreateShaderResourceViewFromFile( m_pDevice, sFilename, 0, 0, &m_pMeshTextures[GRASS_DIFFUSE], 0 ) );

    V_RETURN( DXUTFindDXSDKMediaFileCch( sFilename, MAX_PATH, L"sky.png" ) );
    V_RETURN( D3DX11CreateShaderResourceViewFromFile( m_pDevice, sFilename, 0, 0, &m_pMeshTextures[SKY_DIFFUSE], 0 ) );

    V_RETURN( DXUTFindDXSDKMediaFileCch( sFilename, MAX_PATH, L"skyDark.dds" ) );
    V_RETURN( D3DX11CreateShaderResourceViewFromFile( m_pDevice, sFilename, 0, 0, &m_pMeshTextures[SKY_DARK_DIFFUSE], 0 ) );


    D3D11_SAMPLER_DESC samplerDesc;
    ZeroMemory( &samplerDesc, sizeof( samplerDesc ) );
    samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.MaxAnisotropy = 16;
    V_RETURN( m_pDevice->CreateSamplerState( &samplerDesc, &m_pSamplerState ) );

    // load shaders
    V_RETURN( DXUTFindDXSDKMediaFileCch( sFilename, MAX_PATH, L"Colony.hlsl" ) );

    // define vertex shader
    V_RETURN( D3DX11CompileFromFile( sFilename, NULL, NULL, "VS_Instanced", "vs_4_0", D3DCOMPILE_ENABLE_STRICTNESS,
                                     NULL, NULL, &pVSBlob, &pErrBlob, NULL ) );
    V_RETURN( m_pDevice->CreateVertexShader( pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(),
                                             NULL, &m_pVertexShader ) );

    // define pixel shaders
    V_RETURN( D3DX11CompileFromFile( sFilename, NULL, NULL, "PS", "ps_4_0", D3DCOMPILE_ENABLE_STRICTNESS, NULL,
                                     NULL, &pPSBlob, &pErrBlob, NULL ) );
    V_RETURN( m_pDevice->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(),
                                            NULL, &m_pPixelShader ) );

	V_RETURN( D3DX11CompileFromFile( sFilename, NULL, NULL, "PS_RED", "ps_4_0", D3DCOMPILE_ENABLE_STRICTNESS, NULL,
                                     NULL, &pPSBlob, &pErrBlob, NULL ) );
    V_RETURN( m_pDevice->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(),
                                            NULL, &m_pPixelShaderRED ) );

	V_RETURN( D3DX11CompileFromFile( sFilename, NULL, NULL, "PS_GREEN", "ps_4_0", D3DCOMPILE_ENABLE_STRICTNESS, NULL,
                                     NULL, &pPSBlob, &pErrBlob, NULL ) );
    V_RETURN( m_pDevice->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(),
                                            NULL, &m_pPixelShaderGREEN ) );

	V_RETURN( D3DX11CompileFromFile( sFilename, NULL, NULL, "PS_BLUE", "ps_4_0", D3DCOMPILE_ENABLE_STRICTNESS, NULL,
                                     NULL, &pPSBlob, &pErrBlob, NULL ) );
    V_RETURN( m_pDevice->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(),
                                            NULL, &m_pPixelShaderBLUE ) );

	V_RETURN( D3DX11CompileFromFile( sFilename, NULL, NULL, "PS_YELLOW", "ps_4_0", D3DCOMPILE_ENABLE_STRICTNESS, NULL,
                                     NULL, &pPSBlob, &pErrBlob, NULL ) );
    V_RETURN( m_pDevice->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(),
                                            NULL, &m_pPixelShaderYELLOW ) );

	V_RETURN( D3DX11CompileFromFile( sFilename, NULL, NULL, "PS_CYAN", "ps_4_0", D3DCOMPILE_ENABLE_STRICTNESS, NULL,
                                     NULL, &pPSBlob, &pErrBlob, NULL ) );
    V_RETURN( m_pDevice->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(),
                                            NULL, &m_pPixelShaderCYAN ) );

	V_RETURN( D3DX11CompileFromFile( sFilename, NULL, NULL, "PS_PURPLE", "ps_4_0", D3DCOMPILE_ENABLE_STRICTNESS, NULL,
                                     NULL, &pPSBlob, &pErrBlob, NULL ) );
    V_RETURN( m_pDevice->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(),
                                            NULL, &m_pPixelShaderPURPLE ) );

	V_RETURN( D3DX11CompileFromFile( sFilename, NULL, NULL, "PS_BLACK", "ps_4_0", D3DCOMPILE_ENABLE_STRICTNESS, NULL,
                                     NULL, &pPSBlob, &pErrBlob, NULL ) );
    V_RETURN( m_pDevice->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(),
                                            NULL, &m_pPixelShaderBLACK ) );

    // define sky pixel shader
    V_RETURN( D3DX11CompileFromFile( sFilename, NULL, NULL, "PS_Sky", "ps_4_0", D3DCOMPILE_ENABLE_STRICTNESS, NULL,
                                     NULL, &pPSBlob, &pErrBlob, NULL ) );
    V_RETURN( m_pDevice->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(),
                                            NULL, &m_pSkyPixelShader ) );

    // define input layout
    V_RETURN( m_pDevice->CreateInputLayout( g_pStandardVertexElements, g_nElementCount, pVSBlob->GetBufferPointer(),
                                            pVSBlob->GetBufferSize(), &m_pInputLayout ) );


    // init instance buffers
    D3D11_BUFFER_DESC bufferDesc =
    { 0 };
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bufferDesc.MiscFlags = 0;
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.ByteWidth = gs_nMaxUnits * sizeof( XMMATRIX );
    V_RETURN( m_pDevice->CreateBuffer( &bufferDesc, 0, &( m_pInstanceBuffers[UNIT_MESH] ) ) );
    bufferDesc.ByteWidth = gs_nMaxFactories * sizeof( XMMATRIX );
    V_RETURN( m_pDevice->CreateBuffer( &bufferDesc, 0, &( m_pInstanceBuffers[FACTORY_MESH] ) ) );
    bufferDesc.ByteWidth = gs_nWorldSizeSq * sizeof( XMMATRIX );
    V_RETURN( m_pDevice->CreateBuffer( &bufferDesc, 0, &( m_pInstanceBuffers[TREE_MESH] ) ) );
    V_RETURN( m_pDevice->CreateBuffer( &bufferDesc, 0, &( m_pInstanceBuffers[CONCRETE_MESH] ) ) );
    bufferDesc.ByteWidth = sizeof( XMMATRIX );
    V_RETURN( m_pDevice->CreateBuffer( &bufferDesc, 0, &( m_pInstanceBuffers[GRASS_MESH] ) ) );
    V_RETURN( m_pDevice->CreateBuffer( &bufferDesc, 0, &( m_pInstanceBuffers[SKY_MESH] ) ) );

    return hr;
}

void Render::DrawInstanced( XMMATRIX* pTransforms,
                            MeshType Type,
                            unsigned int nInstanceCount,
                            bool bUpdateTransforms,
                            bool bCullObjects, 
							ColorFilter filter)
{
    GPA_SCOPED_TASK( __FUNCTION__, s_pRenderDomain );

    if( nInstanceCount == 0 )
        return;

    ColonyMesh* pInstancedMesh = m_pMeshes[Type];
    unsigned int nVertexStride = pInstancedMesh->GetVertexStride();
    ID3D11Buffer* pVertexBuffer = pInstancedMesh->GetVertexBuffer();
    ID3D11Buffer* pIndexBuffer = pInstancedMesh->GetIndexBuffer();
    DXGI_FORMAT IndexFormat = pInstancedMesh->GetIndexFormat();
    unsigned int nIndexCount = pInstancedMesh->GetIndexCount();
    ID3D11ShaderResourceView* pTextureResourceView = m_pMeshTextures[Type];
    ID3D11Buffer* pInstanceBuffer = m_pInstanceBuffers[Type];

    m_pContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
    m_pContext->IASetInputLayout( m_pInputLayout );
    m_pContext->VSSetShader( m_pVertexShader, NULL, 0 );
<<<<<<< HEAD
	
	//Color filters
=======
>>>>>>> b561bf0b9d813c3849199d2267141d1bba01da47
	switch(filter)
	{
	case ColorFilter::WHITE:
		m_pContext->PSSetShader( m_pPixelShader, NULL, 0 );
		break;
	case ColorFilter::RED:
		m_pContext->PSSetShader( m_pPixelShaderRED, NULL, 0 );
		break;
	case ColorFilter::GREEN:
		m_pContext->PSSetShader( m_pPixelShaderGREEN, NULL, 0 );
		break;
	case ColorFilter::BLUE:
		m_pContext->PSSetShader( m_pPixelShaderBLUE, NULL, 0 );
		break;
	case ColorFilter::PURPLE:
		m_pContext->PSSetShader( m_pPixelShaderPURPLE, NULL, 0 );
		break;
	case ColorFilter::YELLOW:
		m_pContext->PSSetShader( m_pPixelShaderYELLOW, NULL, 0 );
		break;
	case ColorFilter::CYAN:
		m_pContext->PSSetShader( m_pPixelShaderCYAN, NULL, 0 );
		break;
	case ColorFilter::BLACK:
		m_pContext->PSSetShader( m_pPixelShaderBLACK, NULL, 0 );
		break;
	}
    
    m_pContext->PSSetSamplers( 0, 1, &m_pSamplerState );

    UINT Strides[2] =
    { nVertexStride, sizeof( XMMATRIX ) };
    UINT Offsets[2] =
    { 0, 0 };
    ID3D11Buffer* pVB[2] =
    { pVertexBuffer, pInstanceBuffer };

    if( bCullObjects )
    {
        unsigned int nVisibleCount = 0;
        D3D11_MAPPED_SUBRESOURCE MappedResource;
        for( unsigned int i = 0; i < nInstanceCount; i++ )
        {
            XMVECTOR vPosition = XMVectorSet( pTransforms[i]._41, pTransforms[i]._42, pTransforms[i]._43, 1.0f );
            XMVECTOR vCamToObject = vPosition - m_vCamPos;
            XMVECTOR vCamToObjectNormalized = XMVector4NormalizeEst( vCamToObject );

            if( XMVectorGetX( XMVector4Dot( vCamToObjectNormalized, m_vCamDir ) ) > m_fCamFOV )
            {
                m_pVisible[nVisibleCount++] = pTransforms[i];
            }
        }

        m_nObjectsRendered = nVisibleCount;

        if( nVisibleCount > 0 )
        {
            m_pContext->Map( pInstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
            XMMATRIX* pMatrices = ( XMMATRIX* )MappedResource.pData;
            memcpy_s( pMatrices, gs_nMaxPerDraw * sizeof( XMMATRIX ), m_pVisible, nVisibleCount * sizeof( XMMATRIX ) );
            m_pContext->Unmap( pInstanceBuffer, 0 );

            m_pContext->IASetVertexBuffers( 0, 2, pVB, Strides, Offsets );
            m_pContext->IASetIndexBuffer( pIndexBuffer, IndexFormat, 0 );
            m_pContext->PSSetShaderResources( 0, 1, &pTextureResourceView );
            m_pContext->DrawIndexedInstanced( nIndexCount, nVisibleCount, 0, 0, 0 );
        }
    }
    else
    {
        m_nObjectsRendered = nInstanceCount;
        if( bUpdateTransforms )
        {
            D3D11_MAPPED_SUBRESOURCE MappedResource;
            m_pContext->Map( pInstanceBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
            XMMATRIX* pMatrices = ( XMMATRIX* )MappedResource.pData;
            memcpy_s( pMatrices, gs_nMaxPerDraw * sizeof( XMMATRIX ), pTransforms,
                      nInstanceCount * sizeof( XMMATRIX ) );
            m_pContext->Unmap( pInstanceBuffer, 0 );
        }
        m_pContext->IASetVertexBuffers( 0, 2, pVB, Strides, Offsets );
        m_pContext->IASetIndexBuffer( pIndexBuffer, IndexFormat, 0 );
        m_pContext->PSSetShaderResources( 0, 1, &pTextureResourceView );
        m_pContext->DrawIndexedInstanced( nIndexCount, nInstanceCount, 0, 0, 0 );
    }
}

void Render::DrawTerrain( )
{
    GPA_SCOPED_TASK( __FUNCTION__, s_pRenderDomain );


    // Then draw
    ColonyMesh* pInstancedMesh = m_pMeshes[GRASS_MESH];
    unsigned int nVertexStride = pInstancedMesh->GetVertexStride();
    ID3D11Buffer* pVertexBuffer = pInstancedMesh->GetVertexBuffer();
    ID3D11Buffer* pIndexBuffer = pInstancedMesh->GetIndexBuffer();
    DXGI_FORMAT nIndexFormat = pInstancedMesh->GetIndexFormat();
    unsigned int nIndexCount = pInstancedMesh->GetIndexCount();
    ID3D11ShaderResourceView* pTexture = m_pMeshTextures[GRASS_DIFFUSE];




    m_pContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
    m_pContext->IASetInputLayout( m_pInputLayout );
    m_pContext->VSSetShader( m_pVertexShader, NULL, 0 );
    
	
	//old concrete shader
	m_pContext->PSSetShader( m_pPixelShader, NULL, 0 );	
    
    m_pContext->PSSetSamplers( 0, 1, &m_pSamplerState );

    // Quad to extend terrain past skydome
    UINT Strides[2] =
    { nVertexStride, sizeof( XMMATRIX ) };
    UINT Offsets[2] =
    { 0, 0 };

    static bool bUpdateTransforms = true;
    if( bUpdateTransforms )
    {
        static XMMATRIX mTransforms = XMMatrixTranslation( 0.0f, 0.0f, 0.0f );
        D3D11_MAPPED_SUBRESOURCE MappedResource;
        m_pContext->Map( m_pInstanceBuffers[GRASS_MESH], 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
        XMMATRIX* pMatrices = ( XMMATRIX* )MappedResource.pData;
        memcpy_s( pMatrices, sizeof( XMMATRIX ), &mTransforms, sizeof( XMMATRIX ) );
        m_pContext->Unmap( m_pInstanceBuffers[GRASS_MESH], 0 );
        bUpdateTransforms = false;
    }
    ID3D11Buffer* pVB[2] =
    { pVertexBuffer, m_pInstanceBuffers[GRASS_MESH] };
    m_pContext->IASetVertexBuffers( 0, 2, pVB, Strides, Offsets );
    m_pContext->IASetIndexBuffer( pIndexBuffer, nIndexFormat, 0 );
    m_pContext->PSSetShaderResources( 0, 1, &pTexture );
    m_pContext->DrawIndexed( nIndexCount, 0, 0 );

}

void Render::Destroy( void )
{
    SAFE_DELETE( m_pMeshes[ UNIT_MESH ] );
    SAFE_DELETE( m_pMeshes[ FACTORY_MESH ] );
    SAFE_DELETE( m_pMeshes[ TREE_MESH ] );
    SAFE_DELETE( m_pMeshes[ CONCRETE_MESH ] );
    SAFE_DELETE( m_pMeshes[ GRASS_MESH ] );
    SAFE_DELETE( m_pMeshes[ SKY_MESH ] );

    SAFE_RELEASE( m_pSamplerState );
    SAFE_RELEASE( m_pInputLayout );
    SAFE_RELEASE( m_pVertexShader );
    SAFE_RELEASE( m_pPixelShader );
	SAFE_RELEASE( m_pPixelShaderRED );
	SAFE_RELEASE( m_pPixelShaderGREEN );
	SAFE_RELEASE( m_pPixelShaderBLUE );
	SAFE_RELEASE( m_pPixelShaderYELLOW );
	SAFE_RELEASE( m_pPixelShaderPURPLE );
	SAFE_RELEASE( m_pPixelShaderCYAN );
	SAFE_RELEASE( m_pPixelShaderBLACK );
    SAFE_RELEASE( m_pSkyPixelShader );

    SAFE_RELEASE( m_pMeshTextures[ UNIT_DIFFUSE ] );
    SAFE_RELEASE( m_pMeshTextures[ FACTORY_DIFFUSE ] );
    SAFE_RELEASE( m_pMeshTextures[ TREE_DIFFUSE ] );
    SAFE_RELEASE( m_pMeshTextures[ CONCRETE_DIFFUSE ] );
    SAFE_RELEASE( m_pMeshTextures[ GRASS_DIFFUSE ] );
    SAFE_RELEASE( m_pMeshTextures[ SKY_DIFFUSE ] );
    SAFE_RELEASE( m_pMeshTextures[ SKY_DARK_DIFFUSE ] );

    SAFE_RELEASE( m_pInstanceBuffers[ UNIT_MESH ] );
    SAFE_RELEASE( m_pInstanceBuffers[ FACTORY_MESH ] );
    SAFE_RELEASE( m_pInstanceBuffers[ TREE_MESH ] );
    SAFE_RELEASE( m_pInstanceBuffers[ CONCRETE_MESH ] );
    SAFE_RELEASE( m_pInstanceBuffers[ GRASS_MESH ] );
    SAFE_RELEASE( m_pInstanceBuffers[ SKY_MESH ] );
}

void Render::SetCamera( const XMVECTOR& vCamPos,
                        const XMVECTOR& vCamDir,
                        const float fCamFOV )
{
    m_vCamPos = vCamPos;
    m_vCamDir = vCamDir;
    m_fCamFOV = cos( fCamFOV );
}

void Render::DrawSky( void )
{
    GPA_SCOPED_TASK( __FUNCTION__, s_pRenderDomain );

    ID3D11ShaderResourceView* pLightTexture = m_pMeshTextures[ SKY_DIFFUSE ];
    ID3D11ShaderResourceView* pDarktTexture = m_pMeshTextures[ SKY_DARK_DIFFUSE ];
    ColonyMesh* pInstancedMesh = m_pMeshes[ SKY_MESH ];
    unsigned int nVertexStride = pInstancedMesh->GetVertexStride();
    ID3D11Buffer* pVertexBuffer = pInstancedMesh->GetVertexBuffer();
    ID3D11Buffer* pIndexBuffer = pInstancedMesh->GetIndexBuffer();
    DXGI_FORMAT nIndexFormat = pInstancedMesh->GetIndexFormat();
    unsigned int nIndexCount = pInstancedMesh->GetIndexCount();

    m_pContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
    m_pContext->IASetInputLayout( m_pInputLayout );
    m_pContext->VSSetShader( m_pVertexShader, NULL, 0 );
    m_pContext->PSSetShader( m_pSkyPixelShader, NULL, 0 );
    m_pContext->PSSetSamplers( 0, 1, &m_pSamplerState );

    UINT Strides[2] =
    { nVertexStride, sizeof( XMMATRIX ) };
    UINT Offsets[2] =
    { 0, 0 };

    static bool bUpdateTransforms = true;
    if( bUpdateTransforms )
    {
        static XMMATRIX mTransforms = XMMatrixScaling( gs_fTileSize * 3.6f, gs_fTileSize * 3.6f, gs_fTileSize * 3.6f ) 
                                     * XMMatrixTranslation( gs_fWorldSize / 2, 0.0f, gs_fWorldSize / 2 );

        D3D11_MAPPED_SUBRESOURCE MappedResource;
        m_pContext->Map( m_pInstanceBuffers[SKY_MESH], 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
        XMMATRIX* pMatrices = ( XMMATRIX* )MappedResource.pData;
        memcpy_s( pMatrices, sizeof( XMMATRIX ), &mTransforms, sizeof( XMMATRIX ) );
        m_pContext->Unmap( m_pInstanceBuffers[SKY_MESH], 0 );
        bUpdateTransforms = false;
    }
    ID3D11Buffer* pVB[2] =
    { pVertexBuffer, m_pInstanceBuffers[SKY_MESH] };

    m_pContext->IASetVertexBuffers( 0, 2, pVB, Strides, Offsets );
    m_pContext->IASetIndexBuffer( pIndexBuffer, nIndexFormat, 0 );
    m_pContext->PSSetShaderResources( 1, 1, &pLightTexture );
    m_pContext->PSSetShaderResources( 2, 1, &pDarktTexture );
    m_pContext->DrawIndexed( nIndexCount, 0, 0 );
}

