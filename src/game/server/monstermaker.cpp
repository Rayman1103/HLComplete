//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: An entity that creates NPCs in the game. There are two types of NPC
//			makers -- one which creates NPCs using a template NPC, and one which
//			creates an NPC via a classname.
//
//=============================================================================//

#include "cbase.h"
#include "datacache/imdlcache.h"
#include "entityapi.h"
#include "entityoutput.h"
#include "ai_basenpc.h"
#include "monstermaker.h"
#include "TemplateEntities.h"
#include "ndebugoverlay.h"
#include "mapentities.h"
#include "IEffects.h"
#include "props.h"

//HL1
#include "extdll.h"
#include "util.h"
#include "monsters.h"
#include "saverestore.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static void DispatchActivate( CBaseEntity *pEntity )
{
	bool bAsyncAnims = mdlcache->SetAsyncLoad( MDLCACHE_ANIMBLOCK, false );
	pEntity->Activate();
	mdlcache->SetAsyncLoad( MDLCACHE_ANIMBLOCK, bAsyncAnims );
}

ConVar ai_inhibit_spawners( "ai_inhibit_spawners", "0", FCVAR_CHEAT );


LINK_ENTITY_TO_CLASS( info_npc_spawn_destination, CNPCSpawnDestination );

BEGIN_DATADESC( CNPCSpawnDestination )
	DEFINE_KEYFIELD( m_ReuseDelay, FIELD_FLOAT, "ReuseDelay" ),
	DEFINE_KEYFIELD( m_RenameNPC,FIELD_STRING, "RenameNPC" ),
	DEFINE_FIELD( m_TimeNextAvailable, FIELD_TIME ),

	DEFINE_OUTPUT( m_OnSpawnNPC,	"OnSpawnNPC" ),
END_DATADESC()

//---------------------------------------------------------
//---------------------------------------------------------
CNPCSpawnDestination::CNPCSpawnDestination()
{
	// Available right away, the first time.
	m_TimeNextAvailable = gpGlobals->curtime;
}

//---------------------------------------------------------
//---------------------------------------------------------
bool CNPCSpawnDestination::IsAvailable()
{
	if( m_TimeNextAvailable > gpGlobals->curtime )
	{
		return false;
	}

	return true;
}

//---------------------------------------------------------
//---------------------------------------------------------
void CNPCSpawnDestination::OnSpawnedNPC( CAI_BaseNPC *pNPC )
{
	// Rename the NPC
	if( m_RenameNPC != NULL_STRING )
	{
		pNPC->SetName( m_RenameNPC );
	}

	m_OnSpawnNPC.FireOutput( pNPC, this );
	m_TimeNextAvailable = gpGlobals->curtime + m_ReuseDelay;
}

//-------------------------------------
BEGIN_DATADESC( CBaseNPCMaker )

	DEFINE_KEYFIELD( m_nMaxNumNPCs,			FIELD_INTEGER,	"MaxNPCCount" ),
	DEFINE_KEYFIELD( m_nMaxLiveChildren,		FIELD_INTEGER,	"MaxLiveChildren" ),
	DEFINE_KEYFIELD( m_flSpawnFrequency,		FIELD_FLOAT,	"SpawnFrequency" ),
	DEFINE_KEYFIELD( m_bDisabled,			FIELD_BOOLEAN,	"StartDisabled" ),

	DEFINE_FIELD(	m_nLiveChildren,		FIELD_INTEGER ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID,	"Spawn",	InputSpawnNPC ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"Enable",	InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"Disable",	InputDisable ),
	DEFINE_INPUTFUNC( FIELD_VOID,	"Toggle",	InputToggle ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetMaxChildren", InputSetMaxChildren ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "AddMaxChildren", InputAddMaxChildren ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetMaxLiveChildren", InputSetMaxLiveChildren ),
	DEFINE_INPUTFUNC( FIELD_FLOAT,	 "SetSpawnFrequency", InputSetSpawnFrequency ),

	// Outputs
	DEFINE_OUTPUT( m_OnAllSpawned,		"OnAllSpawned" ),
	DEFINE_OUTPUT( m_OnAllSpawnedDead,	"OnAllSpawnedDead" ),
	DEFINE_OUTPUT( m_OnAllLiveChildrenDead,	"OnAllLiveChildrenDead" ),
	DEFINE_OUTPUT( m_OnSpawnNPC,		"OnSpawnNPC" ),

	// Function Pointers
	DEFINE_THINKFUNC( MakerThink ),

	DEFINE_FIELD( m_hIgnoreEntity, FIELD_EHANDLE ),
	DEFINE_KEYFIELD( m_iszIngoreEnt, FIELD_STRING, "IgnoreEntity" ), 
END_DATADESC()


//-----------------------------------------------------------------------------
// Purpose: Spawn
//-----------------------------------------------------------------------------
void CBaseNPCMaker::Spawn( void )
{
	SetSolid( SOLID_NONE );
	m_nLiveChildren		= 0;
	Precache();

	// If I can make an infinite number of NPC, force them to fade
	if ( m_spawnflags & SF_NPCMAKER_INF_CHILD )
	{
		m_spawnflags |= SF_NPCMAKER_FADE;
	}

	//Start on?
	if ( m_bDisabled == false )
	{
		SetThink ( &CBaseNPCMaker::MakerThink );
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
	else
	{
		//wait to be activated.
		SetThink ( &CBaseNPCMaker::SUB_DoNothing );
	}
}

//-----------------------------------------------------------------------------
// A not-very-robust check to see if a human hull could fit at this location.
// used to validate spawn destinations.
//-----------------------------------------------------------------------------
bool CBaseNPCMaker::HumanHullFits( const Vector &vecLocation )
{
	trace_t tr;
	UTIL_TraceHull( vecLocation,
					vecLocation + Vector( 0, 0, 1 ),
					NAI_Hull::Mins(HULL_HUMAN),
					NAI_Hull::Maxs(HULL_HUMAN),
					MASK_NPCSOLID,
					m_hIgnoreEntity,
					COLLISION_GROUP_NONE,
					&tr );

	if( tr.fraction == 1.0 )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether or not it is OK to make an NPC at this instant.
//-----------------------------------------------------------------------------
bool CBaseNPCMaker::CanMakeNPC( bool bIgnoreSolidEntities )
{
	if( ai_inhibit_spawners.GetBool() )
		return false;

	if ( m_nMaxLiveChildren > 0 && m_nLiveChildren >= m_nMaxLiveChildren )
	{// not allowed to make a new one yet. Too many live ones out right now.
		return false;
	}

	if ( m_iszIngoreEnt != NULL_STRING )
	{
		m_hIgnoreEntity = gEntList.FindEntityByName( NULL, m_iszIngoreEnt );
	}

	Vector mins = GetAbsOrigin() - Vector( 34, 34, 0 );
	Vector maxs = GetAbsOrigin() + Vector( 34, 34, 0 );
	maxs.z = GetAbsOrigin().z;
	
	// If we care about not hitting solid entities, look for 'em
	if ( !bIgnoreSolidEntities )
	{
		CBaseEntity *pList[128];

		int count = UTIL_EntitiesInBox( pList, 128, mins, maxs, FL_CLIENT|FL_NPC );
		if ( count )
		{
			//Iterate through the list and check the results
			for ( int i = 0; i < count; i++ )
			{
				//Don't build on top of another entity
				if ( pList[i] == NULL )
					continue;

				//If one of the entities is solid, then we may not be able to spawn now
				if ( ( pList[i]->GetSolidFlags() & FSOLID_NOT_SOLID ) == false )
				{
					// Since the outer method doesn't work well around striders on account of their huge bounding box.
					// Find the ground under me and see if a human hull would fit there.
					trace_t tr;
					UTIL_TraceHull( GetAbsOrigin() + Vector( 0, 0, 2 ),
									GetAbsOrigin() - Vector( 0, 0, 8192 ),
									NAI_Hull::Mins(HULL_HUMAN),
									NAI_Hull::Maxs(HULL_HUMAN),
									MASK_NPCSOLID,
									m_hIgnoreEntity,
									COLLISION_GROUP_NONE,
									&tr );

					if( !HumanHullFits( tr.endpos + Vector( 0, 0, 1 ) ) )
					{
						return false;
					}
				}
			}
		}
	}

	// Do we need to check to see if the player's looking?
	if ( HasSpawnFlags( SF_NPCMAKER_HIDEFROMPLAYER ) )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex(i);
			if ( pPlayer )
			{
				// Only spawn if the player's looking away from me
				if( pPlayer->FInViewCone( GetAbsOrigin() ) && pPlayer->FVisible( GetAbsOrigin() ) )
				{
					if ( !(pPlayer->GetFlags() & FL_NOTARGET) )
						return false;
					DevMsg( 2, "Spawner %s spawning even though seen due to notarget\n", STRING( GetEntityName() ) );
				}
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: If this had a finite number of children, return true if they've all
//			been created.
//-----------------------------------------------------------------------------
bool CBaseNPCMaker::IsDepleted()
{
	if ( (m_spawnflags & SF_NPCMAKER_INF_CHILD) || m_nMaxNumNPCs > 0 )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Toggle the spawner's state
//-----------------------------------------------------------------------------
void CBaseNPCMaker::Toggle( void )
{
	if ( m_bDisabled )
	{
		Enable();
	}
	else
	{
		Disable();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Start the spawner
//-----------------------------------------------------------------------------
void CBaseNPCMaker::Enable( void )
{
	// can't be enabled once depleted
	if ( IsDepleted() )
		return;

	m_bDisabled = false;
	SetThink ( &CBaseNPCMaker::MakerThink );
	SetNextThink( gpGlobals->curtime );
}


//-----------------------------------------------------------------------------
// Purpose: Stop the spawner
//-----------------------------------------------------------------------------
void CBaseNPCMaker::Disable( void )
{
	m_bDisabled = true;
	SetThink ( NULL );
}


//-----------------------------------------------------------------------------
// Purpose: Input handler that spawns an NPC.
//-----------------------------------------------------------------------------
void CBaseNPCMaker::InputSpawnNPC( inputdata_t &inputdata )
{
	if( !IsDepleted() )
	{
		MakeNPC();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Input hander that starts the spawner
//-----------------------------------------------------------------------------
void CBaseNPCMaker::InputEnable( inputdata_t &inputdata )
{
	Enable();
}


//-----------------------------------------------------------------------------
// Purpose: Input hander that stops the spawner
//-----------------------------------------------------------------------------
void CBaseNPCMaker::InputDisable( inputdata_t &inputdata )
{
	Disable();
}


//-----------------------------------------------------------------------------
// Purpose: Input hander that toggles the spawner
//-----------------------------------------------------------------------------
void CBaseNPCMaker::InputToggle( inputdata_t &inputdata )
{
	Toggle();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseNPCMaker::InputSetMaxChildren( inputdata_t &inputdata )
{
	m_nMaxNumNPCs = inputdata.value.Int();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseNPCMaker::InputAddMaxChildren( inputdata_t &inputdata )
{
	m_nMaxNumNPCs += inputdata.value.Int();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseNPCMaker::InputSetMaxLiveChildren( inputdata_t &inputdata )
{
	m_nMaxLiveChildren = inputdata.value.Int();
}

void CBaseNPCMaker::InputSetSpawnFrequency( inputdata_t &inputdata )
{
	m_flSpawnFrequency = inputdata.value.Float();
}

LINK_ENTITY_TO_CLASS( npc_maker, CNPCMaker );

BEGIN_DATADESC( CNPCMaker )

	DEFINE_KEYFIELD( m_iszNPCClassname,		FIELD_STRING,	"NPCType" ),
	DEFINE_KEYFIELD( m_ChildTargetName,		FIELD_STRING,	"NPCTargetname" ),
	DEFINE_KEYFIELD( m_SquadName,			FIELD_STRING,	"NPCSquadName" ),
	DEFINE_KEYFIELD( m_spawnEquipment,		FIELD_STRING,	"additionalequipment" ),
	DEFINE_KEYFIELD( m_strHintGroup,			FIELD_STRING,	"NPCHintGroup" ),
	DEFINE_KEYFIELD( m_RelationshipString,	FIELD_STRING,	"Relationship" ),

END_DATADESC()


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CNPCMaker::CNPCMaker( void )
{
	m_spawnEquipment = NULL_STRING;
}


//-----------------------------------------------------------------------------
// Purpose: Precache the target NPC
//-----------------------------------------------------------------------------
void CNPCMaker::Precache( void )
{
	BaseClass::Precache();

	const char *pszNPCName = STRING( m_iszNPCClassname );
	if ( !pszNPCName || !pszNPCName[0] )
	{
		Warning("npc_maker %s has no specified NPC-to-spawn classname.\n", STRING(GetEntityName()) );
	}
	else
	{
		UTIL_PrecacheOther( pszNPCName );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Creates the NPC.
//-----------------------------------------------------------------------------
void CNPCMaker::MakeNPC( void )
{
	if (!CanMakeNPC())
		return;

	CAI_BaseNPC	*pent = (CAI_BaseNPC*)CreateEntityByName( STRING(m_iszNPCClassname) );

	if ( !pent )
	{
		Warning("NULL Ent in NPCMaker!\n" );
		return;
	}
	
	// ------------------------------------------------
	//  Intialize spawned NPC's relationships
	// ------------------------------------------------
	pent->SetRelationshipString( m_RelationshipString );

	m_OnSpawnNPC.Set( pent, pent, this );

	pent->SetAbsOrigin( GetAbsOrigin() );

	// Strip pitch and roll from the spawner's angles. Pass only yaw to the spawned NPC.
	QAngle angles = GetAbsAngles();
	angles.x = 0.0;
	angles.z = 0.0;
	pent->SetAbsAngles( angles );

	pent->AddSpawnFlags( SF_NPC_FALL_TO_GROUND );

	if ( m_spawnflags & SF_NPCMAKER_FADE )
	{
		pent->AddSpawnFlags( SF_NPC_FADE_CORPSE );
	}

	pent->m_spawnEquipment	= m_spawnEquipment;
	pent->SetSquadName( m_SquadName );
	pent->SetHintGroup( m_strHintGroup );

	ChildPreSpawn( pent );

	DispatchSpawn( pent );
	pent->SetOwnerEntity( this );
	DispatchActivate( pent );

	if ( m_ChildTargetName != NULL_STRING )
	{
		// if I have a netname (overloaded), give the child NPC that name as a targetname
		pent->SetName( m_ChildTargetName );
	}

	ChildPostSpawn( pent );

	m_nLiveChildren++;// count this NPC

	if (!(m_spawnflags & SF_NPCMAKER_INF_CHILD))
	{
		m_nMaxNumNPCs--;

		if ( IsDepleted() )
		{
			m_OnAllSpawned.FireOutput( this, this );

			// Disable this forever.  Don't kill it because it still gets death notices
			SetThink( NULL );
			SetUse( NULL );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pChild - 
//-----------------------------------------------------------------------------
void CBaseNPCMaker::ChildPostSpawn( CAI_BaseNPC *pChild )
{
	// If I'm stuck inside any props, remove them
	bool bFound = true;
	while ( bFound )
	{
		trace_t tr;
		UTIL_TraceHull( pChild->GetAbsOrigin(), pChild->GetAbsOrigin(), pChild->WorldAlignMins(), pChild->WorldAlignMaxs(), MASK_NPCSOLID, pChild, COLLISION_GROUP_NONE, &tr );
		//NDebugOverlay::Box( pChild->GetAbsOrigin(), pChild->WorldAlignMins(), pChild->WorldAlignMaxs(), 0, 255, 0, 32, 5.0 );
		if ( tr.fraction != 1.0 && tr.m_pEnt )
		{
			if ( FClassnameIs( tr.m_pEnt, "prop_physics" ) )
			{
				// Set to non-solid so this loop doesn't keep finding it
				tr.m_pEnt->AddSolidFlags( FSOLID_NOT_SOLID );
				UTIL_RemoveImmediate( tr.m_pEnt );
				continue;
			}
		}

		bFound = false;
	}
	if ( m_hIgnoreEntity != NULL )
	{
		pChild->SetOwnerEntity( m_hIgnoreEntity );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Creates a new NPC every so often.
//-----------------------------------------------------------------------------
void CBaseNPCMaker::MakerThink ( void )
{
	SetNextThink( gpGlobals->curtime + m_flSpawnFrequency );

	MakeNPC();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pVictim - 
//-----------------------------------------------------------------------------
void CBaseNPCMaker::DeathNotice( CBaseEntity *pVictim )
{
	// ok, we've gotten the deathnotice from our child, now clear out its owner if we don't want it to fade.
	m_nLiveChildren--;

	// If we're here, we're getting erroneous death messages from children we haven't created
	AssertMsg( m_nLiveChildren >= 0, "npc_maker receiving child death notice but thinks has no children\n" );

	if ( m_nLiveChildren <= 0 )
	{
		m_OnAllLiveChildrenDead.FireOutput( this, this );

		// See if we've exhausted our supply of NPCs
		if ( ( (m_spawnflags & SF_NPCMAKER_INF_CHILD) == false ) && IsDepleted() )
		{
			// Signal that all our children have been spawned and are now dead
			m_OnAllSpawnedDead.FireOutput( this, this );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Creates new NPCs from a template NPC. The template NPC must be marked
//			as a template (spawnflag) and does not spawn.
//-----------------------------------------------------------------------------

LINK_ENTITY_TO_CLASS( npc_template_maker, CTemplateNPCMaker );

BEGIN_DATADESC( CTemplateNPCMaker )

	DEFINE_KEYFIELD( m_iszTemplateName, FIELD_STRING, "TemplateName" ),
	DEFINE_KEYFIELD( m_flRadius, FIELD_FLOAT, "radius" ),
	DEFINE_FIELD( m_iszTemplateData, FIELD_STRING ),
	DEFINE_KEYFIELD( m_iszDestinationGroup, FIELD_STRING, "DestinationGroup" ),
	DEFINE_KEYFIELD( m_CriterionVisibility, FIELD_INTEGER, "CriterionVisibility" ),
	DEFINE_KEYFIELD( m_CriterionDistance, FIELD_INTEGER, "CriterionDistance" ),
	DEFINE_KEYFIELD( m_iMinSpawnDistance, FIELD_INTEGER, "MinSpawnDistance" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "SpawnNPCInRadius", InputSpawnInRadius ),
	DEFINE_INPUTFUNC( FIELD_VOID, "SpawnNPCInLine", InputSpawnInLine ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SpawnMultiple", InputSpawnMultiple ),
	DEFINE_INPUTFUNC( FIELD_STRING, "ChangeDestinationGroup", InputChangeDestinationGroup ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetMinimumSpawnDistance", InputSetMinimumSpawnDistance ),

END_DATADESC()


//-----------------------------------------------------------------------------
// A hook that lets derived NPC makers do special stuff when precaching.
//-----------------------------------------------------------------------------
void CTemplateNPCMaker::PrecacheTemplateEntity( CBaseEntity *pEntity )
{
	pEntity->Precache();
}


void CTemplateNPCMaker::Precache()
{
	BaseClass::Precache();

	if ( !m_iszTemplateData )
	{
		//
		// This must be the first time we're activated, not a load from save game.
		// Look up the template in the template database.
		//
		if (!m_iszTemplateName)
		{
			Warning( "npc_template_maker %s has no template NPC!\n", STRING(GetEntityName()) );
			UTIL_Remove( this );
			return;
		}
		else
		{
			m_iszTemplateData = Templates_FindByTargetName(STRING(m_iszTemplateName));
			if ( m_iszTemplateData == NULL_STRING )
			{
				DevWarning( "npc_template_maker %s: template NPC %s not found!\n", STRING(GetEntityName()), STRING(m_iszTemplateName) );
				UTIL_Remove( this );
				return;
			}
		}
	}

	Assert( m_iszTemplateData != NULL_STRING );

	// If the mapper marked this as "preload", then instance the entity preache stuff and delete the entity
	//if ( !HasSpawnFlags(SF_NPCMAKER_NOPRELOADMODELS) )
	if ( m_iszTemplateData != NULL_STRING )
	{
		CBaseEntity *pEntity = NULL;
		MapEntity_ParseEntity( pEntity, STRING(m_iszTemplateData), NULL );
		if ( pEntity != NULL )
		{
			PrecacheTemplateEntity( pEntity );
			UTIL_RemoveImmediate( pEntity );
		}
	}
}

#define MAX_DESTINATION_ENTS	100
CNPCSpawnDestination *CTemplateNPCMaker::FindSpawnDestination()
{
	CNPCSpawnDestination *pDestinations[ MAX_DESTINATION_ENTS ];
	CBaseEntity *pEnt = NULL;
	CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
	int	count = 0;

	if( !pPlayer )
	{
		return NULL;
	}

	// Collect all the qualifiying destination ents
	pEnt = gEntList.FindEntityByName( NULL, m_iszDestinationGroup );

	if( !pEnt )
	{
		DevWarning("Template NPC Spawner (%s) doesn't have any spawn destinations!\n", GetDebugName() );
		return NULL;
	}
	
	while( pEnt )
	{
		CNPCSpawnDestination *pDestination;

		pDestination = dynamic_cast <CNPCSpawnDestination*>(pEnt);

		if( pDestination && pDestination->IsAvailable() )
		{
			bool fValid = true;
			Vector vecTest = pDestination->GetAbsOrigin();

			if( m_CriterionVisibility != TS_YN_DONT_CARE )
			{
				// Right now View Cone check is omitted intentionally.
				Vector vecTopOfHull = NAI_Hull::Maxs( HULL_HUMAN );
				vecTopOfHull.x = 0;
				vecTopOfHull.y = 0;
				bool fVisible = (pPlayer->FVisible( vecTest ) || pPlayer->FVisible( vecTest + vecTopOfHull ) );

				if( m_CriterionVisibility == TS_YN_YES )
				{
					if( !fVisible )
						fValid = false;
				}
				else
				{
					if( fVisible )
					{
						if ( !(pPlayer->GetFlags() & FL_NOTARGET) )
							fValid = false;
						else
							DevMsg( 2, "Spawner %s spawning even though seen due to notarget\n", STRING( GetEntityName() ) );
					}
				}
			}

			if( fValid )
			{
				pDestinations[ count ] = pDestination;
				count++;
			}
		}

		pEnt = gEntList.FindEntityByName( pEnt, m_iszDestinationGroup );
	}

	if( count < 1 )
		return NULL;

	// Now find the nearest/farthest based on distance criterion
	if( m_CriterionDistance == TS_DIST_DONT_CARE )
	{
		// Pretty lame way to pick randomly. Try a few times to find a random
		// location where a hull can fit. Don't try too many times due to performance
		// concerns.
		for( int i = 0 ; i < 5 ; i++ )
		{
			CNPCSpawnDestination *pRandomDest = pDestinations[ rand() % count ];

			if( HumanHullFits( pRandomDest->GetAbsOrigin() ) )
			{
				return pRandomDest;
			}
		}

		return NULL;
	}
	else
	{
		if( m_CriterionDistance == TS_DIST_NEAREST )
		{
			float flNearest = FLT_MAX;
			CNPCSpawnDestination *pNearest = NULL;

			for( int i = 0 ; i < count ; i++ )
			{
				Vector vecTest = pDestinations[ i ]->GetAbsOrigin();
				float flDist = ( vecTest - pPlayer->GetAbsOrigin() ).Length();

				if ( m_iMinSpawnDistance != 0 && m_iMinSpawnDistance > flDist )
					continue;

				if( flDist < flNearest && HumanHullFits( vecTest ) )
				{
					flNearest = flDist;
					pNearest = pDestinations[ i ];
				}
			}

			return pNearest;
		}
		else
		{
			float flFarthest = 0;
			CNPCSpawnDestination *pFarthest = NULL;

			for( int i = 0 ; i < count ; i++ )
			{
				Vector vecTest = pDestinations[ i ]->GetAbsOrigin();
				float flDist = ( vecTest - pPlayer->GetAbsOrigin() ).Length();

				if ( m_iMinSpawnDistance != 0 && m_iMinSpawnDistance > flDist )
					continue;

				if( flDist > flFarthest && HumanHullFits( vecTest ) )
				{
					flFarthest = flDist;
					pFarthest = pDestinations[ i ];
				}
			}

			return pFarthest;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTemplateNPCMaker::MakeNPC( void )
{
	// If we should be using the radius spawn method instead, do so
	if ( m_flRadius && HasSpawnFlags(SF_NPCMAKER_ALWAYSUSERADIUS) )
	{
		MakeNPCInRadius();
		return;
	}

	if (!CanMakeNPC( ( m_iszDestinationGroup != NULL_STRING ) ))
		return;

	CNPCSpawnDestination *pDestination = NULL;
	if ( m_iszDestinationGroup != NULL_STRING )
	{
		pDestination = FindSpawnDestination();
		if ( !pDestination )
		{
			DevMsg( 2, "%s '%s' failed to find a valid spawnpoint in destination group: '%s'\n", GetClassname(), STRING(GetEntityName()), STRING(m_iszDestinationGroup) );
			return;
		}
	}

	CAI_BaseNPC	*pent = NULL;
	CBaseEntity *pEntity = NULL;
	MapEntity_ParseEntity( pEntity, STRING(m_iszTemplateData), NULL );
	if ( pEntity != NULL )
	{
		pent = (CAI_BaseNPC *)pEntity;
	}

	if ( !pent )
	{
		Warning("NULL Ent in NPCMaker!\n" );
		return;
	}
	
	if ( pDestination )
	{
		pent->SetAbsOrigin( pDestination->GetAbsOrigin() );

		// Strip pitch and roll from the spawner's angles. Pass only yaw to the spawned NPC.
		QAngle angles = pDestination->GetAbsAngles();
		angles.x = 0.0;
		angles.z = 0.0;
		pent->SetAbsAngles( angles );

		pDestination->OnSpawnedNPC( pent );
	}
	else
	{
		pent->SetAbsOrigin( GetAbsOrigin() );

		// Strip pitch and roll from the spawner's angles. Pass only yaw to the spawned NPC.
		QAngle angles = GetAbsAngles();
		angles.x = 0.0;
		angles.z = 0.0;
		pent->SetAbsAngles( angles );
	}

	m_OnSpawnNPC.Set( pEntity, pEntity, this );

	if ( m_spawnflags & SF_NPCMAKER_FADE )
	{
		pent->AddSpawnFlags( SF_NPC_FADE_CORPSE );
	}

	pent->RemoveSpawnFlags( SF_NPC_TEMPLATE );

	if ( ( m_spawnflags & SF_NPCMAKER_NO_DROP ) == false )
	{
		pent->RemoveSpawnFlags( SF_NPC_FALL_TO_GROUND ); // don't fall, slam
	}

	ChildPreSpawn( pent );

	DispatchSpawn( pent );
	pent->SetOwnerEntity( this );
	DispatchActivate( pent );

	ChildPostSpawn( pent );

	m_nLiveChildren++;// count this NPC

	if (!(m_spawnflags & SF_NPCMAKER_INF_CHILD))
	{
		m_nMaxNumNPCs--;

		if ( IsDepleted() )
		{
			m_OnAllSpawned.FireOutput( this, this );

			// Disable this forever.  Don't kill it because it still gets death notices
			SetThink( NULL );
			SetUse( NULL );
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTemplateNPCMaker::MakeNPCInLine( void )
{
	if (!CanMakeNPC(true))
		return;

	CAI_BaseNPC	*pent = NULL;
	CBaseEntity *pEntity = NULL;
	MapEntity_ParseEntity( pEntity, STRING(m_iszTemplateData), NULL );
	if ( pEntity != NULL )
	{
		pent = (CAI_BaseNPC *)pEntity;
	}

	if ( !pent )
	{
		Warning("NULL Ent in NPCMaker!\n" );
		return;
	}
	
	m_OnSpawnNPC.Set( pEntity, pEntity, this );

	PlaceNPCInLine( pent );

	pent->AddSpawnFlags( SF_NPC_FALL_TO_GROUND );

	pent->RemoveSpawnFlags( SF_NPC_TEMPLATE );
	ChildPreSpawn( pent );

	DispatchSpawn( pent );
	pent->SetOwnerEntity( this );
	DispatchActivate( pent );

	ChildPostSpawn( pent );

	m_nLiveChildren++;// count this NPC

	if (!(m_spawnflags & SF_NPCMAKER_INF_CHILD))
	{
		m_nMaxNumNPCs--;

		if ( IsDepleted() )
		{
			m_OnAllSpawned.FireOutput( this, this );

			// Disable this forever.  Don't kill it because it still gets death notices
			SetThink( NULL );
			SetUse( NULL );
		}
	}
}

//-----------------------------------------------------------------------------
bool CTemplateNPCMaker::PlaceNPCInLine( CAI_BaseNPC *pNPC )
{
	Vector vecPlace;
	Vector vecLine;

	GetVectors( &vecLine, NULL, NULL );

	// invert this, line up NPC's BEHIND the maker.
	vecLine *= -1;

	trace_t tr;
	UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() - Vector( 0, 0, 8192 ), MASK_SHOT, pNPC, COLLISION_GROUP_NONE, &tr );
	vecPlace = tr.endpos;
	float flStepSize = pNPC->GetHullWidth();

	// Try 10 times to place this npc.
	for( int i = 0 ; i < 10 ; i++ )
	{
		UTIL_TraceHull( vecPlace,
						vecPlace + Vector( 0, 0, 10 ),
						pNPC->GetHullMins(),
						pNPC->GetHullMaxs(),
						MASK_SHOT,
						pNPC,
						COLLISION_GROUP_NONE,
						&tr );

		if( tr.fraction == 1.0 )
		{
			pNPC->SetAbsOrigin( tr.endpos );
			return true;
		}

		vecPlace += vecLine * flStepSize;
	}

	DevMsg("**Failed to place NPC in line!\n");
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Place NPC somewhere on the perimeter of my radius.
//-----------------------------------------------------------------------------
void CTemplateNPCMaker::MakeNPCInRadius( void )
{
	if ( !CanMakeNPC(true))
		return;

	CAI_BaseNPC	*pent = NULL;
	CBaseEntity *pEntity = NULL;
	MapEntity_ParseEntity( pEntity, STRING(m_iszTemplateData), NULL );
	if ( pEntity != NULL )
	{
		pent = (CAI_BaseNPC *)pEntity;
	}

	if ( !pent )
	{
		Warning("NULL Ent in NPCMaker!\n" );
		return;
	}
	
	if ( !PlaceNPCInRadius( pent ) )
	{
		// Failed to place the NPC. Abort
		UTIL_RemoveImmediate( pent );
		return;
	}

	m_OnSpawnNPC.Set( pEntity, pEntity, this );

	pent->AddSpawnFlags( SF_NPC_FALL_TO_GROUND );

	pent->RemoveSpawnFlags( SF_NPC_TEMPLATE );
	ChildPreSpawn( pent );

	DispatchSpawn( pent );

	pent->SetOwnerEntity( this );
	DispatchActivate( pent );

	ChildPostSpawn( pent );

	m_nLiveChildren++;// count this NPC

	if (!(m_spawnflags & SF_NPCMAKER_INF_CHILD))
	{
		m_nMaxNumNPCs--;

		if ( IsDepleted() )
		{
			m_OnAllSpawned.FireOutput( this, this );

			// Disable this forever.  Don't kill it because it still gets death notices
			SetThink( NULL );
			SetUse( NULL );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Find a place to spawn an npc within my radius.
//			Right now this function tries to place them on the perimeter of radius.
// Output : false if we couldn't find a spot!
//-----------------------------------------------------------------------------
bool CTemplateNPCMaker::PlaceNPCInRadius( CAI_BaseNPC *pNPC )
{
	Vector vPos;

	if ( CAI_BaseNPC::FindSpotForNPCInRadius( &vPos, GetAbsOrigin(), pNPC, m_flRadius ) )
	{
		pNPC->SetAbsOrigin( vPos );
		return true;
	}

	DevMsg("**Failed to place NPC in radius!\n");
	return false;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTemplateNPCMaker::MakeMultipleNPCS( int nNPCs )
{
	bool bInRadius = ( m_iszDestinationGroup == NULL_STRING && m_flRadius > 0.1 );
	while ( nNPCs-- )
	{
		if ( !bInRadius )
		{
			MakeNPC();
		}
		else
		{
			MakeNPCInRadius();
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTemplateNPCMaker::InputSpawnMultiple( inputdata_t &inputdata )
{
	MakeMultipleNPCS( inputdata.value.Int() );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTemplateNPCMaker::InputChangeDestinationGroup( inputdata_t &inputdata )
{
	m_iszDestinationGroup = inputdata.value.StringID();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CTemplateNPCMaker::InputSetMinimumSpawnDistance( inputdata_t &inputdata )
{
	m_iMinSpawnDistance = inputdata.value.Int();
}

// HL1

// Monstermaker spawnflags
#define	SF_MONSTERMAKER_START_ON	1 // start active ( if has targetname )
#define	SF_MONSTERMAKER_CYCLIC		4 // drop one monster every time fired.
#define SF_MONSTERMAKER_MONSTERCLIP	8 // Children are blocked by monsterclip

//=========================================================
// MonsterMaker - this ent creates monsters during the game.
//=========================================================
class CMonsterMaker : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void KeyValue( KeyValueData* pkvd);
	void EXPORT ToggleUse ( CBaseEntityHL1 *pActivator, CBaseEntityHL1 *pCaller, USE_TYPE useType, float value );
	void EXPORT CyclicUse ( CBaseEntityHL1 *pActivator, CBaseEntityHL1 *pCaller, USE_TYPE useType, float value );
	void EXPORT MakerThink ( void );
	void DeathNotice ( entvars_t *pevChild );// monster maker children use this to tell the monster maker that they have died.
	void MakeMonster( void );

	virtual int		SaveHL1( CSaveHL1 &save );
	virtual int		RestoreHL1( CRestoreHL1 &restore );

	static	TYPEDESCRIPTION m_SaveData[];
	
	string_t m_iszMonsterClassname;// classname of the monster(s) that will be created.
	
	int	 m_cNumMonsters;// max number of monsters this ent can create

	
	int  m_cLiveChildren;// how many monsters made by this monster maker that are currently alive
	int	 m_iMaxLiveChildren;// max number of monsters that this maker may have out at one time.

	float m_flGround; // z coord of the ground under me, used to make sure no monsters are under the maker when it drops a new child

	BOOL m_fActive;
	BOOL m_fFadeChildren;// should we make the children fadeout?
};

LINK_ENTITY_TO_CLASS( monstermaker, CMonsterMaker );

TYPEDESCRIPTION	CMonsterMaker::m_SaveData[] = 
{
	DEFINE_FIELD( CMonsterMaker, m_iszMonsterClassname, FIELD_STRING ),
	DEFINE_FIELD( CMonsterMaker, m_cNumMonsters, FIELD_INTEGER ),
	DEFINE_FIELD( CMonsterMaker, m_cLiveChildren, FIELD_INTEGER ),
	DEFINE_FIELD( CMonsterMaker, m_flGround, FIELD_FLOAT ),
	DEFINE_FIELD( CMonsterMaker, m_iMaxLiveChildren, FIELD_INTEGER ),
	DEFINE_FIELD( CMonsterMaker, m_fActive, FIELD_BOOLEAN ),
	DEFINE_FIELD( CMonsterMaker, m_fFadeChildren, FIELD_BOOLEAN ),
};


IMPLEMENT_SAVERESTORE( CMonsterMaker, CBaseMonster );

void CMonsterMaker :: KeyValue( KeyValueData *pkvd )
{
	
	if ( FStrEq(pkvd->szKeyName, "monstercount") )
	{
		m_cNumMonsters = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "m_imaxlivechildren") )
	{
		m_iMaxLiveChildren = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if ( FStrEq(pkvd->szKeyName, "monstertype") )
	{
		m_iszMonsterClassname = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseMonster::KeyValue( pkvd );
}


void CMonsterMaker :: Spawn( )
{
	pev->solid = SOLID_NOT;

	m_cLiveChildren = 0;
	Precache();
	if ( !FStringNull ( pev->targetname ) )
	{
		if ( pev->spawnflags & SF_MONSTERMAKER_CYCLIC )
		{
			SetUseHL1 ( CyclicUse );// drop one monster each time we fire
		}
		else
		{
			SetUseHL1 ( ToggleUse );// so can be turned on/off
		}

		if ( FBitSet ( pev->spawnflags, SF_MONSTERMAKER_START_ON ) )
		{// start making monsters as soon as monstermaker spawns
			m_fActive = TRUE;
			SetThinkHL1 ( MakerThink );
		}
		else
		{// wait to be activated.
			m_fActive = FALSE;
			SetThinkHL1 ( SUB_DoNothing );
		}
	}
	else
	{// no targetname, just start.
			pev->nextthink = gpGlobalsHL1->time + m_flDelay;
			m_fActive = TRUE;
			SetThinkHL1 ( MakerThink );
	}

	if ( m_cNumMonsters == 1 )
	{
		m_fFadeChildren = FALSE;
	}
	else
	{
		m_fFadeChildren = TRUE;
	}

	m_flGround = 0;
}

void CMonsterMaker :: Precache( void )
{
	CBaseMonster::Precache();

	UTIL_PrecacheOther( STRING_HL1( m_iszMonsterClassname ) );
}

//=========================================================
// MakeMonster-  this is the code that drops the monster
//=========================================================
void CMonsterMaker::MakeMonster( void )
{
	edict_t_hl1	*pent;
	entvars_t		*pevCreate;

	if ( m_iMaxLiveChildren > 0 && m_cLiveChildren >= m_iMaxLiveChildren )
	{// not allowed to make a new one yet. Too many live ones out right now.
		return;
	}

	if ( !m_flGround )
	{
		// set altitude. Now that I'm activated, any breakables, etc should be out from under me. 
		TraceResult tr;

		UTIL_TraceLine ( pev->origin, pev->origin - VectorHL1 ( 0, 0, 2048 ), ignore_monsters, ENT(pev), &tr );
		m_flGround = tr.vecEndPos.z;
	}

	VectorHL1 mins = pev->origin - VectorHL1( 34, 34, 0 );
	VectorHL1 maxs = pev->origin + VectorHL1( 34, 34, 0 );
	maxs.z = pev->origin.z;
	mins.z = m_flGround;

	CBaseEntityHL1 *pList[2];
	int count = UTIL_EntitiesInBox( pList, 2, mins, maxs, FL_CLIENT|FL_MONSTER );
	if ( count )
	{
		// don't build a stack of monsters!
		return;
	}

	pent = CREATE_NAMED_ENTITY( m_iszMonsterClassname );

	if ( FNullEnt( pent ) )
	{
		ALERT ( at_console, "NULL Ent in MonsterMaker!\n" );
		return;
	}
	
	// If I have a target, fire!
	if ( !FStringNull ( pev->target ) )
	{
		// delay already overloaded for this entity, so can't call SUB_UseTargets()
		FireTargets( STRING_HL1(pev->target), this, this, USE_TOGGLE, 0 );
	}

	pevCreate = VARS( pent );
	pevCreate->origin = pev->origin;
	pevCreate->angles = pev->angles;
	SetBits( pevCreate->spawnflags, SF_MONSTER_FALL_TO_GROUND );

	// Children hit monsterclip brushes
	if ( pev->spawnflags & SF_MONSTERMAKER_MONSTERCLIP )
		SetBits( pevCreate->spawnflags, SF_MONSTER_HITMONSTERCLIP );

	DispatchSpawn( ENT( pevCreate ) );
	pevCreate->owner = edict();

	if ( !FStringNull( pev->netname ) )
	{
		// if I have a netname (overloaded), give the child monster that name as a targetname
		pevCreate->targetname = pev->netname;
	}

	m_cLiveChildren++;// count this monster
	m_cNumMonsters--;

	if ( m_cNumMonsters == 0 )
	{
		// Disable this forever.  Don't kill it because it still gets death notices
		SetThinkHL1( NULL );
		SetUseHL1( NULL );
	}
}

//=========================================================
// CyclicUse - drops one monster from the monstermaker
// each time we call this.
//=========================================================
void CMonsterMaker::CyclicUse ( CBaseEntityHL1 *pActivator, CBaseEntityHL1 *pCaller, USE_TYPE useType, float value )
{
	MakeMonster();
}

//=========================================================
// ToggleUse - activates/deactivates the monster maker
//=========================================================
void CMonsterMaker :: ToggleUse ( CBaseEntityHL1 *pActivator, CBaseEntityHL1 *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_fActive ) )
		return;

	if ( m_fActive )
	{
		m_fActive = FALSE;
		SetThinkHL1 ( NULL );
	}
	else
	{
		m_fActive = TRUE;
		SetThinkHL1 ( MakerThink );
	}

	pev->nextthink = gpGlobals->time;
}

//=========================================================
// MakerThink - creates a new monster every so often
//=========================================================
void CMonsterMaker :: MakerThink ( void )
{
	pev->nextthink = gpGlobalsHL1->time + m_flDelay;

	MakeMonster();
}


//=========================================================
//=========================================================
void CMonsterMaker :: DeathNotice ( entvars_t *pevChild )
{
	// ok, we've gotten the deathnotice from our child, now clear out its owner if we don't want it to fade.
	m_cLiveChildren--;

	if ( !m_fFadeChildren )
	{
		pevChild->owner = NULL;
	}
}
