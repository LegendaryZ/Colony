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
#include "UnitManager.h"
#include "Game.h"
#include "ColonyMath.h"
#include "Instrumentation.h"
#include <intrin.h>

// Intel GPA 4.0 defines
__itt_domain* s_pUnitMgrDomain = __itt_domain_createA( "Colony.UnitManager" );


TASKSETHANDLE   UnitManager::m_hBin = TASKSETHANDLE_INVALID;
TASKSETHANDLE   UnitManager::m_hDirection = TASKSETHANDLE_INVALID;
TASKSETHANDLE   UnitManager::m_hUpdate = TASKSETHANDLE_INVALID;

extern bool     g_bUseSIMD;
extern bool     g_bThreaded;
extern bool     g_bComputeAcrossFrames;

UnitManager::UnitManager( void ) : m_nNumUnits( 0 ),
                                   m_nFluidNumUnits( 0 ),
                                   m_fElapsedTime( 0.0f ),
                                   m_bStarted( false ) {}

UnitManager::~UnitManager( void ) {}

float GetRandValue( float fMin,
                    float fMax )
{
    static const float fRecipRandMax = 1.0f / RAND_MAX;
    float fRand = ( rand() * fRecipRandMax );
    fRand *= ( fMax - fMin );
    fRand += fMin;
    return fRand;
}

void UnitManager::StopWork( void )
{
    if( m_bStarted )
    {
        // Wait for the work to finish
        gTaskMgr.WaitForSet( m_hUpdate );
        gTaskMgr.ReleaseHandle( m_hUpdate );
        gTaskMgr.ReleaseHandle( m_hDirection );
        gTaskMgr.ReleaseHandle( m_hBin );

        m_bStarted = false;
    }
}

void UnitManager::Initialize( Game* pGame )
{
    m_pGame = pGame;
    m_nNumUnits = gs_nStartingUnits;
    m_nFluidNumUnits = m_nNumUnits;
    m_fElapsedTime = 0.0f;
    m_bStarted = false;

    // Zero out structures
    ZeroMemory( m_pBins, sizeof( m_pBins ) );
    ZeroMemory( m_nUnitIndices, sizeof( m_nUnitIndices ) );
    ZeroMemory( m_UnitPositionData, sizeof( m_UnitPositionData ) );
    ZeroMemory( m_UnitSharedData, sizeof( m_UnitSharedData ) );
    ZeroMemory( m_UnitCalculateDirection, sizeof( m_UnitCalculateDirection ) );
    ZeroMemory( m_UnitUpdate, sizeof( m_UnitUpdate ) );
    ZeroMemory( m_UnitRender, sizeof( m_UnitRender ) );

    // Now initialize units with random positions
    for( unsigned int i = 0; i < gs_nUnitTaskCount; ++i )
    {
        for( unsigned int j = 0; j < gs_nSIMDWidth; ++j )
        {
            m_UnitPositionData[i].fPositionX[j] = GetRandValue( 0.0f, gs_fWorldSize );
            m_UnitPositionData[i].fPositionY[j] = GetRandValue( 0.0f, gs_fWorldSize );

            m_UnitSharedData[i].fGoalPositionX[j] = GetRandValue( 0.0f, gs_fWorldSize );
            m_UnitSharedData[i].fGoalPositionY[j] = GetRandValue( 0.0f, gs_fWorldSize );

            m_UnitUpdate[i].fSpeed[j] = gs_fDefaultUnitSpeed;
            m_UnitUpdate[i].bCarrying[j] = false;

            m_UnitCalculateDirection[i].fRadius[j] = gs_fUnitRadius;
        }
    }
}

void UnitManager::Reset( void )
{
    // Reset the units
    for( unsigned int i = 0; i < gs_nUnitTaskCount; ++i )
    {
        for( unsigned int j = 0; j < gs_nSIMDWidth; ++j )
        {
            unsigned int* pFactories = m_pGame->GetFactories();

            unsigned int nIndex = pFactories[ ( i + j ) % gs_nMaxFactories ];

            m_UnitSharedData[i].fGoalPositionX[j] = m_pGame->GetTiles()[ nIndex ].fX;
            m_UnitSharedData[i].fGoalPositionY[j] = m_pGame->GetTiles()[ nIndex ].fY;
            m_UnitUpdate[i].nGoalIndex[j] = nIndex;

            m_UnitUpdate[i].fSpeed[j] = gs_fDefaultUnitSpeed;
            m_UnitUpdate[i].bCarrying[j] = false;

            m_UnitCalculateDirection[i].fRadius[j] = gs_fUnitRadius;
        }
    }
}

void UnitManager::Update( float fElapsedTime )
{
    // We use a "fluid" unit count to help average out frame time spikes with regards to unit count
    m_nNumUnits = ( int )( ( m_nNumUnits * 0.95f ) + ( m_nFluidNumUnits * 0.05f ) );
    m_nNumUnits = min( gs_nUnitTaskCount, m_nNumUnits );
    m_nNumUnits = max( m_nNumUnits, 0 );
    m_fElapsedTime = fElapsedTime;

    // The task handles
    TASKSETFUNC pCalculateDirection = CalculateDirectionTask;
    TASKSETFUNC pUpdate = ( g_bUseSIMD ? SIMDUpdateUnitTask : ScalarUpdateUnitTask );

    if( !g_bThreaded )
    { // Single threaded

        GPA_SCOPED_TASK( __FUNCTION__, s_pUnitMgrDomain );

        // Zero out the bins
        ZeroMemory( m_pBins, sizeof( m_pBins ) );

        // Fill the bins
        FillBinsTask( this, 0, 0, 1 );

        // Calculate the units direction
        pCalculateDirection( this, 0, 0, 1 );

        // Now update the units
        pUpdate( this, 0, 0, 1 );
    }
    else
    { // Multithreaded

        unsigned int uTasksToSpawn = gs_nTBBTaskCount;

        if( 0 == uTasksToSpawn )
        {
            uTasksToSpawn = m_nNumUnits;
        }

        // Wait for the work to finish
        StopWork();

        GPA_SCOPED_TASK( __FUNCTION__, s_pUnitMgrDomain );

        // Zero out the bins
        for( int i = 0; i < gs_nBinCountSq; ++i )
        {
            m_pBins[i].nUnits = 0;
        }

        // Fill the bins
        gTaskMgr.CreateTaskSet( FillBinsTask,
                                this,
                                uTasksToSpawn,
                                NULL,
                                0,
                                "FillBinsTask",
                                &m_hBin );

        // First calculate their directions
        gTaskMgr.CreateTaskSet( pCalculateDirection,
                                this,
                                uTasksToSpawn,
                                &m_hBin,
                                1,
                                "CalculateDirectionTask",
                                &m_hDirection );

        // Then update them
        gTaskMgr.CreateTaskSet( pUpdate,
                                this,
                                uTasksToSpawn,
                                &m_hDirection,
                                1,
                                "UpdateUnitTask",
                                &m_hUpdate );

        m_bStarted = true;

        if( !g_bComputeAcrossFrames )
        {
            // Wait for the work to finish
            StopWork();
        }
    }
}

void UnitManager::UnitLogic( unsigned int nUnit )
{
    Tile* pTiles = m_pGame->GetTiles();
    for( unsigned int nLane = 0; nLane < gs_nSIMDWidth; ++nLane )
    {
        int nTileX = ( int )( m_UnitPositionData[nUnit].fPositionX[nLane] / gs_fTileSize );
        int nTileY = ( int )( m_UnitPositionData[nUnit].fPositionY[nLane] / gs_fTileSize );

        int nTileIndex = nTileX * gs_nWorldSize + nTileY;

        if( nTileIndex < 0 || nTileIndex >= gs_nWorldSizeSq )
        {
            // The unit got pushed off the map, skip it
            continue;
        }

        if( m_UnitUpdate[nUnit].bCarrying[nLane] )
        { // You're carrying concrete
            if( pTiles[m_UnitUpdate[nUnit].nGoalIndex[nLane]].bActive )
            {
                // Your goal has already been paved, find a new one
                unsigned int nIndex = m_pGame->GetInactiveTile();

                m_UnitSharedData[nUnit].fGoalPositionX[nLane] = m_pGame->GetTiles()[ nIndex ].fX;
                m_UnitSharedData[nUnit].fGoalPositionY[nLane] = m_pGame->GetTiles()[ nIndex ].fY;
                m_UnitUpdate[nUnit].nGoalIndex[nLane] = nIndex;
            }

            // You're at an inactive tile, pave it
            if( !pTiles[nTileIndex].bActive )
            {
                // Stop and pave the tile
                if( !m_pGame->PaveTile( nTileIndex ) )
                {
                    m_UnitUpdate[nUnit].fSpeed[nLane] = 0.0f;
                }
                else
                {
                    // The tile was paved, back to your base
                    unsigned int nIndex = m_pGame->GetFactories()[ ( nUnit + nLane ) % gs_nMaxFactories ];

                    m_UnitSharedData[nUnit].fGoalPositionX[nLane] = m_pGame->GetTiles()[ nIndex ].fX;
                    m_UnitSharedData[nUnit].fGoalPositionY[nLane] = m_pGame->GetTiles()[ nIndex ].fY;
                    m_UnitUpdate[nUnit].nGoalIndex[nLane] = nIndex;

                    m_UnitUpdate[nUnit].bCarrying[nLane] = false;
                    m_UnitUpdate[nUnit].fSpeed[nLane] = gs_fDefaultUnitSpeed;
                }

            }
            else
            {
                // Just keep going until you find one
                m_UnitUpdate[nUnit].fSpeed[nLane] = gs_fDefaultUnitSpeed;

                // Now check to see if your at your goal tile
                if( nTileIndex == ( int )m_UnitUpdate[nUnit].nGoalIndex[nLane] )
                {
                    // You're at your goal, but someone already paved it
                    // Find a new goal
                    unsigned int nIndex = m_pGame->GetInactiveTile();

                    m_UnitSharedData[nUnit].fGoalPositionX[nLane] = m_pGame->GetTiles()[ nIndex ].fX;
                    m_UnitSharedData[nUnit].fGoalPositionY[nLane] = m_pGame->GetTiles()[ nIndex ].fY;
                    m_UnitUpdate[nUnit].nGoalIndex[nLane] = nIndex;
                }
            }
        }
        else
        { // You're grabbing more concrete

            Tile pGoalTile = pTiles[m_UnitUpdate[nUnit].nGoalIndex[nLane]];
            Tile pCurrTile = pTiles[nTileIndex];
            if( !pGoalTile.bActive )
            { // Goals not yet active, go directly to it
                if( nTileIndex == ( int )m_UnitUpdate[nUnit].nGoalIndex[nLane] )
                {
                    // You're at your factory
                    m_pGame->SetTileActive( nTileIndex );

                    m_UnitUpdate[nUnit].bCarrying[nLane] = true;
                    // Find a new goal
                    unsigned int nIndex = m_pGame->GetInactiveTile();

                    m_UnitSharedData[nUnit].fGoalPositionX[nLane] = m_pGame->GetTiles()[ nIndex ].fX;
                    m_UnitSharedData[nUnit].fGoalPositionY[nLane] = m_pGame->GetTiles()[ nIndex ].fY;
                    m_UnitUpdate[nUnit].nGoalIndex[nLane] = nIndex;
                }
            }
            else
            { // It is active, just get close enough
                float fXDiff = abs( pGoalTile.fX - pCurrTile.fX );
                float fYDiff = abs( pGoalTile.fY - pCurrTile.fY );

                if( fXDiff <= gs_fTileSize * 2 && fYDiff <= gs_fTileSize * 2 )
                {
                    m_UnitUpdate[nUnit].bCarrying[nLane] = true;
                    // Find a new goal
                    unsigned int nIndex = m_pGame->GetInactiveTile();

                    m_UnitSharedData[nUnit].fGoalPositionX[nLane] = m_pGame->GetTiles()[ nIndex ].fX;
                    m_UnitSharedData[nUnit].fGoalPositionY[nLane] = m_pGame->GetTiles()[ nIndex ].fY;
                    m_UnitUpdate[nUnit].nGoalIndex[nLane] = nIndex;
                }
            }
        }

        // Here we slow the units rotation by only modifying it slighty based on its current
        //   actual rotation. By doing this we can help smooth out very rapid "jukes" to
        //   avoid other units. This helps reduce rendering artifacts
        m_UnitUpdate[nUnit].fRotation[nLane] = ( m_UnitUpdate[nUnit].fRotation[nLane] *
                                                 0.98f ) + ( GetOrientation( nUnit, nLane ) * 0.02f );
    }
}

void UnitManager::AddUnit( void )
{
    if( m_nFluidNumUnits < gs_nUnitTaskCount )
    {
        ++m_nFluidNumUnits;
    }
}

void UnitManager::RemoveUnit( void )
{
    if( m_nFluidNumUnits > 1 )
    {
        --m_nFluidNumUnits;
    }
}

unsigned int UnitManager::GetNumUnits( void ) const
{
    return m_nNumUnits * gs_nSIMDWidth;
}

void UnitManager::SetUnitCount( unsigned int nUnits )
{
    if( nUnits > 0 && nUnits <= gs_nUnitTaskCount )
    {
        m_nNumUnits = nUnits;
        m_nFluidNumUnits = nUnits;
    }
}

/************************************************************************\
  Bins are used so units don't have to check other units that are too far
    away. The map is divided up into 8 tile x 8 tile bins that the units
    are hashed into based on their position. This makes the insertion of
    units into bins very fast, and because the bins only store indices,
    it means getting all neighbor bins units is fast too.
\************************************************************************/
void UnitManager::FillBinsTask( void* pVoid,
                                int nContext,
                                unsigned int uTaskId,
                                unsigned int uTaskCount )
{
    GPA_SCOPED_TASK( __FUNCTION__, s_pUnitMgrDomain );

    UnitManager* pManager = ( UnitManager* )pVoid;
    UnitPositionData* pPositionData = pManager->m_UnitPositionData;
    Bin* pBins = pManager->m_pBins;

    //  Convert task id to unit id.
    unsigned int uUnits = pManager->m_nNumUnits / uTaskCount;
    unsigned int uUnitStartId = uUnits * uTaskId;
    if( uTaskId + 1 == uTaskCount )
    {
        uUnits = pManager->m_nNumUnits - uUnits * uTaskId;
    }

    for( unsigned int i = 0; i < uUnits; ++i )
    {
        unsigned int uIndex = uUnitStartId + i;

        int nBinsAddedTo[4] =
        { -1, -1, -1, -1 };
        int nNumBinsAddedTo = 0;

        for( int nLane = 0; nLane < gs_nSIMDWidth; ++nLane )
        {
            // Get the bin that the unit is in
            int nBinX = ( int )( pPositionData[uIndex].fPositionX[nLane] * gs_fRecipBinSize );
            int nBinY = ( int )( pPositionData[uIndex].fPositionY[nLane] * gs_fRecipBinSize );

            int nBinIndex = nBinX * gs_nBinCount + nBinY;

            // Make sure the unit hasn't been bumped off the map
            if( nBinIndex >= 0 && nBinIndex < gs_nBinCountSq )
            {
                // Make sure the unit(s) haven't already been added to this bin
                //   each Unit is actually 4 to account for the SSE width
                //   so once a unit has been added, its 3 peers have also been added
                bool bAlreadyAdded = false;
                for( int nBin = 0; nBin < nNumBinsAddedTo; ++nBin )
                {
                    if( nBinIndex == nBinsAddedTo[ nBin ] )
                    {
                        bAlreadyAdded = true;
                        break;
                    }
                }

                // The unit(s) is not yet in this bin, add it
                if( !bAlreadyAdded )
                {
                    long nUnit = _InterlockedIncrement( &pBins[nBinIndex].nUnits );
                    assert( nUnit < gs_nBinCapacity );
                    pBins[nBinIndex].pUnits[nUnit - 1] = uIndex;

                    nBinsAddedTo[nNumBinsAddedTo++] = nBinIndex;
                }
            }
        }

    }

}

/***************************************************************\
  This algorithm calculates the units new direction by casting 
    two rays out from the current unit. These rays go in the
    direction the unit is currently moving, and are tangental to
    the units side.

  These rays are compared against every neighbor unit in each of
    the nearby bins. If either ray hits any of the units, the
    current unit turns to the same point on the opposite ray
    (the big X in the diagram below). This causes the unit to make
    a shaper or looser turn, depending on how far away the
    neighbor is.
                                                            
                                                             
                        /|\                /|\                  
                       / | \              / | \                 
                      /  |  \            /  |  \                
                         |                  |            
                         |                  |                
                         |                  |   oooooooo        
                         |                  |oMMooooooooMMo     
                         |                  MMo          oMM    
                         |                 MM              MM     
                         |                 MM   Neighbor   MM     
                         |                 MM     Unit     MM     
                       \ | /               MM              MM     
                         X  . . . . . . . . MMo          oMM      
                       / | \                |oMMooooooooMMo                  
                         |                  |   oooooooo                     
                         |                  |                
                         |                  |                
                         |     oooooooo     |
                         |  oMMooooooooMMo  |
                         | MMo          oMM |
                          MM              MM 
                          MM    Current   MM 
                          MM     Unit     MM 
                          MM              MM 
                           MMo          oMM  
                            oMMooooooooMMo   
                               oooooooo                                          
                                                            
\***************************************************************/

void UnitManager::CalculateDirectionTask( void* pVoid,
                                          int nContext,
                                          unsigned int uTaskId,
                                          unsigned int uTaskCount )
{
    GPA_SCOPED_TASK( __FUNCTION__, s_pUnitMgrDomain );

    UnitManager* pManager = ( UnitManager* )pVoid;
    UnitPositionData* pPositionData = pManager->m_UnitPositionData;
    UnitSharedData* pSharedData = pManager->m_UnitSharedData;
    UnitCalculateDirection* pCalculateData = pManager->m_UnitCalculateDirection;

    //  Covert task id to unit id.
    unsigned int uUnits = pManager->m_nNumUnits / uTaskCount;
    unsigned int uUnitStartId = uUnits * uTaskId;
    if( uTaskId + 1 == uTaskCount )
    {
        uUnits = pManager->m_nNumUnits - uUnits * uTaskId;
    }

    for( unsigned int i = 0; i < uUnits; ++i )
    {
        unsigned int uIndex = uUnitStartId + i;

        for( int nLane = 0; nLane < gs_nSIMDWidth; ++nLane )
        {
            //////////////////////////////////////////////////////////////////////////////////////
            //  Load the units data
            //////////////////////////////////////////////////////////////////////////////////////
            // Distances
            float fDistances[] =
            { gs_fGreatRange, gs_fGreatRange, };

            // Get the positions
            float fPositionX = pPositionData[uIndex].fPositionX[nLane];
            float fPositionY = pPositionData[uIndex].fPositionY[nLane];

            float fGoalPositionX = pSharedData[uIndex].fGoalPositionX[nLane];
            float fGoalPositionY = pSharedData[uIndex].fGoalPositionY[nLane];

            float fRadius = pCalculateData[uIndex].fRadius[nLane];

            // Calculate the ray direction 
            float fRayDirectionX = fGoalPositionX - fPositionX;
            float fRayDirectionY = fGoalPositionY - fPositionY;
            Normalize( fRayDirectionX, fRayDirectionY );

            // Calculate ray vertices
            float fDeltaX = -fRayDirectionY;
            float fDeltaY = fRayDirectionX;

            fDeltaX *= fRadius;
            fDeltaY *= fRadius;

            float fRayVerticesX[2] =
            { fPositionX - fDeltaX, fPositionX + fDeltaX };
            float fRayVerticesY[2] =
            { fPositionY - fDeltaY, fPositionY + fDeltaY };


            //////////////////////////////////////////////////////////////////////////////////////
            // Calculate which bins to check
            //////////////////////////////////////////////////////////////////////////////////////
            int nBinX = ( int )( fPositionX * gs_fRecipBinSize );
            int nBinY = ( int )( fPositionY * gs_fRecipBinSize );
            int nBinIndex = nBinX * gs_nBinCount + nBinY;

            unsigned int nUnitIndicesCount = 0;

            // Add current bins units
            if( nBinIndex < 0 || nBinIndex >= gs_nBinCountSq )
            {
                // The unit has been bumped off the map, 
                // don't simulate it until it comes back
                continue;
            }

            nUnitIndicesCount += pManager->m_pBins[nBinIndex].nUnits;

            unsigned int* p = pManager->m_nUnitIndices[nContext];
            memcpy( p, pManager->m_pBins[nBinIndex].pUnits, sizeof( unsigned int ) *
                    pManager->m_pBins[nBinIndex].nUnits );
            p += pManager->m_pBins[nBinIndex].nUnits;

            int nDeltaX = ( fRayDirectionX > 0.0f ? 1 : -1 );
            int nDeltaY = ( fRayDirectionY > 0.0f ? 1 : -1 );

            // Calculate and add next bins units
            nBinIndex = ( nBinX + nDeltaX ) * gs_nBinCount + nBinY;
            if( nBinIndex >= 0 && nBinIndex < gs_nBinCountSq )
            {
                nUnitIndicesCount += pManager->m_pBins[nBinIndex].nUnits;
                memcpy( p, pManager->m_pBins[nBinIndex].pUnits, sizeof( unsigned int ) *
                        pManager->m_pBins[nBinIndex].nUnits );
                p += pManager->m_pBins[nBinIndex].nUnits;
            }

            nBinIndex = nBinX * gs_nBinCount + ( nBinY + nDeltaY );
            if( nBinIndex >= 0 && nBinIndex < gs_nBinCountSq )
            {
                nUnitIndicesCount += pManager->m_pBins[nBinIndex].nUnits;
                memcpy( p, pManager->m_pBins[nBinIndex].pUnits, sizeof( unsigned int ) *
                        pManager->m_pBins[nBinIndex].nUnits );
                p += pManager->m_pBins[nBinIndex].nUnits;
            }

            nBinIndex = ( nBinX + nDeltaX ) * gs_nBinCount + ( nBinY + nDeltaY );
            if( nBinIndex >= 0 && nBinIndex < gs_nBinCountSq )
            {
                nUnitIndicesCount += pManager->m_pBins[nBinIndex].nUnits;
                memcpy( p, pManager->m_pBins[nBinIndex].pUnits, sizeof( unsigned int ) *
                        pManager->m_pBins[nBinIndex].nUnits );
                p += pManager->m_pBins[nBinIndex].nUnits;
            }


            //////////////////////////////////////////////////////////////////////////////////////
            // Compare against all potential neighbor units
            //////////////////////////////////////////////////////////////////////////////////////
            if( !g_bUseSIMD )
            {
                //////////////////////////////////////////////////////////////////////////////////////
                // Scalar
                //////////////////////////////////////////////////////////////////////////////////////
                for( unsigned int k = 0; k < nUnitIndicesCount; ++k )
                {
                    unsigned int nCompUnit = pManager->m_nUnitIndices[nContext][k];
                    for( int nCompLane = 0; nCompLane < gs_nSIMDWidth; ++nCompLane )
                    {
                        // Skip self
                        if( nCompUnit == uIndex && nCompLane == nLane )
                            continue;

                        float fCompPositionX = pPositionData[nCompUnit].fPositionX[nCompLane];
                        float fCompPositionY = pPositionData[nCompUnit].fPositionY[nCompLane];

                        float fCompRadius = pCalculateData[nCompUnit].fRadius[nCompLane];

                        float fDifferenceX = fCompPositionX - fPositionX;
                        float fDifferenceY = fCompPositionY - fPositionY;

                        float fDistance = LengthSq( fDifferenceX, fDifferenceY );

                        float fDot = DotProduct( fRayDirectionX, fRayDirectionY,
                                                 fDifferenceX, fDifferenceY );

                        // Skip units far away or behind
                        if( fDistance > gs_fGreatRange * gs_fGreatRange || fDot <= 0.0f )
                            continue;

                        fDistances[0] = min( fDistances[0],
                                             CircleRayCollision( fRayVerticesX[0], fRayVerticesY[0],
                                                                 fRayDirectionX, fRayDirectionY,
                                                                 fCompPositionX, fCompPositionY,
                                                                 fCompRadius ) );

                        fDistances[1] = min( fDistances[1],
                                             CircleRayCollision( fRayVerticesX[1], fRayVerticesY[1],
                                                                 fRayDirectionX, fRayDirectionY,
                                                                 fCompPositionX, fCompPositionY,
                                                                 fCompRadius ) );
                    }

                } // for( int k = 0; k < pManager->m_nNumUnits; ++k )
            }
            else
            {
                //////////////////////////////////////////////////////////////////////////////////////
                // SIMD
                //////////////////////////////////////////////////////////////////////////////////////
                __m128 RayDirectionX = _mm_load1_ps( &fRayDirectionX );
                __m128 RayDirectionY = _mm_load1_ps( &fRayDirectionY );

                __m128 RayVerticesX[2] =
                { _mm_load1_ps( &fRayVerticesX[0] ), _mm_load1_ps( &fRayVerticesX[1] ) };
                __m128 RayVerticesY[2] =
                { _mm_load1_ps( &fRayVerticesY[0] ), _mm_load1_ps( &fRayVerticesY[1] ) };

                __m128 Distances[2] =
                { _mm_load1_ps( &fDistances[0] ), _mm_load1_ps( &fDistances[1] ) };

                for( unsigned int k = 0; k < nUnitIndicesCount; ++k )
                {
                    unsigned int nCompUnit = pManager->m_nUnitIndices[nContext][k];
                    // Skip self
                    if( nCompUnit == uIndex )
                        continue;

                    __m128 CompPositionX = _mm_load_ps( pPositionData[nCompUnit].fPositionX );
                    __m128 CompPositionY = _mm_load_ps( pPositionData[nCompUnit].fPositionY );

                    __m128 CompRadius = _mm_load_ps( pCalculateData[nCompUnit].fRadius );

                    Distances[0] = _mm_min_ps( Distances[0],
                                               SSECircleRayCollision( RayVerticesX[0], RayVerticesY[0],
                                                                      RayDirectionX, RayDirectionY,
                                                                      CompPositionX, CompPositionY,
                                                                      CompRadius ) );

                    Distances[1] = _mm_min_ps( Distances[1],
                                               SSECircleRayCollision( RayVerticesX[1], RayVerticesY[1],
                                                                      RayDirectionX, RayDirectionY,
                                                                      CompPositionX, CompPositionY,
                                                                      CompRadius ) );

                } // for( int k = 0; k < pManager->m_nNumUnits; ++k )

                _declspec( align( 16 ) ) float Distances0[4] =
                { 0.0f, 0.0f, 0.0f, 0.0f };
                _declspec( align( 16 ) ) float Distances1[4] =
                { 0.0f, 0.0f, 0.0f, 0.0f };

                _mm_store_ps( Distances0, Distances[0] );
                _mm_store_ps( Distances1, Distances[1] );

                fDistances[0] = min( min( Distances0[0], Distances0[1] ), min( Distances0[2], Distances0[3] ) );
                fDistances[1] = min( min( Distances1[0], Distances1[1] ), min( Distances1[2], Distances1[3] ) );
            }

            //////////////////////////////////////////////////////////////////////////////////////
            //  Calculate the new direction to go in
            //////////////////////////////////////////////////////////////////////////////////////
            int nDistance = ( fDistances[0] <= fDistances[1] ) ? 0 : 1;

            // PointToTurnTo = RayVertex + (RayDirection * Distance)
            float fPointToTurnToX = fRayVerticesX[1 - nDistance] + ( fRayDirectionX * fDistances[nDistance] );
            float fPointToTurnToY = fRayVerticesY[1 - nDistance] + ( fRayDirectionY * fDistances[nDistance] );

            // NewDirection = Normalize( PointToTurnTo - Position )
            float fNewDirectionX = fPointToTurnToX - fPositionX;
            float fNewDirectionY = fPointToTurnToY - fPositionY;
            Normalize( fNewDirectionX, fNewDirectionY );

            if( fDistances[nDistance] < gs_fBadRange )
            {
                fNewDirectionX *= 0.5f;
                fNewDirectionY *= 0.5f;
            }

            pSharedData[uIndex].fDirectionX[nLane] = fNewDirectionX;
            pSharedData[uIndex].fDirectionY[nLane] = fNewDirectionY;

        } //  for( int nLane = 0; nLane < SIMD_WIDTH; ++nLane )
    } // for( unsigned int i = 0; i < uUnits; ++i )

}

void UnitManager::ScalarUpdateUnitTask( void* pVoid,
                                        int nContext,
                                        unsigned int uTaskId,
                                        unsigned int uTaskCount )
{
    GPA_SCOPED_TASK( __FUNCTION__, s_pUnitMgrDomain );

    UnitManager* pManager = ( UnitManager* )pVoid;
    UnitSharedData* pSharedData = pManager->m_UnitSharedData;
    UnitUpdate* pUpdateData = pManager->m_UnitUpdate;
    UnitPositionData* pPositionData = pManager->m_UnitPositionData;

    //  Convert task id to unit id.
    unsigned int uUnits = pManager->m_nNumUnits / uTaskCount;
    unsigned int uUnitStartId = uUnits * uTaskId;
    if( uTaskId + 1 == uTaskCount )
    {
        uUnits = pManager->m_nNumUnits - uUnits * uTaskId;
    }

    for( unsigned int i = 0; i < uUnits; ++i )
    {
        unsigned int uIndex = uUnitStartId + i;

        for( int nLane = 0; nLane < gs_nSIMDWidth; ++nLane )
        {
            // Position = Direction * Speed * ElapsedTime
            pPositionData[uIndex].fPositionX[nLane] += pSharedData[uIndex].fDirectionX[nLane] *
                pUpdateData[uIndex].fSpeed[nLane] * pManager->m_fElapsedTime;
            pPositionData[uIndex].fPositionY[nLane] += pSharedData[uIndex].fDirectionY[nLane] *
                pUpdateData[uIndex].fSpeed[nLane] * pManager->m_fElapsedTime;

            // Update rendering transforms
            pManager->m_UnitRender[ uIndex ].Transform[nLane] =
                XMMatrixRotationAxis( XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f ),
                                      pUpdateData[uIndex].fRotation[nLane] ) *
                XMMatrixTranslation( pManager->m_UnitPositionData[ uIndex ].fPositionX[nLane],
                                     gs_fBoxHeight,
                                     pManager->m_UnitPositionData[ uIndex ].fPositionY[nLane] );

        } //  for( int nLane = 0; nLane < SIMD_WIDTH; ++nLane )

        // Perform serial game update
        pManager->UnitLogic( uIndex );
    }
}

void UnitManager::SIMDUpdateUnitTask( void* pVoid,
                                      int nContext,
                                      unsigned int uTaskId,
                                      unsigned int uTaskCount )
{
    GPA_SCOPED_TASK( __FUNCTION__, s_pUnitMgrDomain );

    UnitManager* pManager = ( UnitManager* )pVoid;
    UnitSharedData* pSharedData = pManager->m_UnitSharedData;
    UnitUpdate* pUpdateData = pManager->m_UnitUpdate;
    UnitPositionData* pPositionData = pManager->m_UnitPositionData;

    //  Convert task id to unit id.
    unsigned int uUnits = pManager->m_nNumUnits / uTaskCount;
    unsigned int uUnitStartId = uUnits * uTaskId;
    if( uTaskId + 1 == uTaskCount )
    {
        uUnits = pManager->m_nNumUnits - uUnits * uTaskId;
    }

    for( unsigned int i = 0; i < uUnits; ++i )
    {
        unsigned int uIndex = uUnitStartId + i;

        //////////////////////////////////////////////////////////////////////////////////////
        // Load into SSE registers
        //////////////////////////////////////////////////////////////////////////////////////
        __m128 fPositionX = _mm_load_ps( pPositionData[uIndex].fPositionX );
        __m128 fPositionY = _mm_load_ps( pPositionData[uIndex].fPositionY );

        __m128 fDirectionX = _mm_load_ps( pSharedData[uIndex].fDirectionX );
        __m128 fDirectionY = _mm_load_ps( pSharedData[uIndex].fDirectionY );

        __m128 fSpeed = _mm_load_ps( pUpdateData[uIndex].fSpeed );

        //////////////////////////////////////////////////////////////////////////////////////
        // Perform calculation
        //////////////////////////////////////////////////////////////////////////////////////
        fSpeed = fSpeed * pManager->m_fElapsedTime;

        fPositionX += ( fDirectionX * fSpeed );
        fPositionY += ( fDirectionY * fSpeed );

        //////////////////////////////////////////////////////////////////////////////////////
        // Save the data back out
        //////////////////////////////////////////////////////////////////////////////////////
        _mm_store_ps( pPositionData[uIndex].fPositionX, fPositionX );
        _mm_store_ps( pPositionData[uIndex].fPositionY, fPositionY );

        //////////////////////////////////////////////////////////////////////////////////////
        // Update the rendering transforms
        //////////////////////////////////////////////////////////////////////////////////////
        pManager->m_UnitRender[ uIndex ].Transform[0] =
            XMMatrixRotationAxis( XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f ),
                                  pUpdateData[uIndex].fRotation[0] ) *
            XMMatrixTranslation( pManager->m_UnitPositionData[ uIndex ].fPositionX[0],
                                 gs_fBoxHeight,
                                 pManager->m_UnitPositionData[ uIndex ].fPositionY[0] );
        pManager->m_UnitRender[ uIndex ].Transform[1] =
            XMMatrixRotationAxis( XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f ),
                                  pUpdateData[uIndex].fRotation[1] ) *
            XMMatrixTranslation( pManager->m_UnitPositionData[ uIndex ].fPositionX[1],
                                 gs_fBoxHeight,
                                 pManager->m_UnitPositionData[ uIndex ].fPositionY[1] );
        pManager->m_UnitRender[ uIndex ].Transform[2] =
            XMMatrixRotationAxis( XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f ),
                                  pUpdateData[uIndex].fRotation[2] ) *
            XMMatrixTranslation( pManager->m_UnitPositionData[ uIndex ].fPositionX[2],
                                 gs_fBoxHeight,
                                 pManager->m_UnitPositionData[ uIndex ].fPositionY[2] );
        pManager->m_UnitRender[ uIndex ].Transform[3] =
            XMMatrixRotationAxis( XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f ),
                                  pUpdateData[uIndex].fRotation[3] ) *
            XMMatrixTranslation( pManager->m_UnitPositionData[ uIndex ].fPositionX[3],
                                 gs_fBoxHeight,
                                 pManager->m_UnitPositionData[ uIndex ].fPositionY[3] );

        // Perform serial game update
        pManager->UnitLogic( uIndex );
    }
}
