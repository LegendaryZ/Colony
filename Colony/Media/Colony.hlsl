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

Texture2D		DiffuseTexture : register( t0 );
Texture2D		SkyTexture : register( t1 );
Texture2D       DarkSkyTexture : register( t2 );

SamplerState	DiffuseTextureSampler : register( s0 );

//---------------------------------------------------
//	Constant buffers
//--------------------------------------------------
cbuffer cbPerFrameCB
{
	Matrix ViewProj;
    float4 Params;
};

//--------------------------------------------------
//	structs
//--------------------------------------------------
struct VS_INPUT_INSTANCED
{
	float4 Position		: POSITION;
	float4 Normal		: NORMAL;
	float2 TexCoord		: TEXCOORD0;
    row_major float4x4 Transform : Transform;
};

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float3 Normal   : NORMAL;
	float2 TexCoord	: TEXCOORD0;
    float2 Coverage : TEXCOORD1;
};

PS_INPUT VS_Instanced( VS_INPUT_INSTANCED Input )
{
	PS_INPUT Output;
	
	float4 PosWorld = mul(Input.Position, Input.Transform);
	Output.Position = mul(PosWorld, ViewProj);
    Output.Normal   = normalize( mul( Input.Normal.xyz, Input.Transform ) );
	Output.TexCoord = Input.TexCoord;
    Output.Coverage = Params.xy;
	
	return Output;
}

float3 increase_saturation( float3 color )
{
    // http://www.francois-tarlier.com/blog/index.php/2009/11/saturation-shader/
    float grayscale = ( color.r + color.g + color.g + color.b ) / 4;
    float3 avgcolor = float3( grayscale, grayscale, grayscale );

    return ( ( color.rgb - avgcolor ) * 0.5f ) + avgcolor;
}

float4 PS( PS_INPUT Input ) : SV_TARGET
{
    // light
    float3 lightColor = float3( 1.0f, 1.0f, 1.0f );
    float3 lightDir = float3( -1.0f, -1.0f, 0.0f );
    float3 lightVec = -lightDir;

    // texture
    float4 texColor = DiffuseTexture.Sample(DiffuseTextureSampler, Input.TexCoord);

    // increase color saturation
    lightColor = increase_saturation( texColor.rgb );
    
    // diffuse
    float ndotl = max( dot( lightVec, Input.Normal.xyz ), 0.05f ); 
    float3 diffuse = ndotl * texColor.rgb * lightColor;

    return float4( diffuse, texColor.a );
}

// Sky
float4 PS_Sky( PS_INPUT Input ) : SV_TARGET
{
    float4 skyColor = SkyTexture.Sample( DiffuseTextureSampler, Input.TexCoord );
    float4 darkSkyColor = DarkSkyTexture.Sample( DiffuseTextureSampler, Input.TexCoord );

    float fCoverage = Input.Coverage.x + 0.4f;

    return float4( max((1.0f - fCoverage), 0.0f) * skyColor.rgb + max(fCoverage,1.0f) * darkSkyColor.rgb, 1.0f );
}
