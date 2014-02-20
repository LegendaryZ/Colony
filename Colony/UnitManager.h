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
#ifndef _UNITMANAGER_H_
#define _UNITMANAGER_H_
#include "Colony.h"
#include "TaskMgrTBB.h"

class Game;

class __declspec( align( 16 ) ) UnitManager
{
public:
    UnitManager( void );
    ~UnitManager( void );

    // Initialize the units
    void Initialize( Game* pGame );

    // Reset the units
    void Reset( void );

    // Update the units
    void Update( float fElapsedTime );

    // Serial unit logic code
    void UnitLogic( unsigned int nUnit );

    // Get the units absolute orientation from 0pi to 2pi (0-360)
    float GetOrientation( unsigned int nUnit,
                          unsigned int nLane ) const;

    // Get the units render transforms
    XMMATRIX* GetTransforms( void ) const;

    // Add/remove units
    void AddUnit( void );
    void RemoveUnit( void );
    unsigned int GetNumUnits( void ) const;
    void SetUnitCount( unsigned int nUnits );

    // Stop the threaded work
    void StopWork( void );

private:
    // TBB Task functions
    static void FillBinsTask( void* pVoid,
                              int nContext,
                              unsigned int uTaskId,
                              unsigned int uTaskCount );

    static void CalculateDirectionTask( void* pVoid,
                                        int nContext,
                                        unsigned int uTaskId,
                                        unsigned int uTaskCount );
    static void ScalarUpdateUnitTask( void* pVoid,
                                      int nContext,
                                      unsigned int uTaskId,
                                      unsigned int uTaskCount );

    static void SIMDUpdateUnitTask( void* pVoid,
                                    int nContext,
                                    unsigned int uTaskId,
                                    unsigned int uTaskCount );

private:
    //////////////////////////////////////////////////////////////////////////////////////
    // Struct definitions
    //////////////////////////////////////////////////////////////////////////////////////
    struct Bin
    {
        unsigned int pUnits[gs_nBinCapacity-1];
        volatile long nUnits;
    };

    //
    // The data is defined in structs to allow for organization
    //   based on the SIMD register width. So each struct can
    //   fill one SIMD register, be it SSE, AVX, etc.
    //
    // These structs are broken up by the code they're needed in
    //   UnitCalculateDirection is only used in CalculateDirection,
    //   UnitUpdate is only used in Update,
    //   UnitSharedData is used in both
    //   and UnitPositionData is used all over the place
    // It is laid out like this to help ensure efficient cache
    //   usage. Only the data you actually need will be in the cache
    //
    struct __declspec( align( 16 ) ) UnitPositionData
    {
        float fPositionX[ gs_nSIMDWidth ];
        float fPositionY[ gs_nSIMDWidth ];
    };

    struct __declspec( align( 16 ) ) UnitSharedData
    {
        float fDirectionX[ gs_nSIMDWidth ];
        float fDirectionY[ gs_nSIMDWidth ];

        float fGoalPositionX[ gs_nSIMDWidth ];
        float fGoalPositionY[ gs_nSIMDWidth ];
    };

    struct __declspec( align( 16 ) ) UnitCalculateDirection
    {
        float fRadius[ gs_nSIMDWidth ];
    };

    struct __declspec( align( 16 ) ) UnitUpdate
    {
        float fSpeed[ gs_nSIMDWidth ];
        float fRotation[ gs_nSIMDWidth ];
        unsigned int nGoalIndex[ gs_nSIMDWidth ];
        int bCarrying[ gs_nSIMDWidth ];
    };

    struct __declspec( align( 16 ) ) UnitRender
    {
        XMMATRIX Transform[ gs_nSIMDWidth ];
    };

    //////////////////////////////////////////////////////////////////////////////////////
    // Member declarations
    //////////////////////////////////////////////////////////////////////////////////////
    UnitPositionData m_UnitPositionData[gs_nUnitTaskCount];
    UnitSharedData m_UnitSharedData[gs_nUnitTaskCount];
    UnitCalculateDirection m_UnitCalculateDirection[gs_nUnitTaskCount];
    UnitUpdate m_UnitUpdate[gs_nUnitTaskCount];
    UnitRender m_UnitRender[gs_nUnitTaskCount];

    Bin m_pBins[gs_nBinCountSq];
    unsigned int m_nUnitIndices[gs_nMaxThreadCount][gs_nBinCapacity];   // We only check 4 bins at a time

    Game* m_pGame;
    unsigned int m_nNumUnits;
    unsigned int m_nFluidNumUnits;
    float m_fElapsedTime;

    bool m_bStarted;

    static TASKSETHANDLE m_hBin;
    static TASKSETHANDLE m_hDirection;
    static TASKSETHANDLE m_hUpdate;
};

// Get the units absolute orientation from 0pi to 2pi (0-360)
_inline float UnitManager::GetOrientation( unsigned int nUnit,
                                           unsigned int nLane ) const
{
    // The arc tangent of a vertical angle, so we can get our 2D rotation
    return gs_fVertatan2 - atan2( m_UnitSharedData[nUnit].fDirectionY[nLane],
                                  m_UnitSharedData[nUnit].fDirectionX[nLane] );
}

// Get the units render transforms
_inline XMMATRIX* UnitManager::GetTransforms( void ) const
{
    return ( XMMATRIX* )&m_UnitRender;
}

#endif // #ifndef _UNITMANAGER_H_
