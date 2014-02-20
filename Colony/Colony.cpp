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
//--------------------------------------------------------------------------------------
// Keyboard:
// Show/hide trees               - J
// (un)Pause game update         - P
// (un)Follow unit               - F
// Turn SIMD on/off              - C
// Turn TBB on/off               - T
// Turn Static unit count on/off - U
// Reset simulation              - R
// Toggle rendering              - K
// Toggle HUD display            - H
// Toggle computing while render - M
//
// Mouse:
// Move camera              - Hold left button
// 
//--------------------------------------------------------------------------------------

#include "Colony.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "SDKmisc.h"
#include "SampleComponents.h"
#include "Render.h"
#include "Game.h"
#include "TaskMgrTBB.h"

//--------------------------------------------------------------------------------------
// Variable declarations
//--------------------------------------------------------------------------------------
ID3D11Buffer*               g_pCBViewProj = 0;
CFirstPersonCamera          g_Camera;
CDXUTDialogResourceManager  g_ResourceManager;
CDXUTTextHelper*            g_pTextWriter = 0;

Game                        g_Game;

bool                        g_bPaused = false;
bool                        g_bRender = true;
bool                        g_bShowHUD = true;
bool                        g_bUseSIMD = true;
bool                        g_bThreaded = true;
bool                        g_bComputeAcrossFrames = true;
bool                        g_bStaticUnitCount = false;
bool                        g_bRenderTrees = true;

int                         g_nStaticUnitCount = false;

typedef struct _PER_FRAME_CB
{
    D3DXMATRIX mViewProj;
    D3DXVECTOR4 vParams;   // coverage
}                           PER_FRAME_CB;

//--------------------------------------------------------------------------------------
// DXUT function declarations
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings,
                                    void* pUserContext );
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice,
                                      const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext );
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice,
                                          IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                          void* pUserContext );
void CALLBACK OnFrameMove( double fTime,
                           float fElapsedTime,
                           void* pUserContext );
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice,
                                  ID3D11DeviceContext* pd3dImmediateContext,
                                  double fTime,
                                  float fElapsedTime,
                                  void* pUserContext );
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D11DestroyDevice( void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd,
                          UINT uMsg,
                          WPARAM wParam,
                          LPARAM lParam,
                          bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar,
                          bool bKeyDown,
                          bool bAltDown,
                          void* pUserContext );
void CALLBACK OnMouse( bool bLeftButtonDown,
                       bool bRightButtonDown,
                       bool bMiddleButtonDown,
                       bool bSideButton1Down,
                       bool bSideButton2Down,
                       int nMouseWheelDelta,
                       int xPos,
                       int yPos,
                       void* pUserContext );
int WINAPI wWinMain( HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPWSTR lpCmdLine,
                     int nCmdShow );


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D11 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings,
                                    void* pUserContext )
{
    return true;
}

//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice,
                                      const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr;

    // define camera
    D3DXVECTOR3 EyePt( gs_fWorldSize / 2, 1.0f, gs_fWorldSize / 2 - 3.0f );
    D3DXVECTOR3 LookAtPt( gs_fWorldSize / 2, 0.0f, gs_fWorldSize / 2 );
    g_Camera.SetViewParams( &EyePt, &LookAtPt );
    g_Camera.SetEnablePositionMovement( true );
    g_Camera.SetEnableYAxisMovement( true );
    g_Camera.SetScalers( 0.005f, 5.0f );
    g_Camera.SetRotateButtons( true, false, false, false );
    D3DXVECTOR3 vMin( 1.5f, 0.1f, 1.5f );
    D3DXVECTOR3 vMax( gs_fWorldSize - 1.5f, 8.0f, gs_fWorldSize - 1.5f );
    g_Camera.SetClipToBoundary( true, &vMin, &vMax );

    // Create shader constant buffers
    D3D11_BUFFER_DESC ViewProjBufferDesc =
    { 0 };
    ViewProjBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    ViewProjBufferDesc.ByteWidth = sizeof( PER_FRAME_CB );
    ViewProjBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ViewProjBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    V_RETURN( pd3dDevice->CreateBuffer( &ViewProjBufferDesc, 0, &g_pCBViewProj ) );

    // Initialize the DXUT resource manager
    V_RETURN( g_ResourceManager.OnD3D11CreateDevice( pd3dDevice, DXUTGetD3D11DeviceContext() ) );

    // Initialize the DXUT text writer
    g_pTextWriter = new CDXUTTextHelper( pd3dDevice, DXUTGetD3D11DeviceContext(), &g_ResourceManager, 15 );

    // Initialize the renderer
    Render::Init();

    // Initialize the game
    g_Game.Initialize();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice,
                                          IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                          void* pUserContext )
{
    // Resize camera
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.01f, 2000.0f );

    // Update resource manager
    g_ResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc );

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime,
                           float fElapsedTime,
                           void* pUserContext )
{
    g_Camera.FrameMove( fElapsedTime );

    if( !g_bPaused )
    {
        if( !g_bStaticUnitCount )
        {
            // Modulate unit count based on framerate
            if( fElapsedTime < ( 1.0f / ( gs_nTargetFPS + 1 ) ) )
            {
                g_Game.GetUnitManager()->AddUnit();
            }
            else if( fElapsedTime > ( 1.0f / ( gs_nTargetFPS - 1 ) ) )
            {
                // Remove faster than we add
                for( int i = 0; i < ( int )( fElapsedTime * 120.0f ); ++i )
                {
                    g_Game.GetUnitManager()->RemoveUnit();
                }
            }
        }
        else
        {
            g_Game.GetUnitManager()->SetUnitCount( g_nStaticUnitCount );
        }

        // Perform game updating
        g_Game.Update( ( float )fTime, fElapsedTime );

    }
}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice,
                                  ID3D11DeviceContext* pd3dImmediateContext,
                                  double fTime,
                                  float fElapsedTime,
                                  void* pUserContext )
{
    HRESULT hr;

    // Clear render target and the depth stencil 
    float ClearColor[4] =
    { 0.89f, 0.97f, 1.0f, 0.0f };

    ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
    ID3D11DepthStencilView* pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearRenderTargetView( pRTV, ClearColor );
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

    ///////////////////////////////////////////
    // Update the camera matrix
    D3DXMATRIX mViewProj = *g_Camera.GetViewMatrix() * *g_Camera.GetProjMatrix();

    // Update the VS per-scene constant data
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    V( pd3dImmediateContext->Map( g_pCBViewProj, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    PER_FRAME_CB* pPerFrameCB = ( PER_FRAME_CB* )MappedResource.pData;
    D3DXMatrixTranspose( &pPerFrameCB->mViewProj, &mViewProj );
    pPerFrameCB->vParams = D3DXVECTOR4( g_Game.GetCoverage(), 0.0f, 0.0f, 0.0f );
    pd3dImmediateContext->Unmap( g_pCBViewProj, 0 );
    pd3dImmediateContext->VSSetConstantBuffers( 0, 1, &g_pCBViewProj );


    // Update the renderer
    Render::SetCamera( XMVectorSet( g_Camera.GetEyePt()->x, g_Camera.GetEyePt()->y, g_Camera.GetEyePt()->z, 1.0f ),
                       XMVectorSet( g_Camera.GetWorldAhead()->x, g_Camera.GetWorldAhead()->y,
                                    g_Camera.GetWorldAhead()->z, 1.0f ) );

    // Render the game
    if( g_bRender )
    {
        // Render the game
        g_Game.Render();
    }


    // Render the HUD
    if( g_bShowHUD )
    {
        g_pTextWriter->SetForegroundColor( D3DCOLOR_XRGB( 100, 100, 0 ) );
        g_pTextWriter->Begin();
        g_pTextWriter->SetInsertionPos( 0, 0 );
        g_pTextWriter->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
        g_pTextWriter->DrawTextLine( DXUTGetDeviceStats() );

        g_pTextWriter->DrawTextLine( L"---------------" );
        g_pTextWriter->DrawFormattedTextLine( L"Frame time: %3.2f ms", fElapsedTime * 1000.0f );
        g_pTextWriter->DrawFormattedTextLine( L"Units: %d", g_Game.GetUnitManager()->GetNumUnits() );
        g_pTextWriter->DrawFormattedTextLine( L"Progress: %3.2f%%", g_Game.GetCoverage() * 100.0f );
        g_pTextWriter->DrawFormattedTextLine( L"[C] SIMD: %d", g_bUseSIMD ? 1 : 0 );
        g_pTextWriter->DrawFormattedTextLine( L"[T] TBB: %d", g_bThreaded ? 1 : 0 );
        g_pTextWriter->DrawFormattedTextLine( L"[M] Compute across frames: %d", g_bComputeAcrossFrames ? 1 : 0 );
        g_pTextWriter->DrawFormattedTextLine( L"[U] Static unit count: %d", g_bStaticUnitCount ? 1 : 0 );
        g_pTextWriter->End();
    }
}

//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_ResourceManager.OnD3D11ReleasingSwapChain();
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    g_ResourceManager.OnD3D11DestroyDevice();
    SAFE_DELETE( g_pTextWriter );
    SAFE_RELEASE( g_pCBViewProj );

    // Clean up the game
    g_Game.GetUnitManager()->StopWork();
}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd,
                          UINT uMsg,
                          WPARAM wParam,
                          LPARAM lParam,
                          bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
    *pbNoFurtherProcessing = g_ResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );
    return 0;
}

//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar,
                          bool bKeyDown,
                          bool bAltDown,
                          void* pUserContext )
{
    if( !bKeyDown )
    {
        switch( nChar )
        {
        case 'J':
            {
                g_bRenderTrees = !g_bRenderTrees;
                break;
            }
        case 'P':
            {
                g_bPaused = !g_bPaused;
                break;
            }
        case 'C':
            {
                g_bUseSIMD = !g_bUseSIMD;
                break;
            }
        case 'T':
            {
                g_bThreaded = !g_bThreaded;
                break;
            }
        case 'U':
            {
                g_bStaticUnitCount = !g_bStaticUnitCount;
                g_nStaticUnitCount = g_Game.GetUnitManager()->GetNumUnits() / gs_nSIMDWidth;
                break;
            }
        case 'R':
            {
                g_Game.Reset();
                break;
            }
        case 'K':
            {
                g_bRender = !g_bRender;
                break;
            }
        case 'M':
            {
                g_bComputeAcrossFrames = !g_bComputeAcrossFrames;
                break;
            }
        }
    }
    else // if( !bKeyDown )
    {
        switch( nChar )
        {
        case VK_SUBTRACT:
            {
                g_nStaticUnitCount--;
                break;
            }
        case VK_ADD:
            {
                g_nStaticUnitCount++;
                break;
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// Handle mouse button presses
//--------------------------------------------------------------------------------------
void CALLBACK OnMouse( bool bLeftButtonDown,
                       bool bRightButtonDown,
                       bool bMiddleButtonDown,
                       bool bSideButton1Down,
                       bool bSideButton2Down,
                       int nMouseWheelDelta,
                       int xPos,
                       int yPos,
                       void* pUserContext ) {}


//--------------------------------------------------------------------------------------
// Initialize everything and start main loop
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPWSTR lpCmdLine,
                     int nCmdShow )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // Seed the random number generator
    srand( GetTickCount() );

    // DXUT will create and use the best device (either D3D9 or D3D11) 
    // that is available on the system depending on which D3D callbacks are set below

    // Set general DXUT callbacks
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackMouse( OnMouse, true );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );

    // Set the D3D11 DXUT callbacks. Remove these sets if the app doesn't need to support D3D11
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    // Perform any application-level initialization here
    DXUTSetD3DVersionSupport( false, true );
    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line 

    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"Colony" );

    // Only require 10-level hardware
    HRESULT hr = DXUTCreateDevice( D3D_FEATURE_LEVEL_10_0, true, 1024, 768 );
    if( FAILED( hr ) || DXUTGetDeviceSettings().ver == DXUT_D3D9_DEVICE )
    {
        MessageBox( 0, L"Colony requres at least Windows Vista with a Direct3D 10 capable GPU.", L"Failure", 0 );
        DXUTShutdown();
        return 0;
    }

    // Start the task manager
    gTaskMgr.Init();

    DXUTMainLoop(); // Enter into the DXUT render loop

    // Stop the task manager
    g_Game.GetUnitManager()->StopWork();
    gTaskMgr.Shutdown();

    // Clean up the renderer
    Render::Destroy();

    return DXUTGetExitCode();
}
