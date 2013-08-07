//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//=========================================================
// NPC template
//=========================================================
#include "cbase.h"

//HL1
#include	"extdll.h"
#include	"util.h"
#include	"monsters.h"
#include	"schedule.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if 0

//=========================================================
// NPC's Anim Events Go Here
//=========================================================

class CMyNPC : public CAI_BaseNPC
{
public:
	DECLARE_CLASS( CMyNPC, CAI_BaseNPC );

	void Spawn( void );
	void Precache( void );
	void MaxYawSpeed( void );
	int  Classify ( void );
	void HandleAnimEvent( animevent_t *pEvent );
};
LINK_ENTITY_TO_CLASS( my_NPC, CMyNPC );

//=========================================================
// Classify - indicates this NPC's place in the 
// relationship table.
//=========================================================
int	CMyNPC::Classify ( void )
{
	return	CLASS_MY_NPC;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
float CMyNPC::MaxYawSpeed ( void )
{
	switch ( m_Activity )
	{
	case ACT_IDLE:
	default:
		return 90;
	}
}

//=========================================================
// HandleAnimEvent - catches the NPC-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CMyNPC::HandleAnimEvent( animevent_t *pEvent )
{
	switch( pEvent->event )
	{
	case 0:
	default:
		CAI_BaseNPC::HandleAnimEvent( pEvent );
		break;
	}
}

//=========================================================
// Spawn
//=========================================================
void CMyNPC::Spawn()
{
	Precache( );

	engine.SetModel(edict(), "models/mymodel.mdl");
	UTIL_SetSize( this, Vector( -12, -12, 0 ), Vector( 12, 12, 24 ) );

	SetSolid( SOLID_SLIDEBOX );
	SetMoveType( MOVETYPE_STEP );
	m_bloodColor		= BLOOD_COLOR_GREEN;
	m_iHealth			= 8;
	m_vecViewOffset		= Vector ( 0, 0, 0 );// position of the eyes relative to NPC's origin.
	m_flFieldOfView		= 0.5;// indicates the width of this NPC's forward view cone ( as a dotproduct result )
	m_NPCState		= NPCSTATE_NONE;

	NPCInit();
}

//=========================================================
// Precache - precaches all resources this NPC needs
//=========================================================
void CMyNPC::Precache()
{
	engine.PrecacheModel("models/mymodel.mdl");
}	

//=========================================================
// AI Schedules Specific to this NPC
//=========================================================

// HL1

//=========================================================
// Monster's Anim Events Go Here
//=========================================================

class CMyMonster : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int  Classify ( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
};
LINK_ENTITY_TO_CLASS( my_monster, CMyMonster );

//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int	CMyMonster :: Classify ( void )
{
	return	CLASS_MY_MONSTER;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CMyMonster :: SetYawSpeed ( void )
{
	int ys;

	switch ( m_Activity )
	{
	case ACT_IDLE:
	default:
		ys = 90;
	}

	pev->yaw_speed = ys;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CMyMonster :: HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
	case 0:
	default:
		CBaseMonster::HandleAnimEvent( pEvent );
		break;
	}
}

//=========================================================
// Spawn
//=========================================================
void CMyMonster :: Spawn()
{
	Precache( );

	SET_MODEL(ENT(pev), "models/mymodel.mdl");
	UTIL_SetSize( pev, Vector( -12, -12, 0 ), Vector( 12, 12, 24 ) );

	pev->solid			= SOLID_SLIDEBOX;
	pev->movetype		= MOVETYPE_STEP;
	m_bloodColor		= BLOOD_COLOR_GREEN;
	pev->health			= 8;
	pev->view_ofs		= Vector ( 0, 0, 0 );// position of the eyes relative to monster's origin.
	m_flFieldOfView		= 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState		= MONSTERSTATE_NONE;

	MonsterInit();
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CMyMonster :: Precache()
{
	PRECACHE_SOUND("mysound.wav");

	PRECACHE_MODEL("models/mymodel.mdl");
}	

//=========================================================
// AI Schedules Specific to this monster
//=========================================================
#endif 0 //if problem remove 0 and monster code
