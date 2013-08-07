//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#ifndef DECALS_H
#define DECALS_H
#ifdef _WIN32
#pragma once
#endif

// NOTE: If you add a tex type, be sure to modify the s_pImpactEffect
// array in fx_impact.cpp to get an effect when that surface is shot.
#define CHAR_TEX_ANTLION		'A'
#define CHAR_TEX_BLOODYFLESH	'B'
#define	CHAR_TEX_CONCRETE		'C'
#define CHAR_TEX_DIRT			'D'
#define CHAR_TEX_EGGSHELL		'E' ///< the egg sacs in the tunnels in ep2.
#define CHAR_TEX_FLESH			'F'
#define CHAR_TEX_GRATE			'G'
#define CHAR_TEX_ALIENFLESH		'H'
#define CHAR_TEX_CLIP			'I'
//#define CHAR_TEX_UNUSED		'J'
//#define CHAR_TEX_UNUSED		'K'
#define CHAR_TEX_PLASTIC		'L'
#define CHAR_TEX_METAL			'M'
#define CHAR_TEX_SAND			'N'
#define CHAR_TEX_FOLIAGE		'O'
#define CHAR_TEX_COMPUTER		'P'
//#define CHAR_TEX_UNUSED		'Q'
//#define CHAR_TEX_UNUSED		'R'
#define CHAR_TEX_SLOSH			'S'
#define CHAR_TEX_TILE			'T'
//#define CHAR_TEX_UNUSED		'U'
#define CHAR_TEX_VENT			'V'
#define CHAR_TEX_WOOD			'W'
//#define CHAR_TEX_UNUSED		'X'
#define CHAR_TEX_GLASS			'Y'
#define CHAR_TEX_WARPSHIELD		'Z' ///< wierd-looking jello effect for advisor shield.

abstract_class IDecalEmitterSystem
{
public:
	virtual int	GetDecalIndexForName( char const *decalname ) = 0;
	virtual const char *GetDecalNameForIndex( int nIndex ) = 0;
	virtual char const *TranslateDecalForGameMaterial( char const *decalName, unsigned char gamematerial ) = 0;
};

extern IDecalEmitterSystem *decalsystem;

// HL1

//
// Dynamic Decals
//
enum decal_e 
{	
	DECAL_GUNSHOT1 = 0, 
	DECAL_GUNSHOT2,
	DECAL_GUNSHOT3,
	DECAL_GUNSHOT4,
	DECAL_GUNSHOT5,
	DECAL_LAMBDA1,
	DECAL_LAMBDA2,
	DECAL_LAMBDA3,
	DECAL_LAMBDA4,
	DECAL_LAMBDA5,
	DECAL_LAMBDA6,
	DECAL_SCORCH1,
	DECAL_SCORCH2,
	DECAL_BLOOD1, 
	DECAL_BLOOD2, 
	DECAL_BLOOD3, 
	DECAL_BLOOD4, 
	DECAL_BLOOD5, 
	DECAL_BLOOD6, 
	DECAL_YBLOOD1, 
	DECAL_YBLOOD2, 
	DECAL_YBLOOD3, 
	DECAL_YBLOOD4, 
	DECAL_YBLOOD5, 
	DECAL_YBLOOD6, 
	DECAL_GLASSBREAK1,
	DECAL_GLASSBREAK2,
	DECAL_GLASSBREAK3,
	DECAL_BIGSHOT1,
	DECAL_BIGSHOT2,
	DECAL_BIGSHOT3,
	DECAL_BIGSHOT4,
	DECAL_BIGSHOT5,
	DECAL_SPIT1,
	DECAL_SPIT2,
	DECAL_BPROOF1,		// Bulletproof glass decal
	DECAL_GARGSTOMP1,	// Gargantua stomp crack
	DECAL_SMALLSCORCH1,	// Small scorch mark
	DECAL_SMALLSCORCH2,	// Small scorch mark
	DECAL_SMALLSCORCH3,	// Small scorch mark
	DECAL_MOMMABIRTH,	// Big momma birth splatter
	DECAL_MOMMASPLAT,
};

typedef struct 
{
	char	*name;
	int		index;
} DLL_DECALLIST;

extern DLL_DECALLIST gDecals[];

#endif // DECALS_H
