//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef GAME_H
#define GAME_H


#include "globals.h"

extern void GameDLLInit( void );

extern ConVar	displaysoundlist;
extern ConVar	mapcyclefile;
extern ConVar	servercfgfile;
extern ConVar	lservercfgfile;

// multiplayer server rules
extern ConVar	teamplay;
extern ConVar	fraglimit;
extern ConVar	falldamage;
extern ConVar	weaponstay;
extern ConVar	forcerespawn;
extern ConVar	footsteps;
extern ConVar	flashlight;
extern ConVar	aimcrosshair;
extern ConVar	decalfrequency;
extern ConVar	teamlist;
extern ConVar	teamoverride;
extern ConVar	defaultteam;
extern ConVar	allowNPCs;

extern ConVar	suitvolume;

// Engine Cvars
extern const ConVar *g_pDeveloper;

// HL1

extern void GameDLLInitHL1( void );


extern cvar_t	displaysoundlist_hl1;

// multiplayer server rules
extern cvar_t	teamplay_hl1;
extern cvar_t	fraglimit_hl1;
extern cvar_t	timelimit;
extern cvar_t	friendlyfire_hl1;
extern cvar_t	falldamage_hl1;
extern cvar_t	weaponstay_hl1;
extern cvar_t	forcerespawn_hl1;
extern cvar_t	flashlight_hl1;
extern cvar_t	aimcrosshair_hl1;
extern cvar_t	decalfrequency_hl1;
extern cvar_t	teamlist_hl1;
extern cvar_t	teamoverride_hl1;
extern cvar_t	defaultteam_hl1;
extern cvar_t	allowmonsters;

// Engine Cvars
extern cvar_t	*g_psv_gravity;
extern cvar_t	*g_psv_aim;
extern cvar_t	*g_footsteps;

#endif		// GAME_H
