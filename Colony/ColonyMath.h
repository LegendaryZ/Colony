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
#ifndef _COLONYMATH_H_
#define _COLONYMATH_H_

__forceinline void SSENormalize( __m128& fX,
                                 __m128& fY )
{
    // Calculate the squares
    __m128 fSqX = _mm_mul_ps( fX, fX );
    __m128 fSqY = _mm_mul_ps( fY, fY );

    // Add them up for the lengths
    __m128 fLength = _mm_add_ps( fSqX, fSqY );

    // Get the sqrt
    fLength = _mm_sqrt_ps( fLength );

    // Then divide by it (normalize), and save as the new direction
    fX = _mm_div_ps( fX, fLength );
    fY = _mm_div_ps( fY, fLength );
}

__inline __m128 SSELengthSq( const __m128& fX,
                             const __m128& fY )
{
    // Calculate the squares
    __m128 fSqX = _mm_mul_ps( fX, fX );
    __m128 fSqY = _mm_mul_ps( fY, fY );

    // Add them up for the lengths
    return _mm_add_ps( fSqX, fSqY );
}

__inline __m128 SSEDotProduct( const __m128& fX1,
                               const __m128& fY1,
                               const __m128& fX2,
                               const __m128& fY2 )
{
    return _mm_add_ps( _mm_mul_ps( fX1, fX2 ), _mm_mul_ps( fY2, fY2 ) );
}
__inline __m128 SSECircleRayCollision( const __m128& fRayVertexX,
                                       const __m128& fRayVertexY,
                                       const __m128& fRayDirectionX,
                                       const __m128& fRayDirectionY,
                                       const __m128& fSphereVertexX,
                                       const __m128& fSphereVertexY,
                                       const __m128& fRadius )
{
    __m128 fDistance =
        SSEDotProduct( fRayDirectionX,
                       fRayDirectionY,
                       _mm_sub_ps( fSphereVertexX, fRayVertexX ),
                       _mm_sub_ps( fSphereVertexY, fRayVertexY ) );

    __m128 fCollisionPointX = _mm_add_ps( _mm_mul_ps( fRayDirectionX, fDistance ), fRayVertexX ),
        fCollisionPointY = _mm_add_ps( _mm_mul_ps( fRayDirectionY, fDistance ), fRayVertexY );

    __m128 fRadiusSq = _mm_mul_ps( fRadius, fRadius );
    __m128 fMiss = _mm_and_ps( _mm_cmpgt_ps( fDistance, _mm_set1_ps( 0.0f ) ),
                               _mm_cmple_ps( SSELengthSq( _mm_sub_ps( fSphereVertexX, fCollisionPointX ),
                                                          _mm_sub_ps( fSphereVertexY, fCollisionPointY ) ),
                                             fRadiusSq ) );

    fDistance = _mm_and_ps( fMiss, fDistance );
    return _mm_or_ps( fDistance, _mm_andnot_ps( fMiss, _mm_set1_ps( gs_fGreatRange ) ) );
}

__inline void Normalize( float& x,
                         float& y )
{
    float fX = x * x;
    float fY = y * y;

    float fLength = fX + fY;

    fLength = sqrtf( fLength );

    x = x / fLength;
    y = y / fLength;
}

__inline float LengthSq( float x,
                         float y )
{
    return x * x + y * y;
}

__inline float DotProduct( float x1,
                           float y1,
                           float x2,
                           float y2 )
{
    return x1 * x2 + y1 * y2;
}

__inline float CircleRayCollision( float fRayVertexX,
                                   float fRayVertexY,
                                   float fRayDirectionX,
                                   float fRayDirectionY,
                                   float fSphereVertexX,
                                   float fSphereVertexY,
                                   float fRadius )
{
    float fVecToSphereX = fSphereVertexX - fRayVertexX;
    float fVecToSphereY = fSphereVertexY - fRayVertexY;

    float fDistance = DotProduct( fVecToSphereX, fVecToSphereY,
                                  fRayDirectionX, fRayDirectionY );

    float fCollisionPosX = fRayDirectionX * fDistance + fRayVertexX;
    float fCollisionPosY = fRayDirectionY * fDistance + fRayVertexY;

    float fCircleRayDistance = LengthSq( fSphereVertexX - fCollisionPosX, fSphereVertexY - fCollisionPosY );
    if( fCircleRayDistance > fRadius * fRadius )
        return gs_fGreatRange; // Clean miss

    fDistance -= fRadius;

    return fDistance;
}


#endif // #ifndef _COLONYMATH_H_
