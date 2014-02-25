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
#ifndef _GAME_H_
#define _GAME_H_
#include "Colony.h"
#include "UnitManager.h"

// Structure for each game "space"
struct Tile
{
    float fTimer;
    float fX;
    float fY;
    int nTree;
    volatile bool bActive;
    bool bFactory;
	int faction;							//Eryc - Way to mark tiles for shaders
};

class __declspec( align( 16 ) ) Game
{
public:
    Game( void );
    ~Game( void );

    // Initialize the game
    void Initialize( void );

    // Update the simulation
    void Update( float fTime,
                 float fElapsedTime );

    // Reset the simulation
    void Reset( void );

    // Render the game
    void Render( void );

    // Get the unit manager
    UnitManager* GetUnitManager( void );

    // Get an inactive tile for the units
    unsigned int GetInactiveTile( void );

    // Flag a tile as active
    void SetTileActive( unsigned int nTile);

    // Get the array of tiles
    Tile* GetTiles( void );

    // Get the factory indices
    unsigned int* GetFactories( void );

    // Coverage
    float GetCoverage( void ) const;
    void SetCoverageThreshold( float fThreshold );

    // Lower the tree
    bool PaveTile( unsigned int nTile );

private:
    UnitManager m_UnitManager;                       // The unit manager
    Tile m_pTiles[ gs_nWorldSizeSq ];         // The world tiles
    unsigned int m_pInactiveTiles[ gs_nWorldSizeSq ]; // The inactive tiles

    // Rendering data
    XMMATRIX m_pTileMatrices[ gs_nWorldSizeSq ];
    XMMATRIX m_pFactoryMatrices[ gs_nMaxFactories ];
    XMMATRIX m_pActiveTreeMatrices[ gs_nTreeCount ];
    XMMATRIX m_pInactiveTreeMatrices[ gs_nTreeCount ];

    unsigned int m_pFactories[ gs_nMaxFactories ];
    unsigned int m_nNumActiveFactories;

    CRITICAL_SECTION m_CriticalSection;// The critical section for safely activating a tile
    volatile long m_nNumActiveTiles;      // The number of active tiles and matrices
    volatile long m_nNumInactiveTiles;    // The number of inactive tiles
    volatile long m_nNumActiveTrees;      // The number of rendered trees
    int m_nNumInactiveTrees;    // The number of out of bounds trees
    float m_fElapsedTime;         // The elapsed time of the last frame
    float m_fTime;                // The running time of the simulation
    float m_fCoverage;            // The current coverage of the world
    float m_fCoverageThreshold;   // The threshold for reset
};

_inline UnitManager* Game::GetUnitManager( void )
{
    return &m_UnitManager;
}

_inline float Game::GetCoverage( void ) const
{
    return m_fCoverage;
}

_inline void Game::SetCoverageThreshold( float fThreshold )
{
    m_fCoverageThreshold = fThreshold;
}

_inline Tile* Game::GetTiles( void )
{
    return m_pTiles;
}

_inline unsigned int* Game::GetFactories( void )
{
    return m_pFactories;
}

#endif // #ifndef _GAME_H_
