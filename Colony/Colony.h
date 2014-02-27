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
#ifndef _STDAFX_H_
#define _STDAFX_H_

#include <DXUT.h>
#include <xnamath.h>
#include <Windows.h>
#include "TaskMgrTBB.h"
#include "Instrumentation.h"

// Global defines
static const unsigned int   gs_nWorldSize = 512;    // Size of the world
static const unsigned int   gs_nWorldSizeSq = gs_nWorldSize * gs_nWorldSize;    // Size of the world
static const unsigned int   gs_nMaxUnits = 1024 * 1; // Max number of units ( 128k )
static const unsigned int   gs_nMaxFactories = 8; // Max number of factories


static const float          gs_fCoveragePerTile = 1.0f / gs_nWorldSizeSq;

static const unsigned int   gs_nTreeGranularity = 16;
static const unsigned int   gs_nTreeCount = gs_nWorldSizeSq / gs_nTreeGranularity;

static const unsigned int   gs_nTargetFPS = 30;

// Data
static const unsigned int   gs_nSIMDWidth = 4;
static const unsigned int   gs_nUnitTaskCount = gs_nMaxUnits / gs_nSIMDWidth;
static const unsigned int   gs_nMaxThreadCount = 32;
static const unsigned int   gs_nBinSize = 8;
static const unsigned int   gs_nBinCount = ( gs_nWorldSize / gs_nBinSize );
static const unsigned int   gs_nBinCountSq = gs_nBinCount * gs_nBinCount;
static const unsigned int   gs_nBinCapacity = 2048;
static const unsigned int   gs_nTBBTaskCount = 64;
static const unsigned int   gs_nStartingUnits = 1 * 1024 / gs_nSIMDWidth;

// Rendering sizes
static const float          gs_fUnitSize = 0.0212f;      // Units are 0.0212x0.0212 in size
static const float          gs_fUnitRadius = 0.015f;     // Units default radius

static const float          gs_fConvertToUnitSpace = 0.06f / 1.0f;

static const float          gs_fTileSize = 1.0f / 16.0f; // The size of 1 tile (16 tiles to 1 'unit')
static const float          gs_fWorldSize = gs_nWorldSize * gs_fTileSize; // The size of the world in 3D space
static const float          gs_fBoxHeight = 0.05f * gs_fTileSize;
static const float          gs_fBinSize = gs_fTileSize * gs_nBinSize;
static const float          gs_fRecipBinSize = 1.0f / gs_fBinSize;

static const unsigned int   gs_nRenderBatchSize = 1024;

// Unit behavior
static const float          gs_fDefaultUnitSpeed = ( gs_fTileSize / 2.0f ) * 5.0f;
static const float          gs_fTileTime = 0.1f;

static const float          gs_fGreatRange = 0.5f;
static const float          gs_fGoodRange = 0.2f;
static const float          gs_fBadRange = gs_fUnitRadius * 4;
// Used in GetOrientation
static const float          gs_fVertatan2 = atan2( -1.0f, 0.0f );

#endif // #ifndef _STDAFX_H_
