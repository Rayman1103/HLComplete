//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Utility code.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "saverestore.h"
#include "globalstate.h"
#include <stdarg.h>
#include "shake.h"
#include "decals.h"
#include "player.h"
#include "gamerules.h"
#include "entitylist.h"
#include "bspfile.h"
#include "mathlib/mathlib.h"
#include "IEffects.h"
#include "vstdlib/random.h"
#include "soundflags.h"
#include "ispatialpartition.h"
#include "igamesystem.h"
#include "saverestoretypes.h"
#include "checksum_crc.h"
#include "hierarchy.h"
#include "iservervehicle.h"
#include "te_effect_dispatch.h"
#include "utldict.h"
#include "collisionutils.h"
#include "movevars_shared.h"
#include "inetchannelinfo.h"
#include "tier0/vprof.h"
#include "ndebugoverlay.h"
#include "engine/ivdebugoverlay.h"
#include "datacache/imdlcache.h"
#include "util.h"
#include "cdll_int.h"

//HL1
#include "extdll.h"
#include <time.h>
#include "weapons.h"

#ifdef PORTAL
#include "PortalSimulation.h"
//#include "Portal_PhysicsEnvironmentMgr.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern short		g_sModelIndexSmoke;			// (in combatweapon.cpp) holds the index for the smoke cloud
extern short		g_sModelIndexBloodDrop;		// (in combatweapon.cpp) holds the sprite index for the initial blood
extern short		g_sModelIndexBloodSpray;	// (in combatweapon.cpp) holds the sprite index for splattered blood

#ifdef	DEBUG
void DBG_AssertFunction( bool fExpr, const char *szExpr, const char *szFile, int szLine, const char *szMessage )
{
	if (fExpr)
		return;
	char szOut[512];
	if (szMessage != NULL)
		Q_snprintf(szOut,sizeof(szOut), "ASSERT FAILED:\n %s \n(%s@%d)\n%s", szExpr, szFile, szLine, szMessage);
	else
		Q_snprintf(szOut,sizeof(szOut), "ASSERT FAILED:\n %s \n(%s@%d)\n", szExpr, szFile, szLine);
	Warning( szOut);
}
#endif	// DEBUG


//-----------------------------------------------------------------------------
// Entity creation factory
//-----------------------------------------------------------------------------
class CEntityFactoryDictionary : public IEntityFactoryDictionary
{
public:
	CEntityFactoryDictionary();

	virtual void InstallFactory( IEntityFactory *pFactory, const char *pClassName );
	virtual IServerNetworkable *Create( const char *pClassName );
	virtual void Destroy( const char *pClassName, IServerNetworkable *pNetworkable );
	virtual const char *GetCannonicalName( const char *pClassName );
	void ReportEntitySizes();

private:
	IEntityFactory *FindFactory( const char *pClassName );
public:
	CUtlDict< IEntityFactory *, unsigned short > m_Factories;
};

//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
IEntityFactoryDictionary *EntityFactoryDictionary()
{
	static CEntityFactoryDictionary s_EntityFactory;
	return &s_EntityFactory;
}

void DumpEntityFactories_f()
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	CEntityFactoryDictionary *dict = ( CEntityFactoryDictionary * )EntityFactoryDictionary();
	if ( dict )
	{
		for ( int i = dict->m_Factories.First(); i != dict->m_Factories.InvalidIndex(); i = dict->m_Factories.Next( i ) )
		{
			Warning( "%s\n", dict->m_Factories.GetElementName( i ) );
		}
	}
}

static ConCommand dumpentityfactories( "dumpentityfactories", DumpEntityFactories_f, "Lists all entity factory names.", FCVAR_GAMEDLL );


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CON_COMMAND( dump_entity_sizes, "Print sizeof(entclass)" )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	((CEntityFactoryDictionary*)EntityFactoryDictionary())->ReportEntitySizes();
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CEntityFactoryDictionary::CEntityFactoryDictionary() : m_Factories( true, 0, 128 )
{
}


//-----------------------------------------------------------------------------
// Finds a new factory
//-----------------------------------------------------------------------------
IEntityFactory *CEntityFactoryDictionary::FindFactory( const char *pClassName )
{
	unsigned short nIndex = m_Factories.Find( pClassName );
	if ( nIndex == m_Factories.InvalidIndex() )
		return NULL;
	return m_Factories[nIndex];
}


//-----------------------------------------------------------------------------
// Install a new factory
//-----------------------------------------------------------------------------
void CEntityFactoryDictionary::InstallFactory( IEntityFactory *pFactory, const char *pClassName )
{
	Assert( FindFactory( pClassName ) == NULL );
	m_Factories.Insert( pClassName, pFactory );
}


//-----------------------------------------------------------------------------
// Instantiate something using a factory
//-----------------------------------------------------------------------------
IServerNetworkable *CEntityFactoryDictionary::Create( const char *pClassName )
{
	IEntityFactory *pFactory = FindFactory( pClassName );
	if ( !pFactory )
	{
		Warning("Attempted to create unknown entity type %s!\n", pClassName );
		return NULL;
	}
#if defined(TRACK_ENTITY_MEMORY) && defined(USE_MEM_DEBUG)
	MEM_ALLOC_CREDIT_( m_Factories.GetElementName( m_Factories.Find( pClassName ) ) );
#endif
	return pFactory->Create( pClassName );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
const char *CEntityFactoryDictionary::GetCannonicalName( const char *pClassName )
{
	return m_Factories.GetElementName( m_Factories.Find( pClassName ) );
}

//-----------------------------------------------------------------------------
// Destroy a networkable
//-----------------------------------------------------------------------------
void CEntityFactoryDictionary::Destroy( const char *pClassName, IServerNetworkable *pNetworkable )
{
	IEntityFactory *pFactory = FindFactory( pClassName );
	if ( !pFactory )
	{
		Warning("Attempted to destroy unknown entity type %s!\n", pClassName );
		return;
	}

	pFactory->Destroy( pNetworkable );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CEntityFactoryDictionary::ReportEntitySizes()
{
	for ( int i = m_Factories.First(); i != m_Factories.InvalidIndex(); i = m_Factories.Next( i ) )
	{
		Msg( " %s: %d", m_Factories.GetElementName( i ), m_Factories[i]->GetEntitySize() );
	}
}


//-----------------------------------------------------------------------------
// class CFlaggedEntitiesEnum
//-----------------------------------------------------------------------------

CFlaggedEntitiesEnum::CFlaggedEntitiesEnum( CBaseEntity **pList, int listMax, int flagMask )
{
	m_pList = pList;
	m_listMax = listMax;
	m_flagMask = flagMask;
	m_count = 0;
}

bool CFlaggedEntitiesEnum::AddToList( CBaseEntity *pEntity )
{
	if ( m_count >= m_listMax )
	{
		AssertMsgOnce( 0, "reached enumerated list limit.  Increase limit, decrease radius, or make it so entity flags will work for you" );
		return false;
	}
	m_pList[m_count] = pEntity;
	m_count++;
	return true;
}

IterationRetval_t CFlaggedEntitiesEnum::EnumElement( IHandleEntity *pHandleEntity )
{
	CBaseEntity *pEntity = gEntList.GetBaseEntity( pHandleEntity->GetRefEHandle() );
	if ( pEntity )
	{
		if ( m_flagMask && !(pEntity->GetFlags() & m_flagMask) )	// Does it meet the criteria?
			return ITERATION_CONTINUE;

		if ( !AddToList( pEntity ) )
			return ITERATION_STOP;
	}

	return ITERATION_CONTINUE;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int UTIL_PrecacheDecal( const char *name, bool preload )
{
	// If this is out of order, make sure to warn.
	if ( !CBaseEntity::IsPrecacheAllowed() )
	{
		if ( !engine->IsDecalPrecached( name ) )
		{
			Assert( !"UTIL_PrecacheDecal:  too late" );

			Warning( "Late precache of %s\n", name );
		}
	}

	return engine->PrecacheDecal( name, preload );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float UTIL_GetSimulationInterval()
{
	if ( CBaseEntity::IsSimulatingOnAlternateTicks() )
		return ( TICK_INTERVAL * 2.0 );
	return TICK_INTERVAL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int UTIL_EntitiesInBox( const Vector &mins, const Vector &maxs, CFlaggedEntitiesEnum *pEnum )
{
	partition->EnumerateElementsInBox( PARTITION_ENGINE_NON_STATIC_EDICTS, mins, maxs, false, pEnum );
	return pEnum->GetCount();
}

int UTIL_EntitiesAlongRay( const Ray_t &ray, CFlaggedEntitiesEnum *pEnum )
{
	partition->EnumerateElementsAlongRay( PARTITION_ENGINE_NON_STATIC_EDICTS, ray, false, pEnum );
	return pEnum->GetCount();
}

int UTIL_EntitiesInSphere( const Vector &center, float radius, CFlaggedEntitiesEnum *pEnum )
{
	partition->EnumerateElementsInSphere( PARTITION_ENGINE_NON_STATIC_EDICTS, center, radius, false, pEnum );
	return pEnum->GetCount();
}

CEntitySphereQuery::CEntitySphereQuery( const Vector &center, float radius, int flagMask )
{
	m_listIndex = 0;
	m_listCount = UTIL_EntitiesInSphere( m_pList, ARRAYSIZE(m_pList), center, radius, flagMask );
}

CBaseEntity *CEntitySphereQuery::GetCurrentEntity()
{
	if ( m_listIndex < m_listCount )
		return m_pList[m_listIndex];
	return NULL;
}


//-----------------------------------------------------------------------------
// Simple trace filter
//-----------------------------------------------------------------------------
class CTracePassFilter : public CTraceFilter
{
public:
	CTracePassFilter( IHandleEntity *pPassEnt ) : m_pPassEnt( pPassEnt ) {}

	bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		if ( !StandardFilterRules( pHandleEntity, contentsMask ) )
			return false;

		if (!PassServerEntityFilter( pHandleEntity, m_pPassEnt ))
			return false;

		return true;
	}

private:
	IHandleEntity *m_pPassEnt;
};


//-----------------------------------------------------------------------------
// Drops an entity onto the floor
//-----------------------------------------------------------------------------
int UTIL_DropToFloor( CBaseEntity *pEntity, unsigned int mask, CBaseEntity *pIgnore )
{
	// Assume no ground
	pEntity->SetGroundEntity( NULL );

	Assert( pEntity );

	trace_t	trace;

#ifndef HL2MP
	// HACK: is this really the only sure way to detect crossing a terrain boundry?
	UTIL_TraceEntity( pEntity, pEntity->GetAbsOrigin(), pEntity->GetAbsOrigin(), mask, pIgnore, pEntity->GetCollisionGroup(), &trace );
	if (trace.fraction == 0.0)
		return -1;
#endif // HL2MP

	UTIL_TraceEntity( pEntity, pEntity->GetAbsOrigin(), pEntity->GetAbsOrigin() - Vector(0,0,256), mask, pIgnore, pEntity->GetCollisionGroup(), &trace );

	if (trace.allsolid)
		return -1;

	if (trace.fraction == 1)
		return 0;

	pEntity->SetAbsOrigin( trace.endpos );
	pEntity->SetGroundEntity( trace.m_pEnt );

	return 1;
}

//-----------------------------------------------------------------------------
// Returns false if any part of the bottom of the entity is off an edge that
// is not a staircase.
//-----------------------------------------------------------------------------
bool UTIL_CheckBottom( CBaseEntity *pEntity, ITraceFilter *pTraceFilter, float flStepSize )
{
	Vector	mins, maxs, start, stop;
	trace_t	trace;
	int		x, y;
	float	mid, bottom;

	Assert( pEntity );

	CTracePassFilter traceFilter(pEntity);
	if ( !pTraceFilter )
	{
		pTraceFilter = &traceFilter;
	}

	unsigned int mask = pEntity->PhysicsSolidMaskForEntity();

	VectorAdd (pEntity->GetAbsOrigin(), pEntity->WorldAlignMins(), mins);
	VectorAdd (pEntity->GetAbsOrigin(), pEntity->WorldAlignMaxs(), maxs);

	// if all of the points under the corners are solid world, don't bother
	// with the tougher checks
	// the corners must be within 16 of the midpoint
	start[2] = mins[2] - 1;
	for	(x=0 ; x<=1 ; x++)
	{
		for	(y=0 ; y<=1 ; y++)
		{
			start[0] = x ? maxs[0] : mins[0];
			start[1] = y ? maxs[1] : mins[1];
			if (enginetrace->GetPointContents(start) != CONTENTS_SOLID)
				goto realcheck;
		}
	}
	return true;		// we got out easy

realcheck:
	// check it for real...
	start[2] = mins[2] + flStepSize; // seems to help going up/down slopes.
	
	// the midpoint must be within 16 of the bottom
	start[0] = stop[0] = (mins[0] + maxs[0])*0.5;
	start[1] = stop[1] = (mins[1] + maxs[1])*0.5;
	stop[2] = start[2] - 2*flStepSize;
	
	UTIL_TraceLine( start, stop, mask, pTraceFilter, &trace );

	if (trace.fraction == 1.0)
		return false;
	mid = bottom = trace.endpos[2];

	// the corners must be within 16 of the midpoint	
	for	(x=0 ; x<=1 ; x++)
	{
		for	(y=0 ; y<=1 ; y++)
		{
			start[0] = stop[0] = x ? maxs[0] : mins[0];
			start[1] = stop[1] = y ? maxs[1] : mins[1];
			
			UTIL_TraceLine( start, stop, mask, pTraceFilter, &trace );
			
			if (trace.fraction != 1.0 && trace.endpos[2] > bottom)
				bottom = trace.endpos[2];
			if (trace.fraction == 1.0 || mid - trace.endpos[2] > flStepSize)
				return false;
		}
	}
	return true;
}



bool g_bDisableEhandleAccess = false;
bool g_bReceivedChainedUpdateOnRemove = false;
//-----------------------------------------------------------------------------
// Purpose: Sets the entity up for deletion.  Entity will not actually be deleted
//			until the next frame, so there can be no pointer errors.
// Input  : *oldObj - object to delete
//-----------------------------------------------------------------------------
void UTIL_Remove( IServerNetworkable *oldObj )
{
	CServerNetworkProperty* pProp = static_cast<CServerNetworkProperty*>( oldObj );
	if ( !pProp || pProp->IsMarkedForDeletion() )
		return;

	if ( PhysIsInCallback() )
	{
		// This assert means that someone is deleting an entity inside a callback.  That isn't supported so
		// this code will defer the deletion of that object until the end of the current physics simulation frame
		// Since this is hidden from the calling code it's preferred to call PhysCallbackRemove() directly from the caller
		// in case the deferred delete will have unwanted results (like continuing to receive callbacks).  That will make it 
		// obvious why the unwanted results are happening so the caller can handle them appropriately. (some callbacks can be masked 
		// or the calling entity can be flagged to filter them in most cases)
		Assert(0);
		PhysCallbackRemove(oldObj);
		return;
	}

	// mark it for deletion	
	pProp->MarkForDeletion( );

	CBaseEntity *pBaseEnt = oldObj->GetBaseEntity();
	if ( pBaseEnt )
	{
#ifdef PORTAL //make sure entities are in the primary physics environment for the portal mod, this code should be safe even if the entity is in neither extra environment
		CPortalSimulator::Pre_UTIL_Remove( pBaseEnt );
#endif
		g_bReceivedChainedUpdateOnRemove = false;
		pBaseEnt->UpdateOnRemove();

		Assert( g_bReceivedChainedUpdateOnRemove );

		// clear oldObj targetname / other flags now
		pBaseEnt->SetName( NULL_STRING );

#ifdef PORTAL
		CPortalSimulator::Post_UTIL_Remove( pBaseEnt );
#endif
	}

	gEntList.AddToDeleteList( oldObj );
}

void UTIL_Remove( CBaseEntity *oldObj )
{
	if ( !oldObj )
		return;
	UTIL_Remove( oldObj->NetworkProp() );
}

static int s_RemoveImmediateSemaphore = 0;
void UTIL_DisableRemoveImmediate()
{
	s_RemoveImmediateSemaphore++;
}
void UTIL_EnableRemoveImmediate()
{
	s_RemoveImmediateSemaphore--;
	Assert(s_RemoveImmediateSemaphore>=0);
}
//-----------------------------------------------------------------------------
// Purpose: deletes an entity, without any delay.  WARNING! Only use this when sure
//			no pointers rely on this entity.
// Input  : *oldObj - the entity to delete
//-----------------------------------------------------------------------------
void UTIL_RemoveImmediate( CBaseEntity *oldObj )
{
	// valid pointer or already removed?
	if ( !oldObj || oldObj->IsEFlagSet(EFL_KILLME) )
		return;

	if ( s_RemoveImmediateSemaphore )
	{
		UTIL_Remove(oldObj);
		return;
	}

#ifdef PORTAL //make sure entities are in the primary physics environment for the portal mod, this code should be safe even if the entity is in neither extra environment
	CPortalSimulator::Pre_UTIL_Remove( oldObj );
#endif

	oldObj->AddEFlags( EFL_KILLME );	// Make sure to ignore further calls into here or UTIL_Remove.

	g_bReceivedChainedUpdateOnRemove = false;
	oldObj->UpdateOnRemove();
	Assert( g_bReceivedChainedUpdateOnRemove );

	// Entities shouldn't reference other entities in their destructors
	//  that type of code should only occur in an UpdateOnRemove call
	g_bDisableEhandleAccess = true;
	delete oldObj;
	g_bDisableEhandleAccess = false;

#ifdef PORTAL
	CPortalSimulator::Post_UTIL_Remove( oldObj );
#endif
}


// returns a CBaseEntity pointer to a player by index.  Only returns if the player is spawned and connected
// otherwise returns NULL
// Index is 1 based
CBasePlayer	*UTIL_PlayerByIndex( int playerIndex )
{
	CBasePlayer *pPlayer = NULL;

	if ( playerIndex > 0 && playerIndex <= gpGlobals->maxClients )
	{
		edict_t *pPlayerEdict = INDEXENT( playerIndex );
		if ( pPlayerEdict && !pPlayerEdict->IsFree() )
		{
			pPlayer = (CBasePlayer*)GetContainingEntity( pPlayerEdict );
		}
	}
	
	return pPlayer;
}

CBasePlayer* UTIL_PlayerByName( const char *name )
{
	if ( !name || !name[0] )
		return NULL;

	for (int i = 1; i<=gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		
		if ( !pPlayer )
			continue;

		if ( !pPlayer->IsConnected() )
			continue;

		if ( Q_stricmp( pPlayer->GetPlayerName(), name ) == 0 )
		{
			return pPlayer;
		}
	}
	
	return NULL;
}

CBasePlayer* UTIL_PlayerByUserId( int userID )
{
	for (int i = 1; i<=gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		
		if ( !pPlayer )
			continue;

		if ( !pPlayer->IsConnected() )
			continue;

		if ( engine->GetPlayerUserId(pPlayer->edict()) == userID )
		{
			return pPlayer;
		}
	}
	
	return NULL;
}

//
// Return the local player.
// If this is a multiplayer game, return NULL.
// 
CBasePlayer *UTIL_GetLocalPlayer( void )
{
	if ( gpGlobals->maxClients > 1 )
	{
		if ( developer.GetBool() )
		{
			Assert( !"UTIL_GetLocalPlayer" );
			
#ifdef	DEBUG
			Warning( "UTIL_GetLocalPlayer() called in multiplayer game.\n" );
#endif
		}

		return NULL;
	}

	return UTIL_PlayerByIndex( 1 );
}

//
// Get the local player on a listen server - this is for multiplayer use only
// 
CBasePlayer *UTIL_GetListenServerHost( void )
{
	// no "local player" if this is a dedicated server or a single player game
	if (engine->IsDedicatedServer())
	{
		Assert( !"UTIL_GetListenServerHost" );
		Warning( "UTIL_GetListenServerHost() called from a dedicated server or single-player game.\n" );
		return NULL;
	}

	return UTIL_PlayerByIndex( 1 );
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Returns true if the command was issued by the listenserver host, or by the dedicated server, via rcon or the server console.
 * This is valid during ConCommand execution.
 */
bool UTIL_IsCommandIssuedByServerAdmin( void )
{
	int issuingPlayerIndex = UTIL_GetCommandClientIndex();

	if ( engine->IsDedicatedServer() && issuingPlayerIndex > 0 )
		return false;

#if defined( REPLAY_ENABLED )
	// entity 1 is replay?
	player_info_t pi;
	bool bPlayerIsReplay = engine->GetPlayerInfo( 1, &pi ) && pi.isreplay;
#else
	bool bPlayerIsReplay = false;
#endif

	if ( bPlayerIsReplay )
	{
		if ( issuingPlayerIndex > 2 )
			return false;
	}
	else if ( issuingPlayerIndex > 1 )
	{
		return false;
	}

	return true;
}


//--------------------------------------------------------------------------------------------------------------
/**
 * Returns a CBaseEntity pointer by entindex.  Index is 1 based.
 */
CBaseEntity	*UTIL_EntityByIndex( int entityIndex )
{
	CBaseEntity *entity = NULL;

	if ( entityIndex > 0 )
	{
		edict_t *edict = INDEXENT( entityIndex );
		if ( edict && !edict->IsFree() )
		{
			entity = GetContainingEntity( edict );
		}
	}
	
	return entity;
}


int ENTINDEX( CBaseEntity *pEnt )
{
	// This works just like ENTINDEX for edicts.
	if ( pEnt )
		return pEnt->entindex();
	else
		return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : playerIndex - 
//			ping - 
//			packetloss - 
//-----------------------------------------------------------------------------
void UTIL_GetPlayerConnectionInfo( int playerIndex, int& ping, int &packetloss )
{
	CBasePlayer *player =  UTIL_PlayerByIndex( playerIndex );

	INetChannelInfo *nci = engine->GetPlayerNetInfo(playerIndex);

	if ( nci && player && !player->IsBot() )
	{
		float latency = nci->GetAvgLatency( FLOW_OUTGOING ); // in seconds
		
		// that should be the correct latency, we assume that cmdrate is higher 
		// then updaterate, what is the case for default settings
		const char * szCmdRate = engine->GetClientConVarValue( playerIndex, "cl_cmdrate" );
		
		int nCmdRate = MAX( 1, Q_atoi( szCmdRate ) );
		latency -= (0.5f/nCmdRate) + TICKS_TO_TIME( 1.0f ); // correct latency

		// in GoldSrc we had a different, not fixed tickrate. so we have to adjust
		// Source pings by half a tick to match the old GoldSrc pings.
		latency -= TICKS_TO_TIME( 0.5f );

		ping = latency * 1000.0f; // as msecs
		ping = clamp( ping, 5, 1000 ); // set bounds, dont show pings under 5 msecs
		
		packetloss = 100.0f * nci->GetAvgLoss( FLOW_INCOMING ); // loss in percentage
		packetloss = clamp( packetloss, 0, 100 );
	}
	else
	{
		ping = 0;
		packetloss = 0;
	}
}

static unsigned short FixedUnsigned16( float value, float scale )
{
	int output;

	output = value * scale;
	if ( output < 0 )
		output = 0;
	if ( output > 0xFFFF )
		output = 0xFFFF;

	return (unsigned short)output;
}


//-----------------------------------------------------------------------------
// Compute shake amplitude
//-----------------------------------------------------------------------------
inline float ComputeShakeAmplitude( const Vector &center, const Vector &shakePt, float amplitude, float radius ) 
{
	if ( radius <= 0 )
		return amplitude;

	float localAmplitude = -1;
	Vector delta = center - shakePt;
	float distance = delta.Length();

	if ( distance <= radius )
	{
		// Make the amplitude fall off over distance
		float flPerc = 1.0 - (distance / radius);
		localAmplitude = amplitude * flPerc;
	}

	return localAmplitude;
}


//-----------------------------------------------------------------------------
// Transmits the actual shake event
//-----------------------------------------------------------------------------
inline void TransmitShakeEvent( CBasePlayer *pPlayer, float localAmplitude, float frequency, float duration, ShakeCommand_t eCommand )
{
	if (( localAmplitude > 0 ) || ( eCommand == SHAKE_STOP ))
	{
		if ( eCommand == SHAKE_STOP )
			localAmplitude = 0;

		CSingleUserRecipientFilter user( pPlayer );
		user.MakeReliable();
		UserMessageBegin( user, "Shake" );
			WRITE_BYTE( eCommand );				// shake command (SHAKE_START, STOP, FREQUENCY, AMPLITUDE)
			WRITE_FLOAT( localAmplitude );			// shake magnitude/amplitude
			WRITE_FLOAT( frequency );				// shake noise frequency
			WRITE_FLOAT( duration );				// shake lasts this long
		MessageEnd();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Shake the screen of all clients within radius.
//			radius == 0, shake all clients
// UNDONE: Fix falloff model (disabled)?
// UNDONE: Affect user controls?
// Input  : center - Center of screen shake, radius is measured from here.
//			amplitude - Amplitude of shake
//			frequency - 
//			duration - duration of shake in seconds.
//			radius - Radius of effect, 0 shakes all clients.
//			command - One of the following values:
//				SHAKE_START - starts the screen shake for all players within the radius
//				SHAKE_STOP - stops the screen shake for all players within the radius
//				SHAKE_AMPLITUDE - modifies the amplitude of the screen shake
//									for all players within the radius
//				SHAKE_FREQUENCY - modifies the frequency of the screen shake
//									for all players within the radius
//			bAirShake - if this is false, then it will only shake players standing on the ground.
//-----------------------------------------------------------------------------
const float MAX_SHAKE_AMPLITUDE = 16.0f;
void UTIL_ScreenShake( const Vector &center, float amplitude, float frequency, float duration, float radius, ShakeCommand_t eCommand, bool bAirShake )
{
	int			i;
	float		localAmplitude;

	if ( amplitude > MAX_SHAKE_AMPLITUDE )
	{
		amplitude = MAX_SHAKE_AMPLITUDE;
	}
	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );

		//
		// Only start shakes for players that are on the ground unless doing an air shake.
		//
		if ( !pPlayer || (!bAirShake && (eCommand == SHAKE_START) && !(pPlayer->GetFlags() & FL_ONGROUND)) )
		{
			continue;
		}

		localAmplitude = ComputeShakeAmplitude( center, pPlayer->WorldSpaceCenter(), amplitude, radius );

		// This happens if the player is outside the radius, in which case we should ignore 
		// all commands
		if (localAmplitude < 0)
			continue;

		TransmitShakeEvent( (CBasePlayer *)pPlayer, localAmplitude, frequency, duration, eCommand );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Shake an object and all players on or near it
//-----------------------------------------------------------------------------
void UTIL_ScreenShakeObject( CBaseEntity *pEnt, const Vector &center, float amplitude, float frequency, float duration, float radius, ShakeCommand_t eCommand, bool bAirShake )
{
	int			i;
	float		localAmplitude;

	CBaseEntity *pHighestParent = pEnt->GetRootMoveParent();
	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
		if (!pPlayer)
			continue;

		// Shake the object, or anything hierarchically attached to it at maximum amplitude
		localAmplitude = 0;
		if (pHighestParent == pPlayer->GetRootMoveParent())
		{
			localAmplitude = amplitude;
		}
		else if ((pPlayer->GetFlags() & FL_ONGROUND) && (pPlayer->GetGroundEntity()->GetRootMoveParent() == pHighestParent))
		{
			// If the player is standing on the object, use maximum amplitude
			localAmplitude = amplitude;
		}
		else
		{
			// Only shake players that are on the ground.
			if ( !bAirShake && !(pPlayer->GetFlags() & FL_ONGROUND) )
			{
				continue;
			}

			localAmplitude = ComputeShakeAmplitude( center, pPlayer->WorldSpaceCenter(), amplitude, radius );

			// This happens if the player is outside the radius, 
			// in which case we should ignore all commands
			if (localAmplitude < 0)
				continue;
		}

		TransmitShakeEvent( (CBasePlayer *)pPlayer, localAmplitude, frequency, duration, eCommand );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Punches the view of all clients within radius.
//			If radius is 0, punches all clients.
// Input  : center - Center of punch, radius is measured from here.
//			radius - Radius of effect, 0 punches all clients.
//			bInAir - if this is false, then it will only punch players standing on the ground.
//-----------------------------------------------------------------------------
void UTIL_ViewPunch( const Vector &center, QAngle angPunch, float radius, bool bInAir )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );

		//
		// Only apply the punch to players that are on the ground unless doing an air punch.
		//
		if ( !pPlayer || (!bInAir && !(pPlayer->GetFlags() & FL_ONGROUND)) )
		{
			continue;
		}

		QAngle angTemp = angPunch;

		if ( radius > 0 )
		{
			Vector delta = center - pPlayer->GetAbsOrigin();
			float distance = delta.Length();

			if ( distance <= radius )
			{
				// Make the punch amplitude fall off over distance.
				float flPerc = 1.0 - (distance / radius);
				angTemp *= flPerc;
			}
			else
			{
				continue;
			}
		}
		
		pPlayer->ViewPunch( angTemp );
	}
}


void UTIL_ScreenFadeBuild( ScreenFade_t &fade, const color32 &color, float fadeTime, float fadeHold, int flags )
{
	fade.duration = FixedUnsigned16( fadeTime, 1<<SCREENFADE_FRACBITS );		// 7.9 fixed
	fade.holdTime = FixedUnsigned16( fadeHold, 1<<SCREENFADE_FRACBITS );		// 7.9 fixed
	fade.r = color.r;
	fade.g = color.g;
	fade.b = color.b;
	fade.a = color.a;
	fade.fadeFlags = flags;
}


void UTIL_ScreenFadeWrite( const ScreenFade_t &fade, CBaseEntity *pEntity )
{
	if ( !pEntity || !pEntity->IsNetClient() )
		return;

	CSingleUserRecipientFilter user( (CBasePlayer *)pEntity );
	user.MakeReliable();

	UserMessageBegin( user, "Fade" );		// use the magic #1 for "one client"
		WRITE_SHORT( fade.duration );		// fade lasts this long
		WRITE_SHORT( fade.holdTime );		// fade lasts this long
		WRITE_SHORT( fade.fadeFlags );		// fade type (in / out)
		WRITE_BYTE( fade.r );				// fade red
		WRITE_BYTE( fade.g );				// fade green
		WRITE_BYTE( fade.b );				// fade blue
		WRITE_BYTE( fade.a );				// fade blue
	MessageEnd();
}


void UTIL_ScreenFadeAll( const color32 &color, float fadeTime, float fadeHold, int flags )
{
	int			i;
	ScreenFade_t	fade;


	UTIL_ScreenFadeBuild( fade, color, fadeTime, fadeHold, flags );

	for ( i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBaseEntity *pPlayer = UTIL_PlayerByIndex( i );
	
		UTIL_ScreenFadeWrite( fade, pPlayer );
	}
}


void UTIL_ScreenFade( CBaseEntity *pEntity, const color32 &color, float fadeTime, float fadeHold, int flags )
{
	ScreenFade_t	fade;

	UTIL_ScreenFadeBuild( fade, color, fadeTime, fadeHold, flags );
	UTIL_ScreenFadeWrite( fade, pEntity );
}


void UTIL_HudMessage( CBasePlayer *pToPlayer, const hudtextparms_t &textparms, const char *pMessage )
{
	CRecipientFilter filter;
	
	if( pToPlayer )
	{
		filter.AddRecipient( pToPlayer );
	}
	else
	{
		filter.AddAllPlayers();
	}

	filter.MakeReliable();

	UserMessageBegin( filter, "HudMsg" );
		WRITE_BYTE ( textparms.channel & 0xFF );
		WRITE_FLOAT( textparms.x );
		WRITE_FLOAT( textparms.y );
		WRITE_BYTE ( textparms.r1 );
		WRITE_BYTE ( textparms.g1 );
		WRITE_BYTE ( textparms.b1 );
		WRITE_BYTE ( textparms.a1 );
		WRITE_BYTE ( textparms.r2 );
		WRITE_BYTE ( textparms.g2 );
		WRITE_BYTE ( textparms.b2 );
		WRITE_BYTE ( textparms.a2 );
		WRITE_BYTE ( textparms.effect );
		WRITE_FLOAT( textparms.fadeinTime );
		WRITE_FLOAT( textparms.fadeoutTime );
		WRITE_FLOAT( textparms.holdTime );
		WRITE_FLOAT( textparms.fxTime );
		WRITE_STRING( pMessage );
	MessageEnd();
}

void UTIL_HudMessageAll( const hudtextparms_t &textparms, const char *pMessage )
{
	UTIL_HudMessage( NULL, textparms, pMessage );
}

void UTIL_HudHintText( CBaseEntity *pEntity, const char *pMessage )
{
	if ( !pEntity )
		return;

	CSingleUserRecipientFilter user( (CBasePlayer *)pEntity );
	user.MakeReliable();
	UserMessageBegin( user, "KeyHintText" );
		WRITE_BYTE( 1 );	// one string
		WRITE_STRING( pMessage );
	MessageEnd();
}

void UTIL_ClientPrintFilter( IRecipientFilter& filter, int msg_dest, const char *msg_name, const char *param1, const char *param2, const char *param3, const char *param4 )
{
	UserMessageBegin( filter, "TextMsg" );
		WRITE_BYTE( msg_dest );
		WRITE_STRING( msg_name );

		if ( param1 )
			WRITE_STRING( param1 );
		else
			WRITE_STRING( "" );

		if ( param2 )
			WRITE_STRING( param2 );
		else
			WRITE_STRING( "" );

		if ( param3 )
			WRITE_STRING( param3 );
		else
			WRITE_STRING( "" );

		if ( param4 )
			WRITE_STRING( param4 );
		else
			WRITE_STRING( "" );

	MessageEnd();
}
					 
void UTIL_ClientPrintAll( int msg_dest, const char *msg_name, const char *param1, const char *param2, const char *param3, const char *param4 )
{
	CReliableBroadcastRecipientFilter filter;

	UTIL_ClientPrintFilter( filter, msg_dest, msg_name, param1, param2, param3, param4 );
}

void ClientPrint( CBasePlayer *player, int msg_dest, const char *msg_name, const char *param1, const char *param2, const char *param3, const char *param4 )
{
	if ( !player )
		return;

	CSingleUserRecipientFilter user( player );
	user.MakeReliable();

	UTIL_ClientPrintFilter( user, msg_dest, msg_name, param1, param2, param3, param4 );
}

void UTIL_SayTextFilter( IRecipientFilter& filter, const char *pText, CBasePlayer *pPlayer, bool bChat )
{
	UserMessageBegin( filter, "SayText" );
		if ( pPlayer ) 
		{
			WRITE_BYTE( pPlayer->entindex() );
		}
		else
		{
			WRITE_BYTE( 0 ); // world, dedicated server says
		}
		WRITE_STRING( pText );
		WRITE_BYTE( bChat );
	MessageEnd();
}

void UTIL_SayText2Filter( IRecipientFilter& filter, CBasePlayer *pEntity, bool bChat, const char *msg_name, const char *param1, const char *param2, const char *param3, const char *param4 )
{
	UserMessageBegin( filter, "SayText2" );
		if ( pEntity )
		{
			WRITE_BYTE( pEntity->entindex() );
		}
		else
		{
			WRITE_BYTE( 0 ); // world, dedicated server says
		}

		WRITE_BYTE( bChat );

		WRITE_STRING( msg_name );

		if ( param1 )
			WRITE_STRING( param1 );
		else
			WRITE_STRING( "" );

		if ( param2 )
			WRITE_STRING( param2 );
		else
			WRITE_STRING( "" );

		if ( param3 )
			WRITE_STRING( param3 );
		else
			WRITE_STRING( "" );

		if ( param4 )
			WRITE_STRING( param4 );
		else
			WRITE_STRING( "" );

	MessageEnd();
}

void UTIL_SayText( const char *pText, CBasePlayer *pToPlayer )
{
	if ( !pToPlayer->IsNetClient() )
		return;

	CSingleUserRecipientFilter user( pToPlayer );
	user.MakeReliable();

	UTIL_SayTextFilter( user, pText, pToPlayer, false );
}

void UTIL_SayTextAll( const char *pText, CBasePlayer *pPlayer, bool bChat )
{
	CReliableBroadcastRecipientFilter filter;
	UTIL_SayTextFilter( filter, pText, pPlayer, bChat );
}

void UTIL_ShowMessage( const char *pString, CBasePlayer *pPlayer )
{
	CRecipientFilter filter;

	if ( pPlayer )
	{
		filter.AddRecipient( pPlayer );
	}
	else
	{
		filter.AddAllPlayers();
	}
	
	filter.MakeReliable();

	UserMessageBegin( filter, "HudText" );
		WRITE_STRING( pString );
	MessageEnd();
}


void UTIL_ShowMessageAll( const char *pString )
{
	UTIL_ShowMessage( pString, NULL );
}

// So we always return a valid surface
static csurface_t	g_NullSurface = { "**empty**", 0 };

void UTIL_SetTrace(trace_t& trace, const Ray_t &ray, edict_t *ent, float fraction, 
				   int hitgroup, unsigned int contents, const Vector& normal, float intercept )
{
	trace.startsolid = (fraction == 0.0f);
	trace.fraction = fraction;
	VectorCopy( ray.m_Start, trace.startpos );
	VectorMA( ray.m_Start, fraction, ray.m_Delta, trace.endpos );
	VectorCopy( normal, trace.plane.normal );
	trace.plane.dist = intercept;
	trace.m_pEnt = CBaseEntity::Instance( ent );
	trace.hitgroup = hitgroup;
	trace.surface =	g_NullSurface;
	trace.contents = contents;
}

void UTIL_ClearTrace( trace_t &trace )
{
	memset( &trace, 0, sizeof(trace));
	trace.fraction = 1.f;
	trace.fractionleftsolid = 0;
	trace.surface = g_NullSurface;
}

	

//-----------------------------------------------------------------------------
// Sets the entity size
//-----------------------------------------------------------------------------
static void SetMinMaxSize (CBaseEntity *pEnt, const Vector& mins, const Vector& maxs )
{
	for ( int i=0 ; i<3 ; i++ )
	{
		if ( mins[i] > maxs[i] )
		{
			Error( "%s: backwards mins/maxs", ( pEnt ) ? pEnt->GetDebugName() : "<NULL>" );
		}
	}

	Assert( pEnt );

	pEnt->SetCollisionBounds( mins, maxs );
}


//-----------------------------------------------------------------------------
// Sets the model size
//-----------------------------------------------------------------------------
void UTIL_SetSize( CBaseEntity *pEnt, const Vector &vecMin, const Vector &vecMax )
{
	SetMinMaxSize (pEnt, vecMin, vecMax);
}

	
//-----------------------------------------------------------------------------
// Sets the model to be associated with an entity
//-----------------------------------------------------------------------------
void UTIL_SetModel( CBaseEntity *pEntity, const char *pModelName )
{
	// check to see if model was properly precached
	int i = modelinfo->GetModelIndex( pModelName );
	if ( i == -1 )	
	{
		Error("%i/%s - %s:  UTIL_SetModel:  not precached: %s\n", pEntity->entindex(),
			STRING( pEntity->GetEntityName() ),
			pEntity->GetClassname(), pModelName);
	}

	CBaseAnimating *pAnimating = pEntity->GetBaseAnimating();
	if ( pAnimating )
	{
		pAnimating->m_nForceBone = 0;
	}

	pEntity->SetModelName( AllocPooledString( pModelName ) );
	pEntity->SetModelIndex( i ) ;
	SetMinMaxSize(pEntity, vec3_origin, vec3_origin);
	pEntity->SetCollisionBoundsFromModel();
}

	
void UTIL_SetOrigin( CBaseEntity *entity, const Vector &vecOrigin, bool bFireTriggers )
{
	entity->SetLocalOrigin( vecOrigin );
	if ( bFireTriggers )
	{
		entity->PhysicsTouchTriggers();
	}
}


void UTIL_ParticleEffect( const Vector &vecOrigin, const Vector &vecDirection, ULONG ulColor, ULONG ulCount )
{
	Msg( "UTIL_ParticleEffect:  Disabled\n" );
}

void UTIL_Smoke( const Vector &origin, const float scale, const float framerate )
{
	g_pEffects->Smoke( origin, g_sModelIndexSmoke, scale, framerate );
}

// snaps a vector to the nearest axis vector (if within epsilon)
void UTIL_SnapDirectionToAxis( Vector &direction, float epsilon )
{
	float proj = 1 - epsilon;
	for ( int i = 0; i < 3; i ++ )
	{
		if ( fabs(direction[i]) > proj )
		{
			// snap to axis unit vector
			if ( direction[i] < 0 )
				direction[i] = -1.0f;
			else
				direction[i] = 1.0f;
			direction[(i+1)%3] = 0;
			direction[(i+2)%3] = 0;
			return;
		}
	}
}

char *UTIL_VarArgs( const char *format, ... )
{
	va_list		argptr;
	static char		string[1024];
	
	va_start (argptr, format);
	Q_vsnprintf(string, sizeof(string), format,argptr);
	va_end (argptr);

	return string;	
}

bool UTIL_IsMasterTriggered(string_t sMaster, CBaseEntity *pActivator)
{
	if (sMaster != NULL_STRING)
	{
		CBaseEntity *pMaster = gEntList.FindEntityByName( NULL, sMaster, NULL, pActivator );
	
		if ( pMaster && (pMaster->ObjectCaps() & FCAP_MASTER) )
		{
			return pMaster->IsTriggered( pActivator );
		}

		Warning( "Master was null or not a master!\n");
	}

	// if this isn't a master entity, just say yes.
	return true;
}

void UTIL_BloodStream( const Vector &origin, const Vector &direction, int color, int amount )
{
	if ( !UTIL_ShouldShowBlood( color ) )
		return;

	if ( g_Language.GetInt() == LANGUAGE_GERMAN && color == BLOOD_COLOR_RED )
		color = 0;

	CPVSFilter filter( origin );
	te->BloodStream( filter, 0.0, &origin, &direction, 247, 63, 14, 255, MIN( amount, 255 ) );
}				


Vector UTIL_RandomBloodVector( void )
{
	Vector direction;

	direction.x = random->RandomFloat ( -1, 1 );
	direction.y = random->RandomFloat ( -1, 1 );
	direction.z = random->RandomFloat ( 0, 1 );

	return direction;
}


//------------------------------------------------------------------------------
// Purpose : Creates both an decal and any associated impact effects (such
//			 as flecks) for the given iDamageType and the trace's end position
// Input   :
// Output  :
//------------------------------------------------------------------------------
void UTIL_ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName )
{
	CBaseEntity *pEntity = pTrace->m_pEnt;

	// Is the entity valid, is the surface sky?
	if ( !pEntity || !UTIL_IsValidEntity( pEntity ) || (pTrace->surface.flags & SURF_SKY) )
		return;

	if ( pTrace->fraction == 1.0 )
		return;

	pEntity->ImpactTrace( pTrace, iDamageType, pCustomImpactName );
}

/*
==============
UTIL_PlayerDecalTrace

A player is trying to apply his custom decal for the spray can.
Tell connected clients to display it, or use the default spray can decal
if the custom can't be loaded.
==============
*/
void UTIL_PlayerDecalTrace( trace_t *pTrace, int playernum )
{
	if (pTrace->fraction == 1.0)
		return;

	CBroadcastRecipientFilter filter;

	te->PlayerDecal( filter, 0.0,
		&pTrace->endpos, playernum, pTrace->m_pEnt->entindex() );
}

bool UTIL_TeamsMatch( const char *pTeamName1, const char *pTeamName2 )
{
	// Everyone matches unless it's teamplay
	if ( !g_pGameRules->IsTeamplay() )
		return true;

	// Both on a team?
	if ( *pTeamName1 != 0 && *pTeamName2 != 0 )
	{
		if ( !stricmp( pTeamName1, pTeamName2 ) )	// Same Team?
			return true;
	}

	return false;
}


void UTIL_AxisStringToPointPoint( Vector &start, Vector &end, const char *pString )
{
	char tmpstr[256];

	Q_strncpy( tmpstr, pString, sizeof(tmpstr) );
	char *pVec = strtok( tmpstr, "," );
	int i = 0;
	while ( pVec != NULL && *pVec )
	{
		if ( i == 0 )
		{
			UTIL_StringToVector( start.Base(), pVec );
			i++;
		}
		else
		{
			UTIL_StringToVector( end.Base(), pVec );
		}
		pVec = strtok( NULL, "," );
	}
}

void UTIL_AxisStringToPointDir( Vector &start, Vector &dir, const char *pString )
{
	Vector end;
	UTIL_AxisStringToPointPoint( start, end, pString );
	dir = end - start;
	VectorNormalize(dir);
}

void UTIL_AxisStringToUnitDir( Vector &dir, const char *pString )
{
	Vector start;
	UTIL_AxisStringToPointDir( start, dir, pString );
}

/*
==================================================
UTIL_ClipPunchAngleOffset
==================================================
*/

void UTIL_ClipPunchAngleOffset( QAngle &in, const QAngle &punch, const QAngle &clip )
{
	QAngle	final = in + punch;

	//Clip each component
	for ( int i = 0; i < 3; i++ )
	{
		if ( final[i] > clip[i] )
		{
			final[i] = clip[i];
		}
		else if ( final[i] < -clip[i] )
		{
			final[i] = -clip[i];
		}

		//Return the result
		in[i] = final[i] - punch[i];
	}
}

float UTIL_WaterLevel( const Vector &position, float minz, float maxz )
{
	Vector midUp = position;
	midUp.z = minz;

	if ( !(UTIL_PointContents(midUp) & MASK_WATER) )
		return minz;

	midUp.z = maxz;
	if ( UTIL_PointContents(midUp) & MASK_WATER )
		return maxz;

	float diff = maxz - minz;
	while (diff > 1.0)
	{
		midUp.z = minz + diff/2.0;
		if ( UTIL_PointContents(midUp) & MASK_WATER )
		{
			minz = midUp.z;
		}
		else
		{
			maxz = midUp.z;
		}
		diff = maxz - minz;
	}

	return midUp.z;
}


//-----------------------------------------------------------------------------
// Like UTIL_WaterLevel, but *way* less expensive.
// I didn't replace UTIL_WaterLevel everywhere to avoid breaking anything.
//-----------------------------------------------------------------------------
class CWaterTraceFilter : public CTraceFilter
{
public:
	bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		CBaseEntity *pCollide = EntityFromEntityHandle( pHandleEntity );

		// Static prop case...
		if ( !pCollide )
			return false;

		// Only impact water stuff...
		if ( pCollide->GetSolidFlags() & FSOLID_VOLUME_CONTENTS )
			return true;

		return false;
	}
};

float UTIL_FindWaterSurface( const Vector &position, float minz, float maxz )
{
	Vector vecStart, vecEnd;
	vecStart.Init( position.x, position.y, maxz );
	vecEnd.Init( position.x, position.y, minz );

	Ray_t ray;
	trace_t tr;
	CWaterTraceFilter waterTraceFilter;
	ray.Init( vecStart, vecEnd );
	enginetrace->TraceRay( ray, MASK_WATER, &waterTraceFilter, &tr );

	return tr.endpos.z;
}


extern short	g_sModelIndexBubbles;// holds the index for the bubbles model

void UTIL_Bubbles( const Vector& mins, const Vector& maxs, int count )
{
	Vector mid =  (mins + maxs) * 0.5;

	float flHeight = UTIL_WaterLevel( mid,  mid.z, mid.z + 1024 );
	flHeight = flHeight - mins.z;

	CPASFilter filter( mid );

	te->Bubbles( filter, 0.0,
		&mins, &maxs, flHeight, g_sModelIndexBubbles, count, 8.0 );
}

void UTIL_BubbleTrail( const Vector& from, const Vector& to, int count )
{
	// Find water surface will return from.z if the from point is above water
	float flStartHeight = UTIL_FindWaterSurface( from, from.z, from.z + 256 );
	flStartHeight = flStartHeight - from.z;

	float flEndHeight = UTIL_FindWaterSurface( to, to.z, to.z + 256 );
	flEndHeight = flEndHeight - to.z;

	if ( ( flStartHeight == 0 ) && ( flEndHeight == 0 ) )
		return;

	float flWaterZ = flStartHeight + from.z;

	const Vector *pFrom = &from;
	const Vector *pTo = &to;
	Vector vecWaterPoint;
	if ( ( flStartHeight == 0 ) || ( flEndHeight == 0 ) )
	{
		if ( flStartHeight == 0 )
		{
			flWaterZ = flEndHeight + to.z;
		}

		float t = IntersectRayWithAAPlane( from, to, 2, 1.0f, flWaterZ );
		Assert( (t >= -1e-3f) && ( t <= 1.0f ) );
		VectorLerp( from, to, t, vecWaterPoint );
		if ( flStartHeight == 0 )
		{
			pFrom = &vecWaterPoint;

			// Reduce the count by the actual length
			count = (int)( count * ( 1.0f - t ) );
		}
		else
		{
			pTo = &vecWaterPoint;

			// Reduce the count by the actual length
			count = (int)( count * t );
		}
	}

	CBroadcastRecipientFilter filter;
	te->BubbleTrail( filter, 0.0, pFrom, pTo, flWaterZ, g_sModelIndexBubbles, count, 8.0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : Start - 
//			End - 
//			ModelIndex - 
//			FrameStart - 
//			FrameRate - 
//			Life - 
//			Width - 
//			Noise - 
//			Red - 
//			Green - 
//			Brightness - 
//			Speed - 
//-----------------------------------------------------------------------------
void UTIL_Beam( Vector &Start, Vector &End, int nModelIndex, int nHaloIndex, unsigned char FrameStart, unsigned char FrameRate,
				float Life, unsigned char Width, unsigned char EndWidth, unsigned char FadeLength, unsigned char Noise, unsigned char Red, unsigned char Green,
				unsigned char Blue, unsigned char Brightness, unsigned char Speed)
{
	CBroadcastRecipientFilter filter;

	te->BeamPoints( filter, 0.0,
		&Start, 
		&End, 
		nModelIndex, 
		nHaloIndex, 
		FrameStart,
		FrameRate, 
		Life,
		Width,
		EndWidth,
		FadeLength,
		Noise,
		Red,
		Green,
		Blue,
		Brightness,
		Speed );
}

bool UTIL_IsValidEntity( CBaseEntity *pEnt )
{
	edict_t *pEdict = pEnt->edict();
	if ( !pEdict || pEdict->IsFree() )
		return false;
	return true;
}


#define PRECACHE_OTHER_ONCE
// UNDONE: Do we need this to avoid doing too much of this?  Measure startup times and see
#if defined( PRECACHE_OTHER_ONCE )

#include "utlsymbol.h"
class CPrecacheOtherList : public CAutoGameSystem
{
public:
	CPrecacheOtherList( char const *name ) : CAutoGameSystem( name )
	{
	}
	virtual void LevelInitPreEntity();
	virtual void LevelShutdownPostEntity();

	bool AddOrMarkPrecached( const char *pClassname );

private:
	CUtlSymbolTable		m_list;
};

void CPrecacheOtherList::LevelInitPreEntity()
{
	m_list.RemoveAll();
}

void CPrecacheOtherList::LevelShutdownPostEntity()
{
	m_list.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: mark or add
// Input  : *pEntity - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CPrecacheOtherList::AddOrMarkPrecached( const char *pClassname )
{
	CUtlSymbol sym = m_list.Find( pClassname );
	if ( sym.IsValid() )
		return false;

	m_list.AddString( pClassname );
	return true;
}

CPrecacheOtherList g_PrecacheOtherList( "CPrecacheOtherList" );
#endif

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *szClassname - 
//			*modelName - 
//-----------------------------------------------------------------------------
void UTIL_PrecacheOther( const char *szClassname, const char *modelName )
{
#if defined( PRECACHE_OTHER_ONCE )
	// already done this one?, if not, mark as done
	if ( !g_PrecacheOtherList.AddOrMarkPrecached( szClassname ) )
		return;
#endif

	CBaseEntity	*pEntity = CreateEntityByName( szClassname );
	if ( !pEntity )
	{
		Warning( "NULL Ent in UTIL_PrecacheOther\n" );
		return;
	}

	// If we have a specified model, set it before calling precache
	if ( modelName && modelName[0] )
	{
		pEntity->SetModelName( AllocPooledString( modelName ) );
	}
	
	if (pEntity)
		pEntity->Precache( );

	UTIL_RemoveImmediate( pEntity );
}

//=========================================================
// UTIL_LogPrintf - Prints a logged message to console.
// Preceded by LOG: ( timestamp ) < message >
//=========================================================
void UTIL_LogPrintf( const char *fmt, ... )
{
	va_list		argptr;
	char		tempString[1024];
	
	va_start ( argptr, fmt );
	Q_vsnprintf( tempString, sizeof(tempString), fmt, argptr );
	va_end   ( argptr );

	// Print to server console
	engine->LogPrint( tempString );
}

//=========================================================
// UTIL_DotPoints - returns the dot product of a line from
// src to check and vecdir.
//=========================================================
float UTIL_DotPoints ( const Vector &vecSrc, const Vector &vecCheck, const Vector &vecDir )
{
	Vector2D	vec2LOS;

	vec2LOS = ( vecCheck - vecSrc ).AsVector2D();
	Vector2DNormalize( vec2LOS );

	return DotProduct2D(vec2LOS, vecDir.AsVector2D());
}


//=========================================================
// UTIL_StripToken - for redundant keynames
//=========================================================
void UTIL_StripToken( const char *pKey, char *pDest )
{
	int i = 0;

	while ( pKey[i] && pKey[i] != '#' )
	{
		pDest[i] = pKey[i];
		i++;
	}
	pDest[i] = 0;
}


// computes gravity scale for an absolute gravity.  Pass the result into CBaseEntity::SetGravity()
float UTIL_ScaleForGravity( float desiredGravity )
{
	float worldGravity = GetCurrentGravity();
	return worldGravity > 0 ? desiredGravity / worldGravity : 0;
}


//-----------------------------------------------------------------------------
// Purpose: Implemented for mathlib.c error handling
// Input  : *error - 
//-----------------------------------------------------------------------------
extern "C" void Sys_Error( char *error, ... )
{
	va_list	argptr;
	char	string[1024];
	
	va_start( argptr, error );
	Q_vsnprintf( string, sizeof(string), error, argptr );
	va_end( argptr );

	Warning( "%s", string );
	Assert(0);
}


//-----------------------------------------------------------------------------
// Purpose: Spawns an entity into the game, initializing it with the map ent data block
// Input  : *pEntity - the newly created entity
//			*mapData - pointer a block of entity map data
// Output : -1 if the entity was not successfully created; 0 on success
//-----------------------------------------------------------------------------
int DispatchSpawn( CBaseEntity *pEntity )
{
	if ( pEntity )
	{
		MDLCACHE_CRITICAL_SECTION();

		// keep a smart pointer that will now if the object gets deleted
		EHANDLE pEntSafe;
		pEntSafe = pEntity;

		// Initialize these or entities who don't link to the world won't have anything in here
		// is this necessary?
		//pEntity->SetAbsMins( pEntity->GetOrigin() - Vector(1,1,1) );
		//pEntity->SetAbsMaxs( pEntity->GetOrigin() + Vector(1,1,1) );

#if defined(TRACK_ENTITY_MEMORY) && defined(USE_MEM_DEBUG)
		const char *pszClassname = NULL;
		int iClassname = ((CEntityFactoryDictionary*)EntityFactoryDictionary())->m_Factories.Find( pEntity->GetClassname() );
		if ( iClassname != ((CEntityFactoryDictionary*)EntityFactoryDictionary())->m_Factories.InvalidIndex() )
			pszClassname = ((CEntityFactoryDictionary*)EntityFactoryDictionary())->m_Factories.GetElementName( iClassname );
		if ( pszClassname )
		{
			MemAlloc_PushAllocDbgInfo( pszClassname, __LINE__ );
		}
#endif
		bool bAsyncAnims = mdlcache->SetAsyncLoad( MDLCACHE_ANIMBLOCK, false );
		CBaseAnimating *pAnimating = pEntity->GetBaseAnimating();
		if (!pAnimating)
		{
			pEntity->Spawn();
		}
		else
		{
			// Don't allow the PVS check to skip animation setup during spawning
			pAnimating->SetBoneCacheFlags( BCF_IS_IN_SPAWN );
			pEntity->Spawn();
			pAnimating->ClearBoneCacheFlags( BCF_IS_IN_SPAWN );
		}
		mdlcache->SetAsyncLoad( MDLCACHE_ANIMBLOCK, bAsyncAnims );

#if defined(TRACK_ENTITY_MEMORY) && defined(USE_MEM_DEBUG)
		if ( pszClassname )
		{
			MemAlloc_PopAllocDbgInfo();
		}
#endif
		// Try to get the pointer again, in case the spawn function deleted the entity.
		// UNDONE: Spawn() should really return a code to ask that the entity be deleted, but
		// that would touch too much code for me to do that right now.

		if ( pEntSafe == NULL || pEntity->IsMarkedForDeletion() )
			return -1;

		if ( pEntity->m_iGlobalname != NULL_STRING ) 
		{
			// Handle global stuff here
			int globalIndex = GlobalEntity_GetIndex( pEntity->m_iGlobalname );
			if ( globalIndex >= 0 )
			{
				// Already dead? delete
				if ( GlobalEntity_GetState(globalIndex) == GLOBAL_DEAD )
				{
					pEntity->Remove();
					return -1;
				}
				else if ( !FStrEq(STRING(gpGlobals->mapname), GlobalEntity_GetMap(globalIndex)) )
				{
					pEntity->MakeDormant();	// Hasn't been moved to this level yet, wait but stay alive
				}
				// In this level & not dead, continue on as normal
			}
			else
			{
				// Spawned entities default to 'On'
				GlobalEntity_Add( pEntity->m_iGlobalname, gpGlobals->mapname, GLOBAL_ON );
//					Msg( "Added global entity %s (%s)\n", pEntity->GetClassname(), STRING(pEntity->m_iGlobalname) );
			}
		}

		gEntList.NotifySpawn( pEntity );
	}

	return 0;
}

// UNDONE: This could be a better test - can we run the absbox through the bsp and see
// if it contains any solid space?  or would that eliminate some entities we want to keep?
int UTIL_EntityInSolid( CBaseEntity *ent )
{
	Vector	point;

	CBaseEntity *pParent = ent->GetMoveParent();
	// HACKHACK -- If you're attached to a client, always go through
	if ( pParent )
	{
		if ( pParent->IsPlayer() )
			return 0;

		ent = ent->GetRootMoveParent();
	}

	point = ent->WorldSpaceCenter();
	return ( enginetrace->GetPointContents( point ) & MASK_SOLID );
}


//-----------------------------------------------------------------------------
// Purpose: Initialize the matrix from an entity
// Input  : *pEntity - 
//-----------------------------------------------------------------------------
void EntityMatrix::InitFromEntity( CBaseEntity *pEntity, int iAttachment )
{
	if ( !pEntity )
	{
		Identity();
		return;
	}

	// Get an attachment's matrix?
	if ( iAttachment != 0 )
	{
		CBaseAnimating *pAnimating = pEntity->GetBaseAnimating();
		if ( pAnimating && pAnimating->GetModelPtr() )
		{
			Vector vOrigin;
			QAngle vAngles;
			if ( pAnimating->GetAttachment( iAttachment, vOrigin, vAngles ) )
			{
				((VMatrix *)this)->SetupMatrixOrgAngles( vOrigin, vAngles );
				return;
			}
		}
	}

	((VMatrix *)this)->SetupMatrixOrgAngles( pEntity->GetAbsOrigin(), pEntity->GetAbsAngles() );
}


void EntityMatrix::InitFromEntityLocal( CBaseEntity *entity )
{
	if ( !entity || !entity->edict() )
	{
		Identity();
		return;
	}
	((VMatrix *)this)->SetupMatrixOrgAngles( entity->GetLocalOrigin(), entity->GetLocalAngles() );
}

//==================================================
// Purpose: 
// Input: 
// Output: 
//==================================================

void UTIL_ValidateSoundName( string_t &name, const char *defaultStr )
{
	if ( ( !name || 
		   strlen( (char*) STRING( name ) ) < 1 ) || 
		   !Q_stricmp( (char *)STRING(name), "0" ) )
	{
		name = AllocPooledString( defaultStr );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Slightly modified strtok. Does not modify the input string. Does
//			not skip over more than one separator at a time. This allows parsing
//			strings where tokens between separators may or may not be present:
//
//			Door01,,,0 would be parsed as "Door01"  ""  ""  "0"
//			Door01,Open,,0 would be parsed as "Door01"  "Open"  ""  "0"
//
// Input  : token - Returns with a token, or zero length if the token was missing.
//			str - String to parse.
//			sep - Character to use as separator. UNDONE: allow multiple separator chars
// Output : Returns a pointer to the next token to be parsed.
//-----------------------------------------------------------------------------
const char *nexttoken(char *token, const char *str, char sep)
{
	if ((str == NULL) || (*str == '\0'))
	{
		*token = '\0';
		return(NULL);
	}

	//
	// Copy everything up to the first separator into the return buffer.
	// Do not include separators in the return buffer.
	//
	while ((*str != sep) && (*str != '\0'))
	{
		*token++ = *str++;
	}
	*token = '\0';

	//
	// Advance the pointer unless we hit the end of the input string.
	//
	if (*str == '\0')
	{
		return(str);
	}

	return(++str);
}

//-----------------------------------------------------------------------------
// Purpose: Helper for UTIL_FindClientInPVS
// Input  : check - last checked client
// Output : static int UTIL_GetNewCheckClient
//-----------------------------------------------------------------------------
// FIXME:  include bspfile.h here?
class CCheckClient : public CAutoGameSystem
{
public:
	CCheckClient( char const *name ) : CAutoGameSystem( name )
	{
	}

	void LevelInitPreEntity()
	{
		m_checkCluster = -1;
		m_lastcheck = 1;
		m_lastchecktime = -1;
		m_bClientPVSIsExpanded = false;
	}

	byte	m_checkPVS[MAX_MAP_LEAFS/8];
	byte	m_checkVisibilityPVS[MAX_MAP_LEAFS/8];
	int		m_checkCluster;
	int		m_lastcheck;
	float	m_lastchecktime;
	bool	m_bClientPVSIsExpanded;
};

CCheckClient g_CheckClient( "CCheckClient" );


static int UTIL_GetNewCheckClient( int check )
{
	int		i;
	edict_t	*ent;
	Vector	org;

// cycle to the next one

	if (check < 1)
		check = 1;
	if (check > gpGlobals->maxClients)
		check = gpGlobals->maxClients;

	if (check == gpGlobals->maxClients)
		i = 1;
	else
		i = check + 1;

	for ( ;  ; i++)
	{
		if ( i > gpGlobals->maxClients )
		{
			i = 1;
		}

		ent = engine->PEntityOfEntIndex( i );
		if ( !ent )
			continue;

		// Looped but didn't find anything else
		if ( i == check )
			break;	

		if ( !ent->GetUnknown() )
			continue;

		CBaseEntity *entity = GetContainingEntity( ent );
		if ( !entity )
			continue;

		if ( entity->GetFlags() & FL_NOTARGET )
			continue;

		// anything that is a client, or has a client as an enemy
		break;
	}

	if ( i != check )
	{
		memset( g_CheckClient.m_checkVisibilityPVS, 0, sizeof(g_CheckClient.m_checkVisibilityPVS) );
		g_CheckClient.m_bClientPVSIsExpanded = false;
	}

	if ( ent )
	{
		// get the PVS for the entity
		CBaseEntity *pce = GetContainingEntity( ent );
		if ( !pce )
			return i;

		org = pce->EyePosition();

		int clusterIndex = engine->GetClusterForOrigin( org );
		if ( clusterIndex != g_CheckClient.m_checkCluster )
		{
			g_CheckClient.m_checkCluster = clusterIndex;
			engine->GetPVSForCluster( clusterIndex, sizeof(g_CheckClient.m_checkPVS), g_CheckClient.m_checkPVS );
		}
	}
	
	return i;
}


//-----------------------------------------------------------------------------
// Gets the current check client....
//-----------------------------------------------------------------------------
static edict_t *UTIL_GetCurrentCheckClient()
{
	edict_t	*ent;

	// find a new check if on a new frame
	float delta = gpGlobals->curtime - g_CheckClient.m_lastchecktime;
	if ( delta >= 0.1 || delta < 0 )
	{
		g_CheckClient.m_lastcheck = UTIL_GetNewCheckClient( g_CheckClient.m_lastcheck );
		g_CheckClient.m_lastchecktime = gpGlobals->curtime;
	}

	// return check if it might be visible	
	ent = engine->PEntityOfEntIndex( g_CheckClient.m_lastcheck );

	// Allow dead clients -- JAY
	// Our monsters know the difference, and this function gates alot of behavior
	// It's annoying to die and see monsters stop thinking because you're no longer
	// "in" their PVS
	if ( !ent || ent->IsFree() || !ent->GetUnknown())
	{
		return NULL;
	}

	return ent;
}

void UTIL_SetClientVisibilityPVS( edict_t *pClient, const unsigned char *pvs, int pvssize )
{
	if ( pClient == UTIL_GetCurrentCheckClient() )
	{
		Assert( pvssize <= sizeof(g_CheckClient.m_checkVisibilityPVS) );

		g_CheckClient.m_bClientPVSIsExpanded = false;

		unsigned *pFrom = (unsigned *)pvs;
		unsigned *pMask = (unsigned *)g_CheckClient.m_checkPVS;
		unsigned *pTo = (unsigned *)g_CheckClient.m_checkVisibilityPVS;

		int limit = pvssize / 4;
		int i;

		for ( i = 0; i < limit; i++ )
		{
			pTo[i] = pFrom[i] & ~pMask[i];

			if ( pFrom[i] )
			{
				g_CheckClient.m_bClientPVSIsExpanded = true;
			}
		}

		int remainder = pvssize % 4;
		for ( i = 0; i < remainder; i++ )
		{
			((unsigned char *)&pTo[limit])[i] = ((unsigned char *)&pFrom[limit])[i] & !((unsigned char *)&pMask[limit])[i];

			if ( ((unsigned char *)&pFrom[limit])[i] != 0)
			{
				g_CheckClient.m_bClientPVSIsExpanded = true;
			}
		}
	}
}

bool UTIL_ClientPVSIsExpanded()
{
	return g_CheckClient.m_bClientPVSIsExpanded;
}


//-----------------------------------------------------------------------------
// Purpose: Returns a client (or object that has a client enemy) that would be a valid target.
//  If there are more than one valid options, they are cycled each frame
//  If (self.origin + self.viewofs) is not in the PVS of the current target, it is not returned at all.
// Input  : *pEdict - 
// Output : edict_t*
//-----------------------------------------------------------------------------
CBaseEntity *UTIL_FindClientInPVS( const Vector &vecBoxMins, const Vector &vecBoxMaxs )
{
	edict_t	*ent = UTIL_GetCurrentCheckClient();
	if ( !ent )
	{
		return NULL;
	}

	if ( !engine->CheckBoxInPVS( vecBoxMins, vecBoxMaxs, g_CheckClient.m_checkPVS, sizeof( g_CheckClient.m_checkPVS ) ) )
	{
		return NULL;
	}

	// might be able to see it
	return GetContainingEntity( ent );
}

//-----------------------------------------------------------------------------
// Purpose: Returns a client (or object that has a client enemy) that would be a valid target.
//  If there are more than one valid options, they are cycled each frame
//  If (self.origin + self.viewofs) is not in the PVS of the current target, it is not returned at all.
// Input  : *pEdict - 
// Output : edict_t*
//-----------------------------------------------------------------------------
ConVar sv_strict_notarget( "sv_strict_notarget", "0", 0, "If set, notarget will cause entities to never think they are in the pvs" );

static edict_t *UTIL_FindClientInPVSGuts(edict_t *pEdict, unsigned char *pvs, unsigned pvssize )
{
	Vector	view;

	edict_t	*ent = UTIL_GetCurrentCheckClient();
	if ( !ent )
	{
		return NULL;
	}

	CBaseEntity *pPlayerEntity = GetContainingEntity( ent );
	if( (!pPlayerEntity || (pPlayerEntity->GetFlags() & FL_NOTARGET)) && sv_strict_notarget.GetBool() )
	{
		return NULL;
	}
	// if current entity can't possibly see the check entity, return 0
	// UNDONE: Build a box for this and do it over that box
	// UNDONE: Use CM_BoxLeafnums()
	CBaseEntity *pe = GetContainingEntity( pEdict );
	if ( pe )
	{
		view = pe->EyePosition();
		
		if ( !engine->CheckOriginInPVS( view, pvs, pvssize ) )
		{
			return NULL;
		}
	}

	// might be able to see it
	return ent;
}

//-----------------------------------------------------------------------------
// Purpose: Returns a client that could see the entity directly
//-----------------------------------------------------------------------------

edict_t *UTIL_FindClientInPVS(edict_t *pEdict)
{
	return UTIL_FindClientInPVSGuts( pEdict, g_CheckClient.m_checkPVS, sizeof( g_CheckClient.m_checkPVS ) );
}

//-----------------------------------------------------------------------------
// Purpose: Returns a client that could see the entity, including through a camera
//-----------------------------------------------------------------------------
edict_t *UTIL_FindClientInVisibilityPVS( edict_t *pEdict )
{
	return UTIL_FindClientInPVSGuts( pEdict, g_CheckClient.m_checkVisibilityPVS, sizeof( g_CheckClient.m_checkVisibilityPVS ) );
}



//-----------------------------------------------------------------------------
// Purpose: Returns a chain of entities within the PVS of another entity (client)
//  starting_ent is the ent currently at in the list
//  a starting_ent of NULL signifies the beginning of a search
// Input  : *pplayer - 
//			*starting_ent - 
// Output : edict_t
//-----------------------------------------------------------------------------
CBaseEntity *UTIL_EntitiesInPVS( CBaseEntity *pPVSEntity, CBaseEntity *pStartingEntity )
{
	Vector			org;
	static byte		pvs[ MAX_MAP_CLUSTERS/8 ];
	static Vector	lastOrg( 0, 0, 0 );
	static int		lastCluster = -1;

	if ( !pPVSEntity )
		return NULL;

	// NOTE: These used to be caching code here to prevent this from
	// being called over+over which breaks when you go back + forth
	// across level transitions
	// So, we'll always get the PVS each time we start a new EntitiesInPVS iteration.
	// Given that weapon_binocs + leveltransition code is the only current clients
	// of this, this seems safe.
	if ( !pStartingEntity )
	{
		org = pPVSEntity->EyePosition();
		int clusterIndex = engine->GetClusterForOrigin( org );
		Assert( clusterIndex >= 0 );
		engine->GetPVSForCluster( clusterIndex, sizeof(pvs), pvs );
	}

	for ( CBaseEntity *pEntity = gEntList.NextEnt(pStartingEntity); pEntity; pEntity = gEntList.NextEnt(pEntity) )
	{
		// Only return attached ents.
		if ( !pEntity->edict() )
			continue;

		CBaseEntity *pParent = pEntity->GetRootMoveParent();

		Vector vecSurroundMins, vecSurroundMaxs;
		pParent->CollisionProp()->WorldSpaceSurroundingBounds( &vecSurroundMins, &vecSurroundMaxs );
		if ( !engine->CheckBoxInPVS( vecSurroundMins, vecSurroundMaxs, pvs, sizeof( pvs ) ) )
			continue;

		return pEntity;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Get the predicted postion of an entity of a certain number of seconds
//			Use this function with caution, it has great potential for annoying the player, especially
//			if used for target firing predition
// Input  : *pTarget - target entity to predict
//			timeDelta - amount of time to predict ahead (in seconds)
//			&vecPredictedPosition - output
//-----------------------------------------------------------------------------
void UTIL_PredictedPosition( CBaseEntity *pTarget, float flTimeDelta, Vector *vecPredictedPosition )
{
	if ( ( pTarget == NULL ) || ( vecPredictedPosition == NULL ) )
		return;

	Vector	vecPredictedVel;

	//FIXME: Should we look at groundspeed or velocity for non-clients??

	//Get the proper velocity to predict with
	CBasePlayer	*pPlayer = ToBasePlayer( pTarget );

	//Player works differently than other entities
	if ( pPlayer != NULL )
	{
		if ( pPlayer->IsInAVehicle() )
		{
			//Calculate the predicted position in this vehicle
			vecPredictedVel = pPlayer->GetVehicleEntity()->GetSmoothedVelocity();
		}
		else
		{
			//Get the player's stored velocity
			vecPredictedVel = pPlayer->GetSmoothedVelocity();
		}
	}
	else
	{
		// See if we're a combat character in a vehicle
		CBaseCombatCharacter *pCCTarget = pTarget->MyCombatCharacterPointer();
		if ( pCCTarget != NULL && pCCTarget->IsInAVehicle() )
		{
			//Calculate the predicted position in this vehicle
			vecPredictedVel = pCCTarget->GetVehicleEntity()->GetSmoothedVelocity();
		}
		else
		{
			// See if we're an animating entity
			CBaseAnimating *pAnimating = dynamic_cast<CBaseAnimating *>(pTarget);
			if ( pAnimating != NULL )
			{
				vecPredictedVel = pAnimating->GetGroundSpeedVelocity();
			}
			else
			{
				// Otherwise we're a vanilla entity
				vecPredictedVel = pTarget->GetSmoothedVelocity();				
			}
		}
	}

	//Get the result
	(*vecPredictedPosition) = pTarget->GetAbsOrigin() + ( vecPredictedVel * flTimeDelta );
}

//-----------------------------------------------------------------------------
// Purpose: Points the destination entity at the target entity
// Input  : *pDest - entity to be pointed at the target
//			*pTarget - target to point at
//-----------------------------------------------------------------------------
bool UTIL_PointAtEntity( CBaseEntity *pDest, CBaseEntity *pTarget )
{
	if ( ( pDest == NULL ) || ( pTarget == NULL ) )
	{
		return false;
	}

	Vector dir = (pTarget->GetAbsOrigin() - pDest->GetAbsOrigin());

	VectorNormalize( dir );

	//Store off as angles
	QAngle angles;
	VectorAngles( dir, angles );
	pDest->SetLocalAngles( angles );
	pDest->SetAbsAngles( angles );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Points the destination entity at the target entity by name
// Input  : *pDest - entity to be pointed at the target
//			strTarget - name of entity to target (will only choose the first!)
//-----------------------------------------------------------------------------
void UTIL_PointAtNamedEntity( CBaseEntity *pDest, string_t strTarget )
{
	//Attempt to find the entity
	if ( !UTIL_PointAtEntity( pDest, gEntList.FindEntityByName( NULL, strTarget ) ) )
	{
		DevMsg( 1, "%s (%s) was unable to point at an entity named: %s\n", pDest->GetClassname(), pDest->GetDebugName(), STRING( strTarget ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Copy the pose parameter values from one entity to the other
// Input  : *pSourceEntity - entity to copy from
//			*pDestEntity - entity to copy to
//-----------------------------------------------------------------------------
bool UTIL_TransferPoseParameters( CBaseEntity *pSourceEntity, CBaseEntity *pDestEntity )
{
	CBaseAnimating *pSourceBaseAnimating = dynamic_cast<CBaseAnimating*>( pSourceEntity );
	CBaseAnimating *pDestBaseAnimating = dynamic_cast<CBaseAnimating*>( pDestEntity );

	if ( !pSourceBaseAnimating || !pDestBaseAnimating )
		return false;

	for ( int iPose = 0; iPose < MAXSTUDIOPOSEPARAM; ++iPose )
	{
		pDestBaseAnimating->SetPoseParameter( iPose, pSourceBaseAnimating->GetPoseParameter( iPose ) );
	}
	
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Make a muzzle flash appear
// Input  : &origin - position of the muzzle flash
//			&angles - angles of the fire direction
//			scale - scale of the muzzle flash
//			type - type of muzzle flash
//-----------------------------------------------------------------------------
void UTIL_MuzzleFlash( const Vector &origin, const QAngle &angles, int scale, int type )
{
	CPASFilter filter( origin );

	te->MuzzleFlash( filter, 0.0f, origin, angles, scale, type );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : vStartPos - start of the line
//			vEndPos - end of the line
//			vPoint - point to find nearest point to on specified line
//			clampEnds - clamps returned points to being on the line segment specified
// Output : Vector - nearest point on the specified line
//-----------------------------------------------------------------------------
Vector UTIL_PointOnLineNearestPoint(const Vector& vStartPos, const Vector& vEndPos, const Vector& vPoint, bool clampEnds )
{
	Vector	vEndToStart		= (vEndPos - vStartPos);
	Vector	vOrgToStart		= (vPoint  - vStartPos);
	float	fNumerator		= DotProduct(vEndToStart,vOrgToStart);
	float	fDenominator	= vEndToStart.Length() * vOrgToStart.Length();
	float	fIntersectDist	= vOrgToStart.Length()*(fNumerator/fDenominator);
	float	flLineLength	= VectorNormalize( vEndToStart ); 
	
	if ( clampEnds )
	{
		fIntersectDist = clamp( fIntersectDist, 0.0f, flLineLength );
	}
	
	Vector	vIntersectPos	= vStartPos + vEndToStart * fIntersectDist;

	return vIntersectPos;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
AngularImpulse WorldToLocalRotation( const VMatrix &localToWorld, const Vector &worldAxis, float rotation )
{
	// fix axes of rotation to match axes of vector
	Vector rot = worldAxis * rotation;
	// since the matrix maps local to world, do a transpose rotation to get world to local
	AngularImpulse ang = localToWorld.VMul3x3Transpose( rot );

	return ang;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//			*pLength - 
// Output : byte
//-----------------------------------------------------------------------------
byte *UTIL_LoadFileForMe( const char *filename, int *pLength )
{
	void *buffer = NULL;

	int length = filesystem->ReadFileEx( filename, "GAME", &buffer, true, true );

	if ( pLength )
	{
		*pLength = length;
	}

	return (byte *)buffer;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *buffer - 
//-----------------------------------------------------------------------------
void UTIL_FreeFile( byte *buffer )
{
	filesystem->FreeOptimalReadBuffer( buffer );
}

//-----------------------------------------------------------------------------
// Purpose: Determines whether an entity is within a certain angular tolerance to viewer
// Input  : *pEntity - entity which is the "viewer"
//			vecPosition - position to test against
//			flTolerance - tolerance (as dot-product)
//			*pflDot - if not NULL, holds the 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool UTIL_IsFacingWithinTolerance( CBaseEntity *pViewer, const Vector &vecPosition, float flDotTolerance, float *pflDot /*= NULL*/ )
{
	if ( pflDot )
	{
		*pflDot = 0.0f;
	}

	// Required elements
	if ( pViewer == NULL )
		return false;

	Vector forward;
	pViewer->GetVectors( &forward, NULL, NULL );

	Vector dir = vecPosition - pViewer->GetAbsOrigin();
	VectorNormalize( dir );

	// Larger dot product corresponds to a smaller angle
	float flDot = dir.Dot( forward );

	// Return the result
	if ( pflDot )
	{
		*pflDot = flDot;
	}

	// Within the goal tolerance
	if ( flDot >= flDotTolerance )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Determines whether an entity is within a certain angular tolerance to viewer
// Input  : *pEntity - entity which is the "viewer"
//			*pTarget - entity to test against
//			flTolerance - tolerance (as dot-product)
//			*pflDot - if not NULL, holds the 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool UTIL_IsFacingWithinTolerance( CBaseEntity *pViewer, CBaseEntity *pTarget, float flDotTolerance, float *pflDot /*= NULL*/ )
{
	if ( pViewer == NULL || pTarget == NULL )
		return false;

	return UTIL_IsFacingWithinTolerance( pViewer, pTarget->GetAbsOrigin(), flDotTolerance, pflDot );
}

//-----------------------------------------------------------------------------
// Purpose: Fills in color for debug purposes based on a relationship
// Input  : nRelationship - relationship to test
//			*pR, *pG, *pB - colors to fill
//-----------------------------------------------------------------------------
void UTIL_GetDebugColorForRelationship( int nRelationship, int &r, int &g, int &b )
{
	switch ( nRelationship )
	{
	case D_LI:
		r = 0;
		g = 255;
		b = 0;
		break;
	case D_NU:
		r = 0;
		g = 0;
		b = 255;
		break;
	case D_HT:
		r = 255;
		g = 0;
		b = 0;
		break;
	case D_FR:
		r = 255;
		g = 255;
		b = 0;
		break;
	default:
		r = 255;
		g = 255;
		b = 255;
		break;
	}
}

void LoadAndSpawnEntities_ParseEntKVBlockHelper( CBaseEntity *pNode, KeyValues *pkvNode )
{
	KeyValues *pkvNodeData = pkvNode->GetFirstSubKey();
	while ( pkvNodeData )
	{
		// Handle the connections block
		if ( !Q_strcmp(pkvNodeData->GetName(), "connections") )
		{
			LoadAndSpawnEntities_ParseEntKVBlockHelper( pNode, pkvNodeData );
		}
		else
		{
			pNode->KeyValue( pkvNodeData->GetName(), pkvNodeData->GetString() );
		}

		pkvNodeData = pkvNodeData->GetNextKey();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Loads and parses a file and spawns entities defined in it.
//-----------------------------------------------------------------------------
bool UTIL_LoadAndSpawnEntitiesFromScript( CUtlVector <CBaseEntity*> &entities, const char *pScriptFile, const char *pBlock, bool bActivate )
{
	KeyValues *pkvFile = new KeyValues( pBlock );

	if ( pkvFile->LoadFromFile( filesystem, pScriptFile, "MOD" ) )
	{	
		// Load each block, and spawn the entities
		KeyValues *pkvNode = pkvFile->GetFirstSubKey();
		while ( pkvNode )
		{
			// Get name
			const char *pNodeName = pkvNode->GetName();

			if ( stricmp( pNodeName, "entity" ) )
			{
				pkvNode = pkvNode->GetNextKey();
				continue;
			}

			KeyValues *pClassname = pkvNode->FindKey( "classname" );

			if ( pClassname )
			{
				// Use the classname instead
				pNodeName = pClassname->GetString();
			}

			// Spawn the entity
			CBaseEntity *pNode = CreateEntityByName( pNodeName );

			if ( pNode )
			{
				LoadAndSpawnEntities_ParseEntKVBlockHelper( pNode, pkvNode );
				DispatchSpawn( pNode );
				entities.AddToTail( pNode );
			}
			else
			{
				Warning( "UTIL_LoadAndSpawnEntitiesFromScript: Failed to spawn entity, type: '%s'\n", pNodeName );
			}

			// Move to next entity
			pkvNode = pkvNode->GetNextKey();
		}

		if ( bActivate == true )
		{
			bool bAsyncAnims = mdlcache->SetAsyncLoad( MDLCACHE_ANIMBLOCK, false );
			// Then activate all the entities
			for ( int i = 0; i < entities.Count(); i++ )
			{
				entities[i]->Activate();
			}
			mdlcache->SetAsyncLoad( MDLCACHE_ANIMBLOCK, bAsyncAnims );
		}
	}
	else
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Convert a vector an angle from worldspace to the entity's parent's local space
// Input  : *pEntity - Entity whose parent we're concerned with
//-----------------------------------------------------------------------------
void UTIL_ParentToWorldSpace( CBaseEntity *pEntity, Vector &vecPosition, QAngle &vecAngles )
{
	if ( pEntity == NULL )
		return;

	// Construct the entity-to-world matrix
	// Start with making an entity-to-parent matrix
	matrix3x4_t matEntityToParent;
	AngleMatrix( vecAngles, matEntityToParent );
	MatrixSetColumn( vecPosition, 3, matEntityToParent );

	// concatenate with our parent's transform
	matrix3x4_t matScratch, matResult;
	matrix3x4_t matParentToWorld;
	
	if ( pEntity->GetParent() != NULL )
	{
		matParentToWorld = pEntity->GetParentToWorldTransform( matScratch );
	}
	else
	{
		matParentToWorld = pEntity->EntityToWorldTransform();
	}

	ConcatTransforms( matParentToWorld, matEntityToParent, matResult );

	// pull our absolute position out of the matrix
	MatrixGetColumn( matResult, 3, vecPosition );
	MatrixAngles( matResult, vecAngles );
}

//-----------------------------------------------------------------------------
// Purpose: Convert a vector and quaternion from worldspace to the entity's parent's local space
// Input  : *pEntity - Entity whose parent we're concerned with
//-----------------------------------------------------------------------------
void UTIL_ParentToWorldSpace( CBaseEntity *pEntity, Vector &vecPosition, Quaternion &quat )
{
	if ( pEntity == NULL )
		return;

	QAngle vecAngles;
	QuaternionAngles( quat, vecAngles );
	UTIL_ParentToWorldSpace( pEntity, vecPosition, vecAngles );
	AngleQuaternion( vecAngles, quat );
}

//-----------------------------------------------------------------------------
// Purpose: Convert a vector an angle from worldspace to the entity's parent's local space
// Input  : *pEntity - Entity whose parent we're concerned with
//-----------------------------------------------------------------------------
void UTIL_WorldToParentSpace( CBaseEntity *pEntity, Vector &vecPosition, QAngle &vecAngles )
{
	if ( pEntity == NULL )
		return;

	// Construct the entity-to-world matrix
	// Start with making an entity-to-parent matrix
	matrix3x4_t matEntityToParent;
	AngleMatrix( vecAngles, matEntityToParent );
	MatrixSetColumn( vecPosition, 3, matEntityToParent );

	// concatenate with our parent's transform
	matrix3x4_t matScratch, matResult;
	matrix3x4_t matWorldToParent;
	
	if ( pEntity->GetParent() != NULL )
	{
		matScratch = pEntity->GetParentToWorldTransform( matScratch );
	}
	else
	{
		matScratch = pEntity->EntityToWorldTransform();
	}

	MatrixInvert( matScratch, matWorldToParent );
	ConcatTransforms( matWorldToParent, matEntityToParent, matResult );

	// pull our absolute position out of the matrix
	MatrixGetColumn( matResult, 3, vecPosition );
	MatrixAngles( matResult, vecAngles );
}

//-----------------------------------------------------------------------------
// Purpose: Convert a vector and quaternion from worldspace to the entity's parent's local space
// Input  : *pEntity - Entity whose parent we're concerned with
//-----------------------------------------------------------------------------
void UTIL_WorldToParentSpace( CBaseEntity *pEntity, Vector &vecPosition, Quaternion &quat )
{
	if ( pEntity == NULL )
		return;

	QAngle vecAngles;
	QuaternionAngles( quat, vecAngles );
	UTIL_WorldToParentSpace( pEntity, vecPosition, vecAngles );
	AngleQuaternion( vecAngles, quat );
}

//-----------------------------------------------------------------------------
// Purpose: Given a vector, clamps the scalar axes to MAX_COORD_FLOAT ranges from worldsize.h
// Input  : *pVecPos - 
//-----------------------------------------------------------------------------
void UTIL_BoundToWorldSize( Vector *pVecPos )
{
	Assert( pVecPos );
	for ( int i = 0; i < 3; ++i )
	{
		(*pVecPos)[ i ] = clamp( (*pVecPos)[ i ], MIN_COORD_FLOAT, MAX_COORD_FLOAT );
	}
}

//=============================================================================
//
// Tests!
//

#define NUM_KDTREE_TESTS		2500
#define NUM_KDTREE_ENTITY_SIZE	256

void CC_KDTreeTest( const CCommand &args )
{
	Msg( "Testing kd-tree entity queries." );

	// Get the testing spot.
//	CBaseEntity *pSpot = gEntList.FindEntityByClassname( NULL, "info_player_start" );
//	Vector vecStart = pSpot->GetAbsOrigin();

	CBasePlayer *pPlayer = static_cast<CBasePlayer*>( UTIL_GetLocalPlayer() );
	Vector vecStart = pPlayer->GetAbsOrigin();

	static Vector *vecTargets = NULL;
	static bool bFirst = true;

	// Generate the targets - rays (1K long).
	if ( bFirst )
	{
		vecTargets = new Vector [NUM_KDTREE_TESTS];
		double flRadius = 0;
		double flTheta = 0;
		double flPhi = 0;
		for ( int i = 0; i < NUM_KDTREE_TESTS; ++i )
		{
			flRadius += NUM_KDTREE_TESTS * 123.123;
			flRadius =  fmod( flRadius, 128.0 );
			flRadius =  fabs( flRadius );

			flTheta  += NUM_KDTREE_TESTS * 76.76;
			flTheta  =  fmod( flTheta, (double) DEG2RAD( 360 ) );
			flTheta  =  fabs( flTheta );

			flPhi    += NUM_KDTREE_TESTS * 1997.99;
			flPhi    =  fmod( flPhi, (double) DEG2RAD( 180 ) );
			flPhi    =  fabs( flPhi );

			float st, ct, sp, cp;
			SinCos( flTheta, &st, &ct );
			SinCos( flPhi, &sp, &cp );

			vecTargets[i].x = flRadius * ct * sp;
			vecTargets[i].y = flRadius * st * sp;
			vecTargets[i].z = flRadius * cp;
			
			// Make the trace 1024 units long.
			Vector vecDir = vecTargets[i] - vecStart;
			VectorNormalize( vecDir );
			vecTargets[i] = vecStart + vecDir * 1024;
		}

		bFirst = false;
	}

	int nTestType = 0;
	if ( args.ArgC() >= 2 )
	{
		nTestType = atoi( args[ 1 ] );
	}

	vtune( true );

#ifdef VPROF_ENABLED
	g_VProfCurrentProfile.Resume();
	g_VProfCurrentProfile.Start();
	g_VProfCurrentProfile.Reset();
	g_VProfCurrentProfile.MarkFrame();
#endif

	switch ( nTestType )
	{
	case 0:
		{
			VPROF( "TraceTotal" );			

			trace_t trace;
			for ( int iTest = 0; iTest < NUM_KDTREE_TESTS; ++iTest )
			{
				UTIL_TraceLine( vecStart, vecTargets[iTest], MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &trace );
			}
			break;
		}
	case 1:
		{
			VPROF( "TraceTotal" );

			trace_t trace;
			for ( int iTest = 0; iTest < NUM_KDTREE_TESTS; ++iTest )
			{
				UTIL_TraceHull( vecStart, vecTargets[iTest], VEC_HULL_MIN_SCALED( pPlayer ), VEC_HULL_MAX_SCALED( pPlayer ), MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &trace );
			}
			break;
		}
	case 2:
		{
			Vector vecMins[NUM_KDTREE_TESTS];
			Vector vecMaxs[NUM_KDTREE_TESTS];
			int iTest;
			for ( iTest = 0; iTest < NUM_KDTREE_TESTS; ++iTest )
			{
				vecMins[iTest] = vecStart;
				vecMaxs[iTest] = vecStart;
				for ( int iAxis = 0; iAxis < 3; ++iAxis )
				{
					if ( vecTargets[iTest].x < vecMins[iTest].x ) { vecMins[iTest].x = vecTargets[iTest].x; }
					if ( vecTargets[iTest].y < vecMins[iTest].y ) { vecMins[iTest].y = vecTargets[iTest].y; }
					if ( vecTargets[iTest].z < vecMins[iTest].z ) { vecMins[iTest].z = vecTargets[iTest].z; }

					if ( vecTargets[iTest].x > vecMaxs[iTest].x ) { vecMaxs[iTest].x = vecTargets[iTest].x; }
					if ( vecTargets[iTest].y > vecMaxs[iTest].y ) { vecMaxs[iTest].y = vecTargets[iTest].y; }
					if ( vecTargets[iTest].z > vecMaxs[iTest].z ) { vecMaxs[iTest].z = vecTargets[iTest].z; }
				}
			}


			VPROF( "TraceTotal" );

			int nCount = 0;

			Vector vecDelta;
			trace_t trace;
			CBaseEntity *pList[1024];
			for ( iTest = 0; iTest < NUM_KDTREE_TESTS; ++iTest )
			{
				nCount += UTIL_EntitiesInBox( pList, 1024, vecMins[iTest], vecMaxs[iTest], 0 );
			}

			Msg( "Count = %d\n", nCount );
			break;
		}
	case 3:
		{
			Vector vecDelta;
			float flRadius[NUM_KDTREE_TESTS];
			int iTest;
			for ( iTest = 0; iTest < NUM_KDTREE_TESTS; ++iTest )
			{
				VectorSubtract( vecTargets[iTest], vecStart, vecDelta );
				flRadius[iTest] = vecDelta.Length() * 0.5f;
			}

			VPROF( "TraceTotal" );

			int nCount = 0;

			trace_t trace;
			CBaseEntity *pList[1024];
			for ( iTest = 0; iTest < NUM_KDTREE_TESTS; ++iTest )
			{
				nCount += UTIL_EntitiesInSphere( pList, 1024, vecStart, flRadius[iTest], 0 );
			}

			Msg( "Count = %d\n", nCount );
			break;
		}
	default:
		{
			break;
		}
	}

#ifdef VPROF_ENABLED
	g_VProfCurrentProfile.MarkFrame();
	g_VProfCurrentProfile.Pause();
	g_VProfCurrentProfile.OutputReport( VPRT_FULL );
#endif
	
	vtune( false );
}

static ConCommand kdtree_test( "kdtree_test", CC_KDTreeTest, "Tests spatial partition for entities queries.", FCVAR_CHEAT );

void CC_VoxelTreeView( void )
{
	Msg( "VoxelTreeView\n" );
	partition->RenderAllObjectsInTree( 10.0f );
}

static ConCommand voxeltree_view( "voxeltree_view", CC_VoxelTreeView, "View entities in the voxel-tree.", FCVAR_CHEAT );

void CC_VoxelTreePlayerView( void )
{
	Msg( "VoxelTreePlayerView\n" );

	CBasePlayer *pPlayer = static_cast<CBasePlayer*>( UTIL_GetLocalPlayer() );
	Vector vecStart = pPlayer->GetAbsOrigin();
	partition->RenderObjectsInPlayerLeafs( vecStart - VEC_HULL_MIN_SCALED( pPlayer ), vecStart + VEC_HULL_MAX_SCALED( pPlayer ), 3.0f  );
}

static ConCommand voxeltree_playerview( "voxeltree_playerview", CC_VoxelTreePlayerView, "View entities in the voxel-tree at the player position.", FCVAR_CHEAT );

void CC_VoxelTreeBox( const CCommand &args )
{
	Vector vecMin, vecMax;
	if ( args.ArgC() >= 6 )
	{
		vecMin.x = atof( args[ 1 ] );
		vecMin.y = atof( args[ 2 ] );
		vecMin.z = atof( args[ 3 ] );

		vecMax.x = atof( args[ 4 ] );
		vecMax.y = atof( args[ 5 ] );
		vecMax.z = atof( args[ 6 ] );
	}
	else
	{
		return;
	}

	float flTime = 10.0f;

	Vector vecPoints[8];
	vecPoints[0].Init( vecMin.x, vecMin.y, vecMin.z );
	vecPoints[1].Init( vecMin.x, vecMax.y, vecMin.z );
	vecPoints[2].Init( vecMax.x, vecMax.y, vecMin.z );
	vecPoints[3].Init( vecMax.x, vecMin.y, vecMin.z );
	vecPoints[4].Init( vecMin.x, vecMin.y, vecMax.z );
	vecPoints[5].Init( vecMin.x, vecMax.y, vecMax.z );
	vecPoints[6].Init( vecMax.x, vecMax.y, vecMax.z );
	vecPoints[7].Init( vecMax.x, vecMin.y, vecMax.z );
	
	debugoverlay->AddLineOverlay( vecPoints[0], vecPoints[1], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[1], vecPoints[2], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[2], vecPoints[3], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[3], vecPoints[0], 255, 0, 0, true, flTime );
	
	debugoverlay->AddLineOverlay( vecPoints[4], vecPoints[5], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[5], vecPoints[6], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[6], vecPoints[7], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[7], vecPoints[4], 255, 0, 0, true, flTime );
	
	debugoverlay->AddLineOverlay( vecPoints[0], vecPoints[4], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[3], vecPoints[7], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[1], vecPoints[5], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[2], vecPoints[6], 255, 0, 0, true, flTime );

	Msg( "VoxelTreeBox - (%f %f %f) to (%f %f %f)\n", vecMin.x, vecMin.y, vecMin.z, vecMax.x, vecMax.y, vecMax.z );
	partition->RenderObjectsInBox( vecMin, vecMax, flTime );
}

static ConCommand voxeltree_box( "voxeltree_box", CC_VoxelTreeBox, "View entities in the voxel-tree inside box <Vector(min), Vector(max)>.", FCVAR_CHEAT );

void CC_VoxelTreeSphere( const CCommand &args )
{
	Vector vecCenter;
	float flRadius;
	if ( args.ArgC() >= 4 )
	{
		vecCenter.x = atof( args[ 1 ] );
		vecCenter.y = atof( args[ 2 ] );
		vecCenter.z = atof( args[ 3 ] );

		flRadius = atof( args[ 3 ] );
	}
	else
	{
		return;
	}

	float flTime = 3.0f;

	Vector vecMin, vecMax;
	vecMin.Init( vecCenter.x - flRadius, vecCenter.y - flRadius, vecCenter.z - flRadius );
	vecMax.Init( vecCenter.x + flRadius, vecCenter.y + flRadius, vecCenter.z + flRadius );

	Vector vecPoints[8];
	vecPoints[0].Init( vecMin.x, vecMin.y, vecMin.z );
	vecPoints[1].Init( vecMin.x, vecMax.y, vecMin.z );
	vecPoints[2].Init( vecMax.x, vecMax.y, vecMin.z );
	vecPoints[3].Init( vecMax.x, vecMin.y, vecMin.z );
	vecPoints[4].Init( vecMin.x, vecMin.y, vecMax.z );
	vecPoints[5].Init( vecMin.x, vecMax.y, vecMax.z );
	vecPoints[6].Init( vecMax.x, vecMax.y, vecMax.z );
	vecPoints[7].Init( vecMax.x, vecMin.y, vecMax.z );
	
	debugoverlay->AddLineOverlay( vecPoints[0], vecPoints[1], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[1], vecPoints[2], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[2], vecPoints[3], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[3], vecPoints[0], 255, 0, 0, true, flTime );
	
	debugoverlay->AddLineOverlay( vecPoints[4], vecPoints[5], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[5], vecPoints[6], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[6], vecPoints[7], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[7], vecPoints[4], 255, 0, 0, true, flTime );
	
	debugoverlay->AddLineOverlay( vecPoints[0], vecPoints[4], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[3], vecPoints[7], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[1], vecPoints[5], 255, 0, 0, true, flTime );
	debugoverlay->AddLineOverlay( vecPoints[2], vecPoints[6], 255, 0, 0, true, flTime );

	Msg( "VoxelTreeSphere - (%f %f %f), %f\n", vecCenter.x, vecCenter.y, vecCenter.z, flRadius );
	partition->RenderObjectsInSphere( vecCenter, flRadius, flTime );
}

static ConCommand voxeltree_sphere( "voxeltree_sphere", CC_VoxelTreeSphere, "View entities in the voxel-tree inside sphere <Vector(center), float(radius)>.", FCVAR_CHEAT );



#define NUM_COLLISION_TESTS 2500
void CC_CollisionTest( const CCommand &args )
{
	if ( !physenv )
		return;

	Msg( "Testing collision system\n" );
	partition->ReportStats( "" );
	int i;
	CBaseEntity *pSpot = gEntList.FindEntityByClassname( NULL, "info_player_start");
	Vector start = pSpot->GetAbsOrigin();
	static Vector *targets = NULL;
	static bool first = true;
	static float test[2] = {1,1};
	if ( first )
	{
		targets = new Vector[NUM_COLLISION_TESTS];
		float radius = 0;
		float theta = 0;
		float phi = 0;
		for ( i = 0; i < NUM_COLLISION_TESTS; i++ )
		{
			radius += NUM_COLLISION_TESTS * 123.123;
			radius = fabs(fmod(radius, 128));
			theta += NUM_COLLISION_TESTS * 76.76;
			theta = fabs(fmod(theta, DEG2RAD(360)));
			phi += NUM_COLLISION_TESTS * 1997.99;
			phi = fabs(fmod(phi, DEG2RAD(180)));
			
			float st, ct, sp, cp;
			SinCos( theta, &st, &ct );
			SinCos( phi, &sp, &cp );

			targets[i].x = radius * ct * sp;
			targets[i].y = radius * st * sp;
			targets[i].z = radius * cp;
			
			// make the trace 1024 units long
			Vector dir = targets[i] - start;
			VectorNormalize(dir);
			targets[i] = start + dir * 1024;
		}
		first = false;
	}

	//Vector results[NUM_COLLISION_TESTS];

	int testType = 0;
	if ( args.ArgC() >= 2 )
	{
		testType = atoi(args[1]);
	}
	float duration = 0;
	Vector size[2];
	size[0].Init(0,0,0);
	size[1].Init(16,16,16);
	unsigned int dots = 0;
	int nMask = MASK_ALL & ~(CONTENTS_MONSTER | CONTENTS_HITBOX );
	for ( int j = 0; j < 2; j++ )
	{
		float startTime = engine->Time();
		if ( testType == 1 )
		{
			trace_t tr;
			for ( i = 0; i < NUM_COLLISION_TESTS; i++ )
			{
				UTIL_TraceHull( start, targets[i], -size[1], size[1], nMask, NULL, COLLISION_GROUP_NONE, &tr );
			}
		}
		else
		{
			testType = 0;
			trace_t tr;

			for ( i = 0; i < NUM_COLLISION_TESTS; i++ )
			{
				if ( i == 0 )
				{
					partition->RenderLeafsForRayTraceStart( 10.0f );
				}

				UTIL_TraceLine( start, targets[i], nMask, NULL, COLLISION_GROUP_NONE, &tr );

				if ( i == 0 )
				{
					partition->RenderLeafsForRayTraceEnd( );
				}
			}
		}

		duration += engine->Time() - startTime;
	}
	test[testType] = duration;
	Msg("%d collisions in %.2f ms (%u dots)\n", NUM_COLLISION_TESTS, duration*1000, dots );
	partition->ReportStats( "" );
#if 1
	int red = 255, green = 0, blue = 0;
	for ( i = 0; i < 1 /*NUM_COLLISION_TESTS*/; i++ )
	{
		NDebugOverlay::Line( start, targets[i], red, green, blue, false, 2 );
	}
#endif
}
static ConCommand collision_test("collision_test", CC_CollisionTest, "Tests collision system", FCVAR_CHEAT );

// HL1

float UTIL_WeaponTimeBase( void )
{
#if defined( CLIENT_WEAPONS )
	return 0.0;
#else
	return gpGlobalsHL1->time;
#endif
}

static unsigned int glSeed = 0; 

unsigned int seed_table[ 256 ] =
{
	28985, 27138, 26457, 9451, 17764, 10909, 28790, 8716, 6361, 4853, 17798, 21977, 19643, 20662, 10834, 20103,
	27067, 28634, 18623, 25849, 8576, 26234, 23887, 18228, 32587, 4836, 3306, 1811, 3035, 24559, 18399, 315,
	26766, 907, 24102, 12370, 9674, 2972, 10472, 16492, 22683, 11529, 27968, 30406, 13213, 2319, 23620, 16823,
	10013, 23772, 21567, 1251, 19579, 20313, 18241, 30130, 8402, 20807, 27354, 7169, 21211, 17293, 5410, 19223,
	10255, 22480, 27388, 9946, 15628, 24389, 17308, 2370, 9530, 31683, 25927, 23567, 11694, 26397, 32602, 15031,
	18255, 17582, 1422, 28835, 23607, 12597, 20602, 10138, 5212, 1252, 10074, 23166, 19823, 31667, 5902, 24630,
	18948, 14330, 14950, 8939, 23540, 21311, 22428, 22391, 3583, 29004, 30498, 18714, 4278, 2437, 22430, 3439,
	28313, 23161, 25396, 13471, 19324, 15287, 2563, 18901, 13103, 16867, 9714, 14322, 15197, 26889, 19372, 26241,
	31925, 14640, 11497, 8941, 10056, 6451, 28656, 10737, 13874, 17356, 8281, 25937, 1661, 4850, 7448, 12744,
	21826, 5477, 10167, 16705, 26897, 8839, 30947, 27978, 27283, 24685, 32298, 3525, 12398, 28726, 9475, 10208,
	617, 13467, 22287, 2376, 6097, 26312, 2974, 9114, 21787, 28010, 4725, 15387, 3274, 10762, 31695, 17320,
	18324, 12441, 16801, 27376, 22464, 7500, 5666, 18144, 15314, 31914, 31627, 6495, 5226, 31203, 2331, 4668,
	12650, 18275, 351, 7268, 31319, 30119, 7600, 2905, 13826, 11343, 13053, 15583, 30055, 31093, 5067, 761,
	9685, 11070, 21369, 27155, 3663, 26542, 20169, 12161, 15411, 30401, 7580, 31784, 8985, 29367, 20989, 14203,
	29694, 21167, 10337, 1706, 28578, 887, 3373, 19477, 14382, 675, 7033, 15111, 26138, 12252, 30996, 21409,
	25678, 18555, 13256, 23316, 22407, 16727, 991, 9236, 5373, 29402, 6117, 15241, 27715, 19291, 19888, 19847
};

unsigned int U_Random( void ) 
{ 
	glSeed *= 69069; 
	glSeed += seed_table[ glSeed & 0xff ];
 
	return ( ++glSeed & 0x0fffffff ); 
} 

void U_Srand( unsigned int seed )
{
	glSeed = seed_table[ seed & 0xff ];
}

/*
=====================
UTIL_SharedRandomLong
=====================
*/
int UTIL_SharedRandomLong( unsigned int seed, int low, int high )
{
	unsigned int range;

	U_Srand( (int)seed + low + high );

	range = high - low + 1;
	if ( !(range - 1) )
	{
		return low;
	}
	else
	{
		int offset;
		int rnum;

		rnum = U_Random();

		offset = rnum % range;

		return (low + offset);
	}
}

/*
=====================
UTIL_SharedRandomFloat
=====================
*/
float UTIL_SharedRandomFloat( unsigned int seed, float low, float high )
{
	//
	unsigned int range;

	U_Srand( (int)seed + *(int *)&low + *(int *)&high );

	U_Random();
	U_Random();

	range = high - low;
	if ( !range )
	{
		return low;
	}
	else
	{
		int tensixrand;
		float offset;

		tensixrand = U_Random() & 65535;

		offset = (float)tensixrand / 65536.0;

		return (low + offset * range );
	}
}

void UTIL_ParametricRocket( entvars_t *pev, VectorHL1 vecOrigin, VectorHL1 vecAngles, edict_t_hl1 *owner )
{	
	pev->startpos = vecOrigin;
	// Trace out line to end pos
	TraceResult tr;
	UTIL_MakeVectors( vecAngles );
	UTIL_TraceLine( pev->startpos, pev->startpos + gpGlobalsHL1->v_forward * 8192, ignore_monsters, owner, &tr);
	pev->endpos = tr.vecEndPos;

	// Now compute how long it will take based on current velocity
	VectorHL1 vecTravel = pev->endpos - pev->startpos;
	float travelTime = 0.0;
	if ( pev->velocity.Length() > 0 )
	{
		travelTime = vecTravel.Length() / pev->velocity.Length();
	}
	pev->starttime = gpGlobalsHL1->time;
	pev->impacttime = gpGlobalsHL1->time + travelTime;
}

int g_groupmask = 0;
int g_groupop = 0;

// Normal overrides
void UTIL_SetGroupTrace( int groupmask, int op )
{
	g_groupmask		= groupmask;
	g_groupop		= op;

	ENGINE_SETGROUPMASK( g_groupmask, g_groupop );
}

void UTIL_UnsetGroupTrace( void )
{
	g_groupmask		= 0;
	g_groupop		= 0;

	ENGINE_SETGROUPMASK( 0, 0 );
}

// Smart version, it'll clean itself up when it pops off stack
UTIL_GroupTrace::UTIL_GroupTrace( int groupmask, int op )
{
	m_oldgroupmask	= g_groupmask;
	m_oldgroupop	= g_groupop;

	g_groupmask		= groupmask;
	g_groupop		= op;

	ENGINE_SETGROUPMASK( g_groupmask, g_groupop );
}

UTIL_GroupTrace::~UTIL_GroupTrace( void )
{
	g_groupmask		=	m_oldgroupmask;
	g_groupop		=	m_oldgroupop;

	ENGINE_SETGROUPMASK( g_groupmask, g_groupop );
}

TYPEDESCRIPTION	gEntvarsDescription[] = 
{
	DEFINE_ENTITY_FIELD( classname, FIELD_STRING_HL1 ),
	DEFINE_ENTITY_GLOBAL_FIELD( globalname, FIELD_STRING_HL1 ),
	
	DEFINE_ENTITY_FIELD( origin, FIELD_POSITION_VECTOR_HL1 ),
	DEFINE_ENTITY_FIELD( oldorigin, FIELD_POSITION_VECTOR_HL1 ),
	DEFINE_ENTITY_FIELD( velocity, FIELD_VECTOR_HL1 ),
	DEFINE_ENTITY_FIELD( basevelocity, FIELD_VECTOR_HL1 ),
	DEFINE_ENTITY_FIELD( movedir, FIELD_VECTOR_HL1 ),

	DEFINE_ENTITY_FIELD( angles, FIELD_VECTOR_HL1 ),
	DEFINE_ENTITY_FIELD( avelocity, FIELD_VECTOR_HL1 ),
	DEFINE_ENTITY_FIELD( punchangle, FIELD_VECTOR_HL1 ),
	DEFINE_ENTITY_FIELD( v_angle, FIELD_VECTOR_HL1 ),
	DEFINE_ENTITY_FIELD( fixangle, FIELD_FLOAT_HL1 ),
	DEFINE_ENTITY_FIELD( idealpitch, FIELD_FLOAT_HL1 ),
	DEFINE_ENTITY_FIELD( pitch_speed, FIELD_FLOAT_HL1 ),
	DEFINE_ENTITY_FIELD( ideal_yaw, FIELD_FLOAT_HL1 ),
	DEFINE_ENTITY_FIELD( yaw_speed, FIELD_FLOAT_HL1 ),

	DEFINE_ENTITY_FIELD( modelindex, FIELD_INTEGER_HL1 ),
	DEFINE_ENTITY_GLOBAL_FIELD( model, FIELD_MODELNAME_HL1 ),

	DEFINE_ENTITY_FIELD( viewmodel, FIELD_MODELNAME_HL1 ),
	DEFINE_ENTITY_FIELD( weaponmodel, FIELD_MODELNAME_HL1 ),

	DEFINE_ENTITY_FIELD( absmin, FIELD_POSITION_VECTOR_HL1 ),
	DEFINE_ENTITY_FIELD( absmax, FIELD_POSITION_VECTOR_HL1 ),
	DEFINE_ENTITY_GLOBAL_FIELD( mins, FIELD_VECTOR_HL1 ),
	DEFINE_ENTITY_GLOBAL_FIELD( maxs, FIELD_VECTOR_HL1 ),
	DEFINE_ENTITY_GLOBAL_FIELD( size, FIELD_VECTOR_HL1 ),

	DEFINE_ENTITY_FIELD( ltime, FIELD_TIME_HL1 ),
	DEFINE_ENTITY_FIELD( nextthink, FIELD_TIME_HL1 ),

	DEFINE_ENTITY_FIELD( solid, FIELD_INTEGER_HL1 ),
	DEFINE_ENTITY_FIELD( movetype, FIELD_INTEGER_HL1 ),

	DEFINE_ENTITY_FIELD( skin, FIELD_INTEGER_HL1 ),
	DEFINE_ENTITY_FIELD( body, FIELD_INTEGER_HL1 ),
	DEFINE_ENTITY_FIELD( effects, FIELD_INTEGER_HL1 ),

	DEFINE_ENTITY_FIELD( gravity, FIELD_FLOAT_HL1 ),
	DEFINE_ENTITY_FIELD( friction, FIELD_FLOAT_HL1 ),
	DEFINE_ENTITY_FIELD( light_level, FIELD_FLOAT_HL1 ),

	DEFINE_ENTITY_FIELD( frame, FIELD_FLOAT_HL1 ),
	DEFINE_ENTITY_FIELD( scale, FIELD_FLOAT_HL1 ),
	DEFINE_ENTITY_FIELD( sequence, FIELD_INTEGER_HL1 ),
	DEFINE_ENTITY_FIELD( animtime, FIELD_TIME_HL1 ),
	DEFINE_ENTITY_FIELD( framerate, FIELD_FLOAT_HL1 ),
	DEFINE_ENTITY_FIELD( controller, FIELD_INTEGER_HL1 ),
	DEFINE_ENTITY_FIELD( blending, FIELD_INTEGER_HL1 ),

	DEFINE_ENTITY_FIELD( rendermode, FIELD_INTEGER_HL1 ),
	DEFINE_ENTITY_FIELD( renderamt, FIELD_FLOAT_HL1 ),
	DEFINE_ENTITY_FIELD( rendercolor, FIELD_VECTOR_HL1 ),
	DEFINE_ENTITY_FIELD( renderfx, FIELD_INTEGER_HL1 ),

	DEFINE_ENTITY_FIELD( health, FIELD_FLOAT_HL1 ),
	DEFINE_ENTITY_FIELD( frags, FIELD_FLOAT_HL1 ),
	DEFINE_ENTITY_FIELD( weapons, FIELD_INTEGER_HL1 ),
	DEFINE_ENTITY_FIELD( takedamage, FIELD_FLOAT_HL1 ),

	DEFINE_ENTITY_FIELD( deadflag, FIELD_FLOAT_HL1 ),
	DEFINE_ENTITY_FIELD( view_ofs, FIELD_VECTOR_HL1 ),
	DEFINE_ENTITY_FIELD( button, FIELD_INTEGER_HL1 ),
	DEFINE_ENTITY_FIELD( impulse, FIELD_INTEGER_HL1 ),

	DEFINE_ENTITY_FIELD( chain, FIELD_EDICT_HL1 ),
	DEFINE_ENTITY_FIELD( dmg_inflictor, FIELD_EDICT_HL1 ),
	DEFINE_ENTITY_FIELD( enemy, FIELD_EDICT_HL1 ),
	DEFINE_ENTITY_FIELD( aiment, FIELD_EDICT_HL1 ),
	DEFINE_ENTITY_FIELD( owner, FIELD_EDICT_HL1 ),
	DEFINE_ENTITY_FIELD( groundentity, FIELD_EDICT_HL1 ),

	DEFINE_ENTITY_FIELD( spawnflags, FIELD_INTEGER_HL1 ),
	DEFINE_ENTITY_FIELD( flags, FIELD_FLOAT_HL1 ),

	DEFINE_ENTITY_FIELD( colormap, FIELD_INTEGER_HL1 ),
	DEFINE_ENTITY_FIELD( team, FIELD_INTEGER_HL1 ),

	DEFINE_ENTITY_FIELD( max_health, FIELD_FLOAT_HL1 ),
	DEFINE_ENTITY_FIELD( teleport_time, FIELD_TIME_HL1 ),
	DEFINE_ENTITY_FIELD( armortype, FIELD_FLOAT_HL1 ),
	DEFINE_ENTITY_FIELD( armorvalue, FIELD_FLOAT_HL1 ),
	DEFINE_ENTITY_FIELD( waterlevel, FIELD_INTEGER_HL1 ),
	DEFINE_ENTITY_FIELD( watertype, FIELD_INTEGER_HL1 ),

	// Having these fields be local to the individual levels makes it easier to test those levels individually.
	DEFINE_ENTITY_GLOBAL_FIELD( target, FIELD_STRING_HL1 ),
	DEFINE_ENTITY_GLOBAL_FIELD( targetname, FIELD_STRING_HL1 ),
	DEFINE_ENTITY_FIELD( netname, FIELD_STRING_HL1 ),
	DEFINE_ENTITY_FIELD( message, FIELD_STRING_HL1 ),

	DEFINE_ENTITY_FIELD( dmg_take, FIELD_FLOAT_HL1 ),
	DEFINE_ENTITY_FIELD( dmg_save, FIELD_FLOAT_HL1 ),
	DEFINE_ENTITY_FIELD( dmg, FIELD_FLOAT_HL1 ),
	DEFINE_ENTITY_FIELD( dmgtime, FIELD_TIME_HL1 ),

	DEFINE_ENTITY_FIELD( noise, FIELD_SOUNDNAME_HL1 ),
	DEFINE_ENTITY_FIELD( noise1, FIELD_SOUNDNAME_HL1 ),
	DEFINE_ENTITY_FIELD( noise2, FIELD_SOUNDNAME_HL1 ),
	DEFINE_ENTITY_FIELD( noise3, FIELD_SOUNDNAME_HL1 ),
	DEFINE_ENTITY_FIELD( speed, FIELD_FLOAT_HL1 ),
	DEFINE_ENTITY_FIELD( air_finished, FIELD_TIME_HL1 ),
	DEFINE_ENTITY_FIELD( pain_finished, FIELD_TIME_HL1 ),
	DEFINE_ENTITY_FIELD( radsuit_finished, FIELD_TIME_HL1 ),
};

#define ENTVARS_COUNT		(sizeof(gEntvarsDescription)/sizeof(gEntvarsDescription[0]))


#ifdef	DEBUG
edict_t_hl1 *DBG_EntOfVars( const entvars_t *pev )
{
	if (pev->pContainingEntity != NULL)
		return pev->pContainingEntity;
	ALERT(at_console, "entvars_t pContainingEntity is NULL, calling into engine");
	edict_t_hl1* pent = (*g_engfuncs.pfnFindEntityByVars)((entvars_t*)pev);
	if (pent == NULL)
		ALERT(at_console, "DAMN!  Even the engine couldn't FindEntityByVars!");
	((entvars_t *)pev)->pContainingEntity = pent;
	return pent;
}
#endif //DEBUG


#ifdef	DEBUG
	void
DBG_AssertFunction(
	BOOL		fExpr,
	const char*	szExpr,
	const char*	szFile,
	int			szLine,
	const char*	szMessage)
	{
	if (fExpr)
		return;
	char szOut[512];
	if (szMessage != NULL)
		sprintf(szOut, "ASSERT FAILED:\n %s \n(%s@%d)\n%s", szExpr, szFile, szLine, szMessage);
	else
		sprintf(szOut, "ASSERT FAILED:\n %s \n(%s@%d)", szExpr, szFile, szLine);
	ALERT(at_console, szOut);
	}
#endif	// DEBUG

BOOL UTIL_GetNextBestWeapon( CBasePlayerHL1 *pPlayer, CBasePlayerItemHL1 *pCurrentWeapon )
{
	return g_pGameRulesHL1->GetNextBestWeapon( pPlayer, pCurrentWeapon );
}

// ripped this out of the engine
float	UTIL_AngleMod(float a)
{
	if (a < 0)
	{
		a = a + 360 * ((int)(a / 360) + 1);
	}
	else if (a >= 360)
	{
		a = a - 360 * ((int)(a / 360));
	}
	// a = (360.0/65536) * ((int)(a*(65536/360.0)) & 65535);
	return a;
}

float UTIL_AngleDiff( float destAngle, float srcAngle )
{
	float delta;

	delta = destAngle - srcAngle;
	if ( destAngle > srcAngle )
	{
		if ( delta >= 180 )
			delta -= 360;
	}
	else
	{
		if ( delta <= -180 )
			delta += 360;
	}
	return delta;
}

VectorHL1 UTIL_VecToAngles( const VectorHL1 &vec )
{
	float rgflVecOut[3];
	VEC_TO_ANGLES(vec, rgflVecOut);
	return VectorHL1(rgflVecOut);
}
	
//	float UTIL_MoveToOrigin( edict_t_hl1 *pent, const VectorHL1 vecGoal, float flDist, int iMoveType )
void UTIL_MoveToOrigin( edict_t_hl1 *pent, const VectorHL1 &vecGoal, float flDist, int iMoveType )
{
	float rgfl[3];
	vecGoal.CopyToArray(rgfl);
//		return MOVE_TO_ORIGIN ( pent, rgfl, flDist, iMoveType ); 
	MOVE_TO_ORIGIN ( pent, rgfl, flDist, iMoveType ); 
}


int UTIL_EntitiesInBox( CBaseEntityHL1 **pList, int listMax, const VectorHL1 &mins, const VectorHL1 &maxs, int flagMask )
{
	edict_t_hl1		*pEdict = g_engfuncsHL1.pfnPEntityOfEntIndex( 1 );
	CBaseEntityHL1 *pEntity;
	int			count;

	count = 0;

	if ( !pEdict )
		return count;

	for ( int i = 1; i < gpGlobalsHL1->maxEntities; i++, pEdict++ )
	{
		if ( pEdict->free )	// Not in use
			continue;
		
		if ( flagMask && !(pEdict->v.flags & flagMask) )	// Does it meet the criteria?
			continue;

		if ( mins.x > pEdict->v.absmax.x ||
			 mins.y > pEdict->v.absmax.y ||
			 mins.z > pEdict->v.absmax.z ||
			 maxs.x < pEdict->v.absmin.x ||
			 maxs.y < pEdict->v.absmin.y ||
			 maxs.z < pEdict->v.absmin.z )
			 continue;

		pEntity = CBaseEntityHL1::Instance(pEdict);
		if ( !pEntity )
			continue;

		pList[ count ] = pEntity;
		count++;

		if ( count >= listMax )
			return count;
	}

	return count;
}


int UTIL_MonstersInSphere( CBaseEntityHL1 **pList, int listMax, const VectorHL1 &center, float radius )
{
	edict_t_hl1		*pEdict = g_engfuncsHL1.pfnPEntityOfEntIndex( 1 );
	CBaseEntityHL1 *pEntity;
	int			count;
	float		distance, delta;

	count = 0;
	float radiusSquared = radius * radius;

	if ( !pEdict )
		return count;

	for ( int i = 1; i < gpGlobalsHL1->maxEntities; i++, pEdict++ )
	{
		if ( pEdict->free )	// Not in use
			continue;
		
		if ( !(pEdict->v.flags & (FL_CLIENT|FL_MONSTER)) )	// Not a client/monster ?
			continue;

		// Use origin for X & Y since they are centered for all monsters
		// Now X
		delta = center.x - pEdict->v.origin.x;//(pEdict->v.absmin.x + pEdict->v.absmax.x)*0.5;
		delta *= delta;

		if ( delta > radiusSquared )
			continue;
		distance = delta;
		
		// Now Y
		delta = center.y - pEdict->v.origin.y;//(pEdict->v.absmin.y + pEdict->v.absmax.y)*0.5;
		delta *= delta;

		distance += delta;
		if ( distance > radiusSquared )
			continue;

		// Now Z
		delta = center.z - (pEdict->v.absmin.z + pEdict->v.absmax.z)*0.5;
		delta *= delta;

		distance += delta;
		if ( distance > radiusSquared )
			continue;

		pEntity = CBaseEntityHL1::Instance(pEdict);
		if ( !pEntity )
			continue;

		pList[ count ] = pEntity;
		count++;

		if ( count >= listMax )
			return count;
	}


	return count;
}


CBaseEntityHL1 *UTIL_FindEntityInSphere( CBaseEntityHL1 *pStartEntity, const VectorHL1 &vecCenter, float flRadius )
{
	edict_t_hl1	*pentEntity;

	if (pStartEntity)
		pentEntity = pStartEntity->edict();
	else
		pentEntity = NULL;

	pentEntity = FIND_ENTITY_IN_SPHERE( pentEntity, vecCenter, flRadius);

	if (!FNullEnt(pentEntity))
		return CBaseEntityHL1::Instance(pentEntity);
	return NULL;
}


CBaseEntityHL1 *UTIL_FindEntityByString( CBaseEntityHL1 *pStartEntity, const char *szKeyword, const char *szValue )
{
	edict_t_hl1	*pentEntity;

	if (pStartEntity)
		pentEntity = pStartEntity->edict();
	else
		pentEntity = NULL;

	pentEntity = FIND_ENTITY_BY_STRING( pentEntity, szKeyword, szValue );

	if (!FNullEnt(pentEntity))
		return CBaseEntityHL1::Instance(pentEntity);
	return NULL;
}

CBaseEntityHL1 *UTIL_FindEntityByClassname( CBaseEntityHL1 *pStartEntity, const char *szName )
{
	return UTIL_FindEntityByString( pStartEntity, "classname", szName );
}

CBaseEntityHL1 *UTIL_FindEntityByTargetname( CBaseEntityHL1 *pStartEntity, const char *szName )
{
	return UTIL_FindEntityByString( pStartEntity, "targetname", szName );
}


CBaseEntityHL1 *UTIL_FindEntityGeneric( const char *szWhatever, VectorHL1 &vecSrc, float flRadius )
{
	CBaseEntityHL1 *pEntity = NULL;

	pEntity = UTIL_FindEntityByTargetname( NULL, szWhatever );
	if (pEntity)
		return pEntity;

	CBaseEntityHL1 *pSearch = NULL;
	float flMaxDist2 = flRadius * flRadius;
	while ((pSearch = UTIL_FindEntityByClassname( pSearch, szWhatever )) != NULL)
	{
		float flDist2 = (pSearch->pev->origin - vecSrc).Length();
		flDist2 = flDist2 * flDist2;
		if (flMaxDist2 > flDist2)
		{
			pEntity = pSearch;
			flMaxDist2 = flDist2;
		}
	}
	return pEntity;
}


// returns a CBaseEntityHL1 pointer to a player by index.  Only returns if the player is spawned and connected
// otherwise returns NULL
// Index is 1 based
CBaseEntityHL1	*UTIL_PlayerByIndexHL1( int playerIndex )
{
	CBaseEntityHL1 *pPlayer = NULL;

	if ( playerIndex > 0 && playerIndex <= gpGlobalsHL1->maxClients )
	{
		edict_t_hl1 *pPlayerEdict = INDEXENTHL1( playerIndex );
		if ( pPlayerEdict && !pPlayerEdict->free )
		{
			pPlayer = CBaseEntityHL1::Instance( pPlayerEdict );
		}
	}
	
	return pPlayer;
}


void UTIL_MakeVectors( const VectorHL1 &vecAngles )
{
	MAKE_VECTORS( vecAngles );
}


void UTIL_MakeAimVectors( const VectorHL1 &vecAngles )
{
	float rgflVec[3];
	vecAngles.CopyToArray(rgflVec);
	rgflVec[0] = -rgflVec[0];
	MAKE_VECTORS(rgflVec);
}


#define SWAP(a,b,temp)	((temp)=(a),(a)=(b),(b)=(temp))

void UTIL_MakeInvVectors( const VectorHL1 &vec, globalvars_t *pgv )
{
	MAKE_VECTORS(vec);

	float tmp;
	pgv->v_right = pgv->v_right * -1;

	SWAP(pgv->v_forward.y, pgv->v_right.x, tmp);
	SWAP(pgv->v_forward.z, pgv->v_up.x, tmp);
	SWAP(pgv->v_right.z, pgv->v_up.y, tmp);
}


void UTIL_EmitAmbientSound( edict_t_hl1 *entity, const VectorHL1 &vecOrigin, const char *samp, float vol, float attenuation, int fFlags, int pitch )
{
	float rgfl[3];
	vecOrigin.CopyToArray(rgfl);

	if (samp && *samp == '!')
	{
		char name[32];
		if (SENTENCEG_Lookup(samp, name) >= 0)
			EMIT_AMBIENT_SOUND(entity, rgfl, name, vol, attenuation, fFlags, pitch);
	}
	else
		EMIT_AMBIENT_SOUND(entity, rgfl, samp, vol, attenuation, fFlags, pitch);
}

static unsigned short FixedUnsigned16( float value, float scale )
{
	int output;

	output = value * scale;
	if ( output < 0 )
		output = 0;
	if ( output > 0xFFFF )
		output = 0xFFFF;

	return (unsigned short)output;
}

static short FixedSigned16( float value, float scale )
{
	int output;

	output = value * scale;

	if ( output > 32767 )
		output = 32767;

	if ( output < -32768 )
		output = -32768;

	return (short)output;
}

// Shake the screen of all clients within radius
// radius == 0, shake all clients
// UNDONE: Allow caller to shake clients not ONGROUND?
// UNDONE: Fix falloff model (disabled)?
// UNDONE: Affect user controls?
void UTIL_ScreenShake( const VectorHL1 &center, float amplitude, float frequency, float duration, float radius )
{
	int			i;
	float		localAmplitude;
	ScreenShake	shake;

	shake.duration = FixedUnsigned16( duration, 1<<12 );		// 4.12 fixed
	shake.frequency = FixedUnsigned16( frequency, 1<<8 );	// 8.8 fixed

	for ( i = 1; i <= gpGlobalsHL1->maxClients; i++ )
	{
		CBaseEntityHL1 *pPlayer = UTIL_PlayerByIndexHL1( i );

		if ( !pPlayer || !(pPlayer->pev->flags & FL_ONGROUND) )	// Don't shake if not onground
			continue;

		localAmplitude = 0;

		if ( radius <= 0 )
			localAmplitude = amplitude;
		else
		{
			VectorHL1 delta = center - pPlayer->pev->origin;
			float distance = delta.Length();
	
			// Had to get rid of this falloff - it didn't work well
			if ( distance < radius )
				localAmplitude = amplitude;//radius - distance;
		}
		if ( localAmplitude )
		{
			shake.amplitude = FixedUnsigned16( localAmplitude, 1<<12 );		// 4.12 fixed
			
			MESSAGE_BEGIN( MSG_ONE, gmsgShake, NULL, pPlayer->edict() );		// use the magic #1 for "one client"
				
				WRITE_SHORT( shake.amplitude );				// shake amount
				WRITE_SHORT( shake.duration );				// shake lasts this long
				WRITE_SHORT( shake.frequency );				// shake noise frequency

			MESSAGE_END();
		}
	}
}



void UTIL_ScreenShakeAll( const VectorHL1 &center, float amplitude, float frequency, float duration )
{
	UTIL_ScreenShake( center, amplitude, frequency, duration, 0 );
}


void UTIL_ScreenFadeBuild( ScreenFade &fade, const VectorHL1 &color, float fadeTime, float fadeHold, int alpha, int flags )
{
	fade.duration = FixedUnsigned16( fadeTime, 1<<12 );		// 4.12 fixed
	fade.holdTime = FixedUnsigned16( fadeHold, 1<<12 );		// 4.12 fixed
	fade.r = (int)color.x;
	fade.g = (int)color.y;
	fade.b = (int)color.z;
	fade.a = alpha;
	fade.fadeFlags = flags;
}


void UTIL_ScreenFadeWrite( const ScreenFade &fade, CBaseEntityHL1 *pEntity )
{
	if ( !pEntity || !pEntity->IsNetClient() )
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsgFade, NULL, pEntity->edict() );		// use the magic #1 for "one client"
		
		WRITE_SHORT( fade.duration );		// fade lasts this long
		WRITE_SHORT( fade.holdTime );		// fade lasts this long
		WRITE_SHORT( fade.fadeFlags );		// fade type (in / out)
		WRITE_BYTE( fade.r );				// fade red
		WRITE_BYTE( fade.g );				// fade green
		WRITE_BYTE( fade.b );				// fade blue
		WRITE_BYTE( fade.a );				// fade blue

	MESSAGE_END();
}


void UTIL_ScreenFadeAll( const VectorHL1 &color, float fadeTime, float fadeHold, int alpha, int flags )
{
	int			i;
	ScreenFade	fade;


	UTIL_ScreenFadeBuild( fade, color, fadeTime, fadeHold, alpha, flags );

	for ( i = 1; i <= gpGlobalsHL1->maxClients; i++ )
	{
		CBaseEntityHL1 *pPlayer = UTIL_PlayerByIndexHL1( i );
	
		UTIL_ScreenFadeWrite( fade, pPlayer );
	}
}


void UTIL_ScreenFade( CBaseEntityHL1 *pEntity, const VectorHL1 &color, float fadeTime, float fadeHold, int alpha, int flags )
{
	ScreenFade	fade;

	UTIL_ScreenFadeBuild( fade, color, fadeTime, fadeHold, alpha, flags );
	UTIL_ScreenFadeWrite( fade, pEntity );
}


void UTIL_HudMessageHL1( CBaseEntityHL1 *pEntity, const hudtextparms_t &textparms, const char *pMessage )
{
	if ( !pEntity || !pEntity->IsNetClient() )
		return;

	MESSAGE_BEGIN( MSG_ONE, SVC_TEMPENTITY, NULL, pEntity->edict() );
		WRITE_BYTE( TE_TEXTMESSAGE );
		WRITE_BYTE( textparms.channel & 0xFF );

		WRITE_SHORT( FixedSigned16( textparms.x, 1<<13 ) );
		WRITE_SHORT( FixedSigned16( textparms.y, 1<<13 ) );
		WRITE_BYTE( textparms.effect );

		WRITE_BYTE( textparms.r1 );
		WRITE_BYTE( textparms.g1 );
		WRITE_BYTE( textparms.b1 );
		WRITE_BYTE( textparms.a1 );

		WRITE_BYTE( textparms.r2 );
		WRITE_BYTE( textparms.g2 );
		WRITE_BYTE( textparms.b2 );
		WRITE_BYTE( textparms.a2 );

		WRITE_SHORT( FixedUnsigned16( textparms.fadeinTime, 1<<8 ) );
		WRITE_SHORT( FixedUnsigned16( textparms.fadeoutTime, 1<<8 ) );
		WRITE_SHORT( FixedUnsigned16( textparms.holdTime, 1<<8 ) );

		if ( textparms.effect == 2 )
			WRITE_SHORT( FixedUnsigned16( textparms.fxTime, 1<<8 ) );
		
		if ( strlen( pMessage ) < 512 )
		{
			WRITE_STRING( pMessage );
		}
		else
		{
			char tmp[512];
			strncpy( tmp, pMessage, 511 );
			tmp[511] = 0;
			WRITE_STRING( tmp );
		}
	MESSAGE_END();
}

void UTIL_HudMessageAllHL1( const hudtextparms_t &textparms, const char *pMessage )
{
	int			i;

	for ( i = 1; i <= gpGlobalsHL1->maxClients; i++ )
	{
		CBaseEntityHL1 *pPlayer = UTIL_PlayerByIndexHL1( i );
		if ( pPlayer )
			UTIL_HudMessageHL1( pPlayer, textparms, pMessage );
	}
}

					 
extern int gmsgTextMsg, gmsgSayText;
void UTIL_ClientPrintAllHL1( int msg_dest, const char *msg_name, const char *param1, const char *param2, const char *param3, const char *param4 )
{
	MESSAGE_BEGIN( MSG_ALL, gmsgTextMsg );
		WRITE_BYTE( msg_dest );
		WRITE_STRING( msg_name );

		if ( param1 )
			WRITE_STRING( param1 );
		if ( param2 )
			WRITE_STRING( param2 );
		if ( param3 )
			WRITE_STRING( param3 );
		if ( param4 )
			WRITE_STRING( param4 );

	MESSAGE_END();
}

void ClientPrintHL1( entvars_t *client, int msg_dest, const char *msg_name, const char *param1, const char *param2, const char *param3, const char *param4 )
{
	MESSAGE_BEGIN_HL1( MSG_ONE, gmsgTextMsg, NULL, client );
		WRITE_BYTE( msg_dest );
		WRITE_STRING( msg_name );

		if ( param1 )
			WRITE_STRING( param1 );
		if ( param2 )
			WRITE_STRING( param2 );
		if ( param3 )
			WRITE_STRING( param3 );
		if ( param4 )
			WRITE_STRING( param4 );

	MESSAGE_END();
}

void UTIL_SayText( const char *pText, CBaseEntityHL1 *pEntity )
{
	if ( !pEntity->IsNetClient() )
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsgSayText, NULL, pEntity->edict() );
		WRITE_BYTE( pEntity->entindex() );
		WRITE_STRING( pText );
	MESSAGE_END();
}

void UTIL_SayTextAll( const char *pText, CBaseEntityHL1 *pEntity )
{
	MESSAGE_BEGIN( MSG_ALL, gmsgSayText, NULL );
		WRITE_BYTE( pEntity->entindex() );
		WRITE_STRING( pText );
	MESSAGE_END();
}


char *UTIL_dtos1( int d )
{
	static char buf[8];
	sprintf( buf, "%d", d );
	return buf;
}

char *UTIL_dtos2( int d )
{
	static char buf[8];
	sprintf( buf, "%d", d );
	return buf;
}

char *UTIL_dtos3( int d )
{
	static char buf[8];
	sprintf( buf, "%d", d );
	return buf;
}

char *UTIL_dtos4( int d )
{
	static char buf[8];
	sprintf( buf, "%d", d );
	return buf;
}

void UTIL_ShowMessageHL1( const char *pString, CBaseEntityHL1 *pEntity )
{
	if ( !pEntity || !pEntity->IsNetClient() )
		return;

	MESSAGE_BEGIN( MSG_ONE, gmsgHudText, NULL, pEntity->edict() );
	WRITE_STRING( pString );
	MESSAGE_END();
}


void UTIL_ShowMessageAllHL1( const char *pString )
{
	int		i;

	// loop through all players

	for ( i = 1; i <= gpGlobalsHL1->maxClients; i++ )
	{
		CBaseEntityHL1 *pPlayer = UTIL_PlayerByIndexHL1( i );
		if ( pPlayer )
			UTIL_ShowMessageHL1( pString, pPlayer );
	}
}

// Overloaded to add IGNORE_GLASS
void UTIL_TraceLine( const VectorHL1 &vecStart, const VectorHL1 &vecEnd, IGNORE_MONSTERS igmon, IGNORE_GLASS ignoreGlass, edict_t_hl1 *pentIgnore, TraceResult *ptr )
{
	TRACE_LINE( vecStart, vecEnd, (igmon == ignore_monsters ? TRUE : FALSE) | (ignoreGlass?0x100:0), pentIgnore, ptr );
}


void UTIL_TraceLine( const VectorHL1 &vecStart, const VectorHL1 &vecEnd, IGNORE_MONSTERS igmon, edict_t_hl1 *pentIgnore, TraceResult *ptr )
{
	TRACE_LINE( vecStart, vecEnd, (igmon == ignore_monsters ? TRUE : FALSE), pentIgnore, ptr );
}


void UTIL_TraceHull( const VectorHL1 &vecStart, const VectorHL1 &vecEnd, IGNORE_MONSTERS igmon, int hullNumber, edict_t_hl1 *pentIgnore, TraceResult *ptr )
{
	TRACE_HULL( vecStart, vecEnd, (igmon == ignore_monsters ? TRUE : FALSE), hullNumber, pentIgnore, ptr );
}

void UTIL_TraceModel( const VectorHL1 &vecStart, const VectorHL1 &vecEnd, int hullNumber, edict_t_hl1 *pentModel, TraceResult *ptr )
{
	g_engfuncsHL1.pfnTraceModel( vecStart, vecEnd, hullNumber, pentModel, ptr );
}


TraceResult UTIL_GetGlobalTrace( )
{
	TraceResult tr;

	tr.fAllSolid		= gpGlobalsHL1->trace_allsolid;
	tr.fStartSolid		= gpGlobalsHL1->trace_startsolid;
	tr.fInOpen			= gpGlobalsHL1->trace_inopen;
	tr.fInWater			= gpGlobalsHL1->trace_inwater;
	tr.flFraction		= gpGlobalsHL1->trace_fraction;
	tr.flPlaneDist		= gpGlobalsHL1->trace_plane_dist;
	tr.pHit			= gpGlobalsHL1->trace_ent;
	tr.vecEndPos		= gpGlobalsHL1->trace_endpos;
	tr.vecPlaneNormal	= gpGlobalsHL1->trace_plane_normal;
	tr.iHitgroup		= gpGlobalsHL1->trace_hitgroup;
	return tr;
}

	
void UTIL_SetSizeHL1( entvars_t *pev, const VectorHL1 &vecMin, const VectorHL1 &vecMax )
{
	SET_SIZE( ENT(pev), vecMin, vecMax );
}
	
	
float UTIL_VecToYaw( const VectorHL1 &vec )
{
	return VEC_TO_YAW(vec);
}


void UTIL_SetOrigin( entvars_t *pev, const VectorHL1 &vecOrigin )
{
	SET_ORIGIN(ENT(pev), vecOrigin );
}

void UTIL_ParticleEffect( const VectorHL1 &vecOrigin, const VectorHL1 &vecDirection, ULONG ulColor, ULONG ulCount )
{
	PARTICLE_EFFECT( vecOrigin, vecDirection, (float)ulColor, (float)ulCount );
}


float UTIL_Approach( float target, float value, float speed )
{
	float delta = target - value;

	if ( delta > speed )
		value += speed;
	else if ( delta < -speed )
		value -= speed;
	else 
		value = target;

	return value;
}


float UTIL_ApproachAngle( float target, float value, float speed )
{
	target = UTIL_AngleMod( target );
	value = UTIL_AngleMod( target );
	
	float delta = target - value;

	// Speed is assumed to be positive
	if ( speed < 0 )
		speed = -speed;

	if ( delta < -180 )
		delta += 360;
	else if ( delta > 180 )
		delta -= 360;

	if ( delta > speed )
		value += speed;
	else if ( delta < -speed )
		value -= speed;
	else 
		value = target;

	return value;
}


float UTIL_AngleDistance( float next, float cur )
{
	float delta = next - cur;

	if ( delta < -180 )
		delta += 360;
	else if ( delta > 180 )
		delta -= 360;

	return delta;
}


float UTIL_SplineFraction( float value, float scale )
{
	value = scale * value;
	float valueSquared = value * value;

	// Nice little ease-in, ease-out spline-like curve
	return 3 * valueSquared - 2 * valueSquared * value;
}


char* UTIL_VarArgs( char *format, ... )
{
	va_list		argptr;
	static char		string[1024];
	
	va_start (argptr, format);
	vsprintf (string, format,argptr);
	va_end (argptr);

	return string;	
}
	
VectorHL1 UTIL_GetAimVector( edict_t_hl1 *pent, float flSpeed )
{
	VectorHL1 tmp;
	GET_AIM_VECTOR(pent, flSpeed, tmp);
	return tmp;
}

int UTIL_IsMasterTriggered(string_t sMaster, CBaseEntityHL1 *pActivator)
{
	if (sMaster)
	{
		edict_t_hl1 *pentTarget = FIND_ENTITY_BY_TARGETNAME(NULL, STRING_HL1(sMaster));
	
		if ( !FNullEnt(pentTarget) )
		{
			CBaseEntityHL1 *pMaster = CBaseEntityHL1::Instance(pentTarget);
			if ( pMaster && (pMaster->ObjectCaps() & FCAP_MASTER) )
				return pMaster->IsTriggered( pActivator );
		}

		ALERT(at_console, "Master was null or not a master!\n");
	}

	// if this isn't a master entity, just say yes.
	return 1;
}

BOOL UTIL_ShouldShowBloodHL1( int color )
{
	if ( color != DONT_BLEED )
	{
		if ( color == BLOOD_COLOR_RED )
		{
			if ( CVAR_GET_FLOAT("violence_hblood") != 0 )
				return TRUE;
		}
		else
		{
			if ( CVAR_GET_FLOAT("violence_ablood") != 0 )
				return TRUE;
		}
	}
	return FALSE;
}

int UTIL_PointContents(	const VectorHL1 &vec )
{
	return POINT_CONTENTS(vec);
}

void UTIL_BloodStreamHL1( const VectorHL1 &origin, const VectorHL1 &direction, int color, int amount )
{
	if ( !UTIL_ShouldShowBloodHL1( color ) )
		return;

	if ( g_LanguageHL1 == LANGUAGE_GERMAN && color == BLOOD_COLOR_RED )
		color = 0;

	
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, origin );
		WRITE_BYTE( TE_BLOODSTREAM );
		WRITE_COORD( origin.x );
		WRITE_COORD( origin.y );
		WRITE_COORD( origin.z );
		WRITE_COORD( direction.x );
		WRITE_COORD( direction.y );
		WRITE_COORD( direction.z );
		WRITE_BYTE( color );
		WRITE_BYTE( min( amount, 255 ) );
	MESSAGE_END();
}				

void UTIL_BloodDripsHL1( const VectorHL1 &origin, const VectorHL1 &direction, int color, int amount )
{
	if ( !UTIL_ShouldShowBloodHL1( color ) )
		return;

	if ( color == DONT_BLEED || amount == 0 )
		return;

	if ( g_LanguageHL1 == LANGUAGE_GERMAN && color == BLOOD_COLOR_RED )
		color = 0;

	if ( g_pGameRulesHL1->IsMultiplayer() )
	{
		// scale up blood effect in multiplayer for better visibility
		amount *= 2;
	}

	if ( amount > 255 )
		amount = 255;

	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, origin );
		WRITE_BYTE( TE_BLOODSPRITE );
		WRITE_COORD( origin.x);								// pos
		WRITE_COORD( origin.y);
		WRITE_COORD( origin.z);
		WRITE_SHORT( g_sModelIndexBloodSpray );				// initial sprite model
		WRITE_SHORT( g_sModelIndexBloodDrop );				// droplet sprite models
		WRITE_BYTE( color );								// color index into host_basepal
		WRITE_BYTE( min( max( 3, amount / 10 ), 16 ) );		// size
	MESSAGE_END();
}				

VectorHL1 UTIL_RandomBloodVectorHL1( void )
{
	VectorHL1 direction;

	direction.x = RANDOM_FLOAT ( -1, 1 );
	direction.y = RANDOM_FLOAT ( -1, 1 );
	direction.z = RANDOM_FLOAT ( 0, 1 );

	return direction;
}


void UTIL_BloodDecalTrace( TraceResult *pTrace, int bloodColor )
{
	if ( UTIL_ShouldShowBlood( bloodColor ) )
	{
		if ( bloodColor == BLOOD_COLOR_RED )
			UTIL_DecalTrace( pTrace, DECAL_BLOOD1 + RANDOM_LONG(0,5) );
		else
			UTIL_DecalTrace( pTrace, DECAL_YBLOOD1 + RANDOM_LONG(0,5) );
	}
}


void UTIL_DecalTrace( TraceResult *pTrace, int decalNumber )
{
	short entityIndex;
	int index;
	int message;

	if ( decalNumber < 0 )
		return;

	index = gDecals[ decalNumber ].index;

	if ( index < 0 )
		return;

	if (pTrace->flFraction == 1.0)
		return;

	// Only decal BSP models
	if ( pTrace->pHit )
	{
		CBaseEntityHL1 *pEntity = CBaseEntityHL1::Instance( pTrace->pHit );
		if ( pEntity && !pEntity->IsBSPModel() )
			return;
		entityIndex = ENTINDEXHL1( pTrace->pHit );
	}
	else 
		entityIndex = 0;

	message = TE_DECAL;
	if ( entityIndex != 0 )
	{
		if ( index > 255 )
		{
			message = TE_DECALHIGH;
			index -= 256;
		}
	}
	else
	{
		message = TE_WORLDDECAL;
		if ( index > 255 )
		{
			message = TE_WORLDDECALHIGH;
			index -= 256;
		}
	}
	
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( message );
		WRITE_COORD( pTrace->vecEndPos.x );
		WRITE_COORD( pTrace->vecEndPos.y );
		WRITE_COORD( pTrace->vecEndPos.z );
		WRITE_BYTE( index );
		if ( entityIndex )
			WRITE_SHORT( entityIndex );
	MESSAGE_END();
}

/*
==============
UTIL_PlayerDecalTrace

A player is trying to apply his custom decal for the spray can.
Tell connected clients to display it, or use the default spray can decal
if the custom can't be loaded.
==============
*/
void UTIL_PlayerDecalTrace( TraceResult *pTrace, int playernum, int decalNumber, BOOL bIsCustom )
{
	int index;
	
	if (!bIsCustom)
	{
		if ( decalNumber < 0 )
			return;

		index = gDecals[ decalNumber ].index;
		if ( index < 0 )
			return;
	}
	else
		index = decalNumber;

	if (pTrace->flFraction == 1.0)
		return;

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_PLAYERDECAL );
		WRITE_BYTE ( playernum );
		WRITE_COORD( pTrace->vecEndPos.x );
		WRITE_COORD( pTrace->vecEndPos.y );
		WRITE_COORD( pTrace->vecEndPos.z );
		WRITE_SHORT( (short)ENTINDEXHL1(pTrace->pHit) );
		WRITE_BYTE( index );
	MESSAGE_END();
}

void UTIL_GunshotDecalTrace( TraceResult *pTrace, int decalNumber )
{
	if ( decalNumber < 0 )
		return;

	int index = gDecals[ decalNumber ].index;
	if ( index < 0 )
		return;

	if (pTrace->flFraction == 1.0)
		return;

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, pTrace->vecEndPos );
		WRITE_BYTE( TE_GUNSHOTDECAL );
		WRITE_COORD( pTrace->vecEndPos.x );
		WRITE_COORD( pTrace->vecEndPos.y );
		WRITE_COORD( pTrace->vecEndPos.z );
		WRITE_SHORT( (short)ENTINDEXHL1(pTrace->pHit) );
		WRITE_BYTE( index );
	MESSAGE_END();
}


void UTIL_Sparks( const VectorHL1 &position )
{
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, position );
		WRITE_BYTE( TE_SPARKS );
		WRITE_COORD( position.x );
		WRITE_COORD( position.y );
		WRITE_COORD( position.z );
	MESSAGE_END();
}


void UTIL_Ricochet( const VectorHL1 &position, float scale )
{
	MESSAGE_BEGIN( MSG_PVS, SVC_TEMPENTITY, position );
		WRITE_BYTE( TE_ARMOR_RICOCHET );
		WRITE_COORD( position.x );
		WRITE_COORD( position.y );
		WRITE_COORD( position.z );
		WRITE_BYTE( (int)(scale*10) );
	MESSAGE_END();
}


BOOL UTIL_TeamsMatchHL1( const char *pTeamName1, const char *pTeamName2 )
{
	// Everyone matches unless it's teamplay
	if ( !g_pGameRules->IsTeamplay() )
		return TRUE;

	// Both on a team?
	if ( *pTeamName1 != 0 && *pTeamName2 != 0 )
	{
		if ( !stricmp( pTeamName1, pTeamName2 ) )	// Same Team?
			return TRUE;
	}

	return FALSE;
}


void UTIL_StringToVector( float *pVector, const char *pString )
{
	char *pstr, *pfront, tempString[128];
	int	j;

	strcpy( tempString, pString );
	pstr = pfront = tempString;

	for ( j = 0; j < 3; j++ )			// lifted from pr_edict.c
	{
		pVector[j] = atof( pfront );

		while ( *pstr && *pstr != ' ' )
			pstr++;
		if (!*pstr)
			break;
		pstr++;
		pfront = pstr;
	}
	if (j < 2)
	{
		/*
		ALERT( at_error, "Bad field in entity!! %s:%s == \"%s\"\n",
			pkvd->szClassName, pkvd->szKeyName, pkvd->szValue );
		*/
		for (j = j+1;j < 3; j++)
			pVector[j] = 0;
	}
}


void UTIL_StringToIntArray( int *pVector, int count, const char *pString )
{
	char *pstr, *pfront, tempString[128];
	int	j;

	strcpy( tempString, pString );
	pstr = pfront = tempString;

	for ( j = 0; j < count; j++ )			// lifted from pr_edict.c
	{
		pVector[j] = atoi( pfront );

		while ( *pstr && *pstr != ' ' )
			pstr++;
		if (!*pstr)
			break;
		pstr++;
		pfront = pstr;
	}

	for ( j++; j < count; j++ )
	{
		pVector[j] = 0;
	}
}

VectorHL1 UTIL_ClampVectorToBox( const VectorHL1 &input, const VectorHL1 &clampSize )
{
	VectorHL1 sourceVector = input;

	if ( sourceVector.x > clampSize.x )
		sourceVector.x -= clampSize.x;
	else if ( sourceVector.x < -clampSize.x )
		sourceVector.x += clampSize.x;
	else
		sourceVector.x = 0;

	if ( sourceVector.y > clampSize.y )
		sourceVector.y -= clampSize.y;
	else if ( sourceVector.y < -clampSize.y )
		sourceVector.y += clampSize.y;
	else
		sourceVector.y = 0;
	
	if ( sourceVector.z > clampSize.z )
		sourceVector.z -= clampSize.z;
	else if ( sourceVector.z < -clampSize.z )
		sourceVector.z += clampSize.z;
	else
		sourceVector.z = 0;

	return sourceVector.Normalize();
}


float UTIL_WaterLevel( const VectorHL1 &position, float minz, float maxz )
{
	VectorHL1 midUp = position;
	midUp.z = minz;

	if (UTIL_PointContents(midUp) != CONTENTS_WATER)
		return minz;

	midUp.z = maxz;
	if (UTIL_PointContents(midUp) == CONTENTS_WATER)
		return maxz;

	float diff = maxz - minz;
	while (diff > 1.0)
	{
		midUp.z = minz + diff/2.0;
		if (UTIL_PointContents(midUp) == CONTENTS_WATER)
		{
			minz = midUp.z;
		}
		else
		{
			maxz = midUp.z;
		}
		diff = maxz - minz;
	}

	return midUp.z;
}


extern DLL_GLOBAL	short	g_sModelIndexBubbles;// holds the index for the bubbles model

void UTIL_BubblesHL1( VectorHL1 mins, VectorHL1 maxs, int count )
{
	VectorHL1 mid =  (mins + maxs) * 0.5;

	float flHeight = UTIL_WaterLevel( mid,  mid.z, mid.z + 1024 );
	flHeight = flHeight - mins.z;

	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, mid );
		WRITE_BYTE( TE_BUBBLES );
		WRITE_COORD( mins.x );	// mins
		WRITE_COORD( mins.y );
		WRITE_COORD( mins.z );
		WRITE_COORD( maxs.x );	// maxz
		WRITE_COORD( maxs.y );
		WRITE_COORD( maxs.z );
		WRITE_COORD( flHeight );			// height
		WRITE_SHORT( g_sModelIndexBubbles );
		WRITE_BYTE( count ); // count
		WRITE_COORD( 8 ); // speed
	MESSAGE_END();
}

void UTIL_BubbleTrailHL1( VectorHL1 from, VectorHL1 to, int count )
{
	float flHeight = UTIL_WaterLevel( from,  from.z, from.z + 256 );
	flHeight = flHeight - from.z;

	if (flHeight < 8)
	{
		flHeight = UTIL_WaterLevel( to,  to.z, to.z + 256 );
		flHeight = flHeight - to.z;
		if (flHeight < 8)
			return;

		// UNDONE: do a ploink sound
		flHeight = flHeight + to.z - from.z;
	}

	if (count > 255) 
		count = 255;

	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BUBBLETRAIL );
		WRITE_COORD( from.x );	// mins
		WRITE_COORD( from.y );
		WRITE_COORD( from.z );
		WRITE_COORD( to.x );	// maxz
		WRITE_COORD( to.y );
		WRITE_COORD( to.z );
		WRITE_COORD( flHeight );			// height
		WRITE_SHORT( g_sModelIndexBubbles );
		WRITE_BYTE( count ); // count
		WRITE_COORD( 8 ); // speed
	MESSAGE_END();
}


void UTIL_Remove( CBaseEntityHL1 *pEntity )
{
	if ( !pEntity )
		return;

	pEntity->UpdateOnRemove();
	pEntity->pev->flags |= FL_KILLME;
	pEntity->pev->targetname = 0;
}


BOOL UTIL_IsValidEntity( edict_t_hl1 *pent )
{
	if ( !pent || pent->free || (pent->v.flags & FL_KILLME) )
		return FALSE;
	return TRUE;
}


void UTIL_PrecacheOtherHL1( const char *szClassname )
{
	edict_t_hl1	*pent;

	pent = CREATE_NAMED_ENTITY( MAKE_STRING_HL1( szClassname ) );
	if ( FNullEnt( pent ) )
	{
		ALERT ( at_console, "NULL Ent in UTIL_PrecacheOther\n" );
		return;
	}
	
	CBaseEntityHL1 *pEntity = CBaseEntityHL1::Instance (VARS( pent ));
	if (pEntity)
		pEntity->Precache( );
	REMOVE_ENTITY(pent);
}

//=========================================================
// UTIL_LogPrintf - Prints a logged message to console.
// Preceded by LOG: ( timestamp ) < message >
//=========================================================
void UTIL_LogPrintf( char *fmt, ... )
{
	va_list			argptr;
	static char		string[1024];
	
	va_start ( argptr, fmt );
	vsprintf ( string, fmt, argptr );
	va_end   ( argptr );

	// Print to server console
	ALERT( at_logged, "%s", string );
}

//=========================================================
// UTIL_DotPoints - returns the dot product of a line from
// src to check and vecdir.
//=========================================================
float UTIL_DotPointsHL1 ( const VectorHL1 &vecSrc, const VectorHL1 &vecCheck, const VectorHL1 &vecDir )
{
	Vector2DHL1	vec2LOS;

	vec2LOS = ( vecCheck - vecSrc ).Make2D();
	vec2LOS = vec2LOS.Normalize();

	return DotProduct (vec2LOS , ( vecDir.Make2D() ) );
}


//=========================================================
// UTIL_StripToken - for redundant keynames
//=========================================================
void UTIL_StripToken( const char *pKey, char *pDest )
{
	int i = 0;

	while ( pKey[i] && pKey[i] != '#' )
	{
		pDest[i] = pKey[i];
		i++;
	}
	pDest[i] = 0;
}


// --------------------------------------------------------------
//
// CSave
//
// --------------------------------------------------------------
static int gSizes[FIELD_TYPECOUNT] = 
{
	sizeof(float),		// FIELD_FLOAT
	sizeof(int),		// FIELD_STRING
	sizeof(int),		// FIELD_ENTITY
	sizeof(int),		// FIELD_CLASSPTR
	sizeof(int),		// FIELD_EHANDLE
	sizeof(int),		// FIELD_entvars_t
	sizeof(int),		// FIELD_EDICT
	sizeof(float)*3,	// FIELD_VECTOR
	sizeof(float)*3,	// FIELD_POSITION_VECTOR
	sizeof(int *),		// FIELD_POINTER
	sizeof(int),		// FIELD_INTEGER
	sizeof(int *),		// FIELD_FUNCTION
	sizeof(int),		// FIELD_BOOLEAN
	sizeof(short),		// FIELD_SHORT
	sizeof(char),		// FIELD_CHARACTER
	sizeof(float),		// FIELD_TIME
	sizeof(int),		// FIELD_MODELNAME
	sizeof(int),		// FIELD_SOUNDNAME
};


// Base class includes common SAVERESTOREDATA pointer, and manages the entity table
CSaveRestoreBuffer :: CSaveRestoreBuffer( void )
{
	m_pdata = NULL;
}


CSaveRestoreBuffer :: CSaveRestoreBuffer( SAVERESTOREDATA *pdata )
{
	m_pdata = pdata;
}


CSaveRestoreBuffer :: ~CSaveRestoreBuffer( void )
{
}

int	CSaveRestoreBuffer :: EntityIndex( CBaseEntityHL1 *pEntity )
{
	if ( pEntity == NULL )
		return -1;
	return EntityIndex( pEntity->pev );
}


int	CSaveRestoreBuffer :: EntityIndex( entvars_t *pevLookup )
{
	if ( pevLookup == NULL )
		return -1;
	return EntityIndex( ENT( pevLookup ) );
}

int	CSaveRestoreBuffer :: EntityIndex( EOFFSET eoLookup )
{
	return EntityIndex( ENT( eoLookup ) );
}


int	CSaveRestoreBuffer :: EntityIndex( edict_t_hl1 *pentLookup )
{
	if ( !m_pdata || pentLookup == NULL )
		return -1;

	int i;
	ENTITYTABLE *pTable;

	for ( i = 0; i < m_pdata->tableCount; i++ )
	{
		pTable = m_pdata->pTable + i;
		if ( pTable->pent == pentLookup )
			return i;
	}
	return -1;
}


edict_t_hl1 *CSaveRestoreBuffer :: EntityFromIndex( int entityIndex )
{
	if ( !m_pdata || entityIndex < 0 )
		return NULL;

	int i;
	ENTITYTABLE *pTable;

	for ( i = 0; i < m_pdata->tableCount; i++ )
	{
		pTable = m_pdata->pTable + i;
		if ( pTable->id == entityIndex )
			return pTable->pent;
	}
	return NULL;
}


int	CSaveRestoreBuffer :: EntityFlagsSet( int entityIndex, int flags )
{
	if ( !m_pdata || entityIndex < 0 )
		return 0;
	if ( entityIndex > m_pdata->tableCount )
		return 0;

	m_pdata->pTable[ entityIndex ].flags |= flags;

	return m_pdata->pTable[ entityIndex ].flags;
}


void CSaveRestoreBuffer :: BufferRewind( int size )
{
	if ( !m_pdata )
		return;

	if ( m_pdata->size < size )
		size = m_pdata->size;

	m_pdata->pCurrentData -= size;
	m_pdata->size -= size;
}

#ifndef _WIN32
extern "C" {
unsigned _rotr ( unsigned val, int shift)
{
        register unsigned lobit;        /* non-zero means lo bit set */
        register unsigned num = val;    /* number to rotate */

        shift &= 0x1f;                  /* modulo 32 -- this will also make
                                           negative shifts work */

        while (shift--) {
                lobit = num & 1;        /* get high bit */
                num >>= 1;              /* shift right one bit */
                if (lobit)
                        num |= 0x80000000;  /* set hi bit if lo bit was set */
        }

        return num;
}
}
#endif

unsigned int CSaveRestoreBuffer :: HashString( const char *pszToken )
{
	unsigned int	hash = 0;

	while ( *pszToken )
		hash = _rotr( hash, 4 ) ^ *pszToken++;

	return hash;
}

unsigned short CSaveRestoreBuffer :: TokenHash( const char *pszToken )
{
	unsigned short	hash = (unsigned short)(HashString( pszToken ) % (unsigned)m_pdata->tokenCount );
	
#if _DEBUG
	static int tokensparsed = 0;
	tokensparsed++;
	if ( !m_pdata->tokenCount || !m_pdata->pTokens )
		ALERT( at_error, "No token table array in TokenHash()!" );
#endif

	for ( int i=0; i<m_pdata->tokenCount; i++ )
	{
#if _DEBUG
		static qboolean beentheredonethat = FALSE;
		if ( i > 50 && !beentheredonethat )
		{
			beentheredonethat = TRUE;
			ALERT( at_error, "CSaveRestoreBuffer :: TokenHash() is getting too full!" );
		}
#endif

		int	index = hash + i;
		if ( index >= m_pdata->tokenCount )
			index -= m_pdata->tokenCount;

		if ( !m_pdata->pTokens[index] || strcmp( pszToken, m_pdata->pTokens[index] ) == 0 )
		{
			m_pdata->pTokens[index] = (char *)pszToken;
			return index;
		}
	}
		
	// Token hash table full!!! 
	// [Consider doing overflow table(s) after the main table & limiting linear hash table search]
	ALERT( at_error, "CSaveRestoreBuffer :: TokenHash() is COMPLETELY FULL!" );
	return 0;
}

void CSaveHL1 :: WriteData( const char *pname, int size, const char *pdata )
{
	BufferField( pname, size, pdata );
}


void CSaveHL1 :: WriteShort( const char *pname, const short *data, int count )
{
	BufferField( pname, sizeof(short) * count, (const char *)data );
}


void CSaveHL1 :: WriteInt( const char *pname, const int *data, int count )
{
	BufferField( pname, sizeof(int) * count, (const char *)data );
}


void CSaveHL1 :: WriteFloat( const char *pname, const float *data, int count )
{
	BufferField( pname, sizeof(float) * count, (const char *)data );
}


void CSaveHL1 :: WriteTime( const char *pname, const float *data, int count )
{
	int i;
	VectorHL1 tmp, input;

	BufferHeader( pname, sizeof(float) * count );
	for ( i = 0; i < count; i++ )
	{
		float tmp = data[0];

		// Always encode time as a delta from the current time so it can be re-based if loaded in a new level
		// Times of 0 are never written to the file, so they will be restored as 0, not a relative time
		if ( m_pdata )
			tmp -= m_pdata->time;

		BufferData( (const char *)&tmp, sizeof(float) );
		data ++;
	}
}


void CSaveHL1 :: WriteString( const char *pname, const char *pdata )
{
#ifdef TOKENIZE
	short	token = (short)TokenHash( pdata );
	WriteShort( pname, &token, 1 );
#else
	BufferField( pname, strlen(pdata) + 1, pdata );
#endif
}


void CSaveHL1 :: WriteString( const char *pname, const int *stringId, int count )
{
	int i, size;

#ifdef TOKENIZE
	short	token = (short)TokenHash( STRING( *stringId ) );
	WriteShort( pname, &token, 1 );
#else
#if 0
	if ( count != 1 )
		ALERT( at_error, "No string arrays!\n" );
	WriteString( pname, (char *)STRING(*stringId) );
#endif

	size = 0;
	for ( i = 0; i < count; i++ )
		size += strlen( STRING_HL1( stringId[i] ) ) + 1;

	BufferHeader( pname, size );
	for ( i = 0; i < count; i++ )
	{
		const char *pString = STRING_HL1(stringId[i]);
		BufferData( pString, strlen(pString)+1 );
	}
#endif
}


void CSaveHL1 :: WriteVector( const char *pname, const VectorHL1 &value )
{
	WriteVector( pname, &value.x, 1 );
}


void CSaveHL1 :: WriteVector( const char *pname, const float *value, int count )
{
	BufferHeader( pname, sizeof(float) * 3 * count );
	BufferData( (const char *)value, sizeof(float) * 3 * count );
}



void CSaveHL1 :: WritePositionVector( const char *pname, const VectorHL1 &value )
{

	if ( m_pdata && m_pdata->fUseLandmark )
	{
		VectorHL1 tmp = value - m_pdata->vecLandmarkOffset;
		WriteVector( pname, tmp );
	}

	WriteVector( pname, value );
}


void CSaveHL1 :: WritePositionVector( const char *pname, const float *value, int count )
{
	int i;
	VectorHL1 tmp, input;

	BufferHeader( pname, sizeof(float) * 3 * count );
	for ( i = 0; i < count; i++ )
	{
		VectorHL1 tmp( value[0], value[1], value[2] );

		if ( m_pdata && m_pdata->fUseLandmark )
			tmp = tmp - m_pdata->vecLandmarkOffset;

		BufferData( (const char *)&tmp.x, sizeof(float) * 3 );
		value += 3;
	}
}


void CSaveHL1 :: WriteFunction( const char *pname, const int *data, int count )
{
	const char *functionName;

	functionName = NAME_FOR_FUNCTION( *data );
	if ( functionName )
		BufferField( pname, strlen(functionName) + 1, functionName );
	else
		ALERT( at_error, "Invalid function pointer in entity!" );
}


void EntvarsKeyvalue( entvars_t *pev, KeyValueData *pkvd )
{
	int i;
	TYPEDESCRIPTION		*pField;

	for ( i = 0; i < ENTVARS_COUNT; i++ )
	{
		pField = &gEntvarsDescription[i];

		if ( !stricmp( pField->fieldName, pkvd->szKeyName ) )
		{
			switch( pField->fieldType )
			{
			case FIELD_MODELNAME:
			case FIELD_SOUNDNAME:
			case FIELD_STRING:
				(*(int *)((char *)pev + pField->fieldOffset)) = ALLOC_STRING( pkvd->szValue );
				break;

			case FIELD_TIME:
			case FIELD_FLOAT:
				(*(float *)((char *)pev + pField->fieldOffset)) = atof( pkvd->szValue );
				break;

			case FIELD_INTEGER:
				(*(int *)((char *)pev + pField->fieldOffset)) = atoi( pkvd->szValue );
				break;

			case FIELD_POSITION_VECTOR:
			case FIELD_VECTOR:
				UTIL_StringToVector( (float *)((char *)pev + pField->fieldOffset), pkvd->szValue );
				break;

			default:
			case FIELD_EVARS:
			case FIELD_CLASSPTR:
			case FIELD_EDICT:
			case FIELD_ENTITY:
			case FIELD_POINTER:
				ALERT( at_error, "Bad field in entity!!\n" );
				break;
			}
			pkvd->fHandled = TRUE;
			return;
		}
	}
}



int CSaveHL1 :: WriteEntVars( const char *pname, entvars_t *pev )
{
	return WriteFields( pname, pev, gEntvarsDescription, ENTVARS_COUNT );
}



int CSaveHL1 :: WriteFields( const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount )
{
	int				i, j, actualCount, emptyCount;
	TYPEDESCRIPTION	*pTest;
	int				entityArray[MAX_ENTITYARRAY];

	// Precalculate the number of empty fields
	emptyCount = 0;
	for ( i = 0; i < fieldCount; i++ )
	{
		void *pOutputData;
		pOutputData = ((char *)pBaseData + pFields[i].fieldOffset );
		if ( DataEmpty( (const char *)pOutputData, pFields[i].fieldSize * gSizes[pFields[i].fieldType] ) )
			emptyCount++;
	}

	// Empty fields will not be written, write out the actual number of fields to be written
	actualCount = fieldCount - emptyCount;
	WriteInt( pname, &actualCount, 1 );

	for ( i = 0; i < fieldCount; i++ )
	{
		void *pOutputData;
		pTest = &pFields[ i ];
		pOutputData = ((char *)pBaseData + pTest->fieldOffset );

		// UNDONE: Must we do this twice?
		if ( DataEmpty( (const char *)pOutputData, pTest->fieldSize * gSizes[pTest->fieldType] ) )
			continue;

		switch( pTest->fieldType )
		{
		case FIELD_FLOAT:
			WriteFloat( pTest->fieldName, (float *)pOutputData, pTest->fieldSize );
		break;
		case FIELD_TIME:
			WriteTime( pTest->fieldName, (float *)pOutputData, pTest->fieldSize );
		break;
		case FIELD_MODELNAME:
		case FIELD_SOUNDNAME:
		case FIELD_STRING:
			WriteString( pTest->fieldName, (int *)pOutputData, pTest->fieldSize );
		break;
		case FIELD_CLASSPTR:
		case FIELD_EVARS:
		case FIELD_EDICT:
		case FIELD_ENTITY:
		case FIELD_EHANDLE:
			if ( pTest->fieldSize > MAX_ENTITYARRAY )
				ALERT( at_error, "Can't save more than %d entities in an array!!!\n", MAX_ENTITYARRAY );
			for ( j = 0; j < pTest->fieldSize; j++ )
			{
				switch( pTest->fieldType )
				{
					case FIELD_EVARS:
						entityArray[j] = EntityIndex( ((entvars_t **)pOutputData)[j] );
					break;
					case FIELD_CLASSPTR:
						entityArray[j] = EntityIndex( ((CBaseEntityHL1 **)pOutputData)[j] );
					break;
					case FIELD_EDICT:
						entityArray[j] = EntityIndex( ((edict_t_hl1 **)pOutputData)[j] );
					break;
					case FIELD_ENTITY:
						entityArray[j] = EntityIndex( ((EOFFSET *)pOutputData)[j] );
					break;
					case FIELD_EHANDLE:
						entityArray[j] = EntityIndex( (CBaseEntityHL1 *)(((EHANDLE *)pOutputData)[j]) );
					break;
				}
			}
			WriteInt( pTest->fieldName, entityArray, pTest->fieldSize );
		break;
		case FIELD_POSITION_VECTOR:
			WritePositionVector( pTest->fieldName, (float *)pOutputData, pTest->fieldSize );
		break;
		case FIELD_VECTOR:
			WriteVector( pTest->fieldName, (float *)pOutputData, pTest->fieldSize );
		break;

		case FIELD_BOOLEAN:
		case FIELD_INTEGER:
			WriteInt( pTest->fieldName, (int *)pOutputData, pTest->fieldSize );
		break;

		case FIELD_SHORT:
			WriteData( pTest->fieldName, 2 * pTest->fieldSize, ((char *)pOutputData) );
		break;

		case FIELD_CHARACTER:
			WriteData( pTest->fieldName, pTest->fieldSize, ((char *)pOutputData) );
		break;

		// For now, just write the address out, we're not going to change memory while doing this yet!
		case FIELD_POINTER:
			WriteInt( pTest->fieldName, (int *)(char *)pOutputData, pTest->fieldSize );
		break;

		case FIELD_FUNCTION:
			WriteFunction( pTest->fieldName, (int *)(char *)pOutputData, pTest->fieldSize );
		break;
		default:
			ALERT( at_error, "Bad field type\n" );
		}
	}

	return 1;
}


void CSaveHL1 :: BufferString( char *pdata, int len )
{
	char c = 0;

	BufferData( pdata, len );		// Write the string
	BufferData( &c, 1 );			// Write a null terminator
}


int CSaveHL1 :: DataEmpty( const char *pdata, int size )
{
	for ( int i = 0; i < size; i++ )
	{
		if ( pdata[i] )
			return 0;
	}
	return 1;
}


void CSaveHL1 :: BufferField( const char *pname, int size, const char *pdata )
{
	BufferHeader( pname, size );
	BufferData( pdata, size );
}


void CSaveHL1 :: BufferHeader( const char *pname, int size )
{
	short	hashvalue = TokenHash( pname );
	if ( size > 1<<(sizeof(short)*8) )
		ALERT( at_error, "CSave :: BufferHeader() size parameter exceeds 'short'!" );
	BufferData( (const char *)&size, sizeof(short) );
	BufferData( (const char *)&hashvalue, sizeof(short) );
}


void CSaveHL1 :: BufferData( const char *pdata, int size )
{
	if ( !m_pdata )
		return;

	if ( m_pdata->size + size > m_pdata->bufferSize )
	{
		ALERT( at_error, "Save/Restore overflow!" );
		m_pdata->size = m_pdata->bufferSize;
		return;
	}

	memcpy( m_pdata->pCurrentData, pdata, size );
	m_pdata->pCurrentData += size;
	m_pdata->size += size;
}



// --------------------------------------------------------------
//
// CRestore
//
// --------------------------------------------------------------

int CRestoreHL1::ReadField( void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount, int startField, int size, char *pName, void *pData )
{
	int i, j, stringCount, fieldNumber, entityIndex;
	TYPEDESCRIPTION *pTest;
	float	time, timeData;
	VectorHL1	position;
	edict_t_hl1	*pent;
	char	*pString;

	time = 0;
	position = VectorHL1(0,0,0);

	if ( m_pdata )
	{
		time = m_pdata->time;
		if ( m_pdata->fUseLandmark )
			position = m_pdata->vecLandmarkOffset;
	}

	for ( i = 0; i < fieldCount; i++ )
	{
		fieldNumber = (i+startField)%fieldCount;
		pTest = &pFields[ fieldNumber ];
		if ( !stricmp( pTest->fieldName, pName ) )
		{
			if ( !m_global || !(pTest->flags & FTYPEDESC_GLOBAL) )
			{
				for ( j = 0; j < pTest->fieldSize; j++ )
				{
					void *pOutputData = ((char *)pBaseData + pTest->fieldOffset + (j*gSizes[pTest->fieldType]) );
					void *pInputData = (char *)pData + j * gSizes[pTest->fieldType];

					switch( pTest->fieldType )
					{
					case FIELD_TIME:
						timeData = *(float *)pInputData;
						// Re-base time variables
						timeData += time;
						*((float *)pOutputData) = timeData;
					break;
					case FIELD_FLOAT:
						*((float *)pOutputData) = *(float *)pInputData;
					break;
					case FIELD_MODELNAME:
					case FIELD_SOUNDNAME:
					case FIELD_STRING:
						// Skip over j strings
						pString = (char *)pData;
						for ( stringCount = 0; stringCount < j; stringCount++ )
						{
							while (*pString)
								pString++;
							pString++;
						}
						pInputData = pString;
						if ( strlen( (char *)pInputData ) == 0 )
							*((int *)pOutputData) = 0;
						else
						{
							int string;

							string = ALLOC_STRING( (char *)pInputData );
							
							*((int *)pOutputData) = string;

							if ( !FStringNull( string ) && m_precache )
							{
								if ( pTest->fieldType == FIELD_MODELNAME )
									PRECACHE_MODEL( (char *)STRING_HL1( string ) );
								else if ( pTest->fieldType == FIELD_SOUNDNAME )
									PRECACHE_SOUND( (char *)STRING_HL1( string ) );
							}
						}
					break;
					case FIELD_EVARS:
						entityIndex = *( int *)pInputData;
						pent = EntityFromIndex( entityIndex );
						if ( pent )
							*((entvars_t **)pOutputData) = VARS(pent);
						else
							*((entvars_t **)pOutputData) = NULL;
					break;
					case FIELD_CLASSPTR:
						entityIndex = *( int *)pInputData;
						pent = EntityFromIndex( entityIndex );
						if ( pent )
							*((CBaseEntityHL1 **)pOutputData) = CBaseEntityHL1::Instance(pent);
						else
							*((CBaseEntityHL1 **)pOutputData) = NULL;
					break;
					case FIELD_EDICT:
						entityIndex = *( int *)pInputData;
						pent = EntityFromIndex( entityIndex );
						*((edict_t_hl1 **)pOutputData) = pent;
					break;
					case FIELD_EHANDLE:
						// Input and Output sizes are different!
						pOutputData = (char *)pOutputData + j*(sizeof(EHANDLE) - gSizes[pTest->fieldType]);
						entityIndex = *( int *)pInputData;
						pent = EntityFromIndex( entityIndex );
						if ( pent )
							*((EHANDLE *)pOutputData) = CBaseEntityHL1::Instance(pent);
						else
							*((EHANDLE *)pOutputData) = NULL;
					break;
					case FIELD_ENTITY:
						entityIndex = *( int *)pInputData;
						pent = EntityFromIndex( entityIndex );
						if ( pent )
							*((EOFFSET *)pOutputData) = OFFSET(pent);
						else
							*((EOFFSET *)pOutputData) = 0;
					break;
					case FIELD_VECTOR:
						((float *)pOutputData)[0] = ((float *)pInputData)[0];
						((float *)pOutputData)[1] = ((float *)pInputData)[1];
						((float *)pOutputData)[2] = ((float *)pInputData)[2];
					break;
					case FIELD_POSITION_VECTOR:
						((float *)pOutputData)[0] = ((float *)pInputData)[0] + position.x;
						((float *)pOutputData)[1] = ((float *)pInputData)[1] + position.y;
						((float *)pOutputData)[2] = ((float *)pInputData)[2] + position.z;
					break;

					case FIELD_BOOLEAN:
					case FIELD_INTEGER:
						*((int *)pOutputData) = *( int *)pInputData;
					break;

					case FIELD_SHORT:
						*((short *)pOutputData) = *( short *)pInputData;
					break;

					case FIELD_CHARACTER:
						*((char *)pOutputData) = *( char *)pInputData;
					break;

					case FIELD_POINTER:
						*((int *)pOutputData) = *( int *)pInputData;
					break;
					case FIELD_FUNCTION:
						if ( strlen( (char *)pInputData ) == 0 )
							*((int *)pOutputData) = 0;
						else
							*((int *)pOutputData) = FUNCTION_FROM_NAME( (char *)pInputData );
					break;

					default:
						ALERT( at_error, "Bad field type\n" );
					}
				}
			}
#if 0
			else
			{
				ALERT( at_console, "Skipping global field %s\n", pName );
			}
#endif
			return fieldNumber;
		}
	}

	return -1;
}


int CRestoreHL1::ReadEntVars( const char *pname, entvars_t *pev )
{
	return ReadFields( pname, pev, gEntvarsDescription, ENTVARS_COUNT );
}


int CRestoreHL1::ReadFields( const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount )
{
	unsigned short	i, token;
	int		lastField, fileCount;
	HEADER	header;

	i = ReadShort();
	ASSERT( i == sizeof(int) );			// First entry should be an int

	token = ReadShort();

	// Check the struct name
	if ( token != TokenHash(pname) )			// Field Set marker
	{
//		ALERT( at_error, "Expected %s found %s!\n", pname, BufferPointer() );
		BufferRewind( 2*sizeof(short) );
		return 0;
	}

	// Skip over the struct name
	fileCount = ReadInt();						// Read field count

	lastField = 0;								// Make searches faster, most data is read/written in the same order

	// Clear out base data
	for ( i = 0; i < fieldCount; i++ )
	{
		// Don't clear global fields
		if ( !m_global || !(pFields[i].flags & FTYPEDESC_GLOBAL) )
			memset( ((char *)pBaseData + pFields[i].fieldOffset), 0, pFields[i].fieldSize * gSizes[pFields[i].fieldType] );
	}

	for ( i = 0; i < fileCount; i++ )
	{
		BufferReadHeader( &header );
		lastField = ReadField( pBaseData, pFields, fieldCount, lastField, header.size, m_pdata->pTokens[header.token], header.pData );
		lastField++;
	}
	
	return 1;
}


void CRestoreHL1::BufferReadHeader( HEADER *pheader )
{
	ASSERT( pheader!=NULL );
	pheader->size = ReadShort();				// Read field size
	pheader->token = ReadShort();				// Read field name token
	pheader->pData = BufferPointer();			// Field Data is next
	BufferSkipBytes( pheader->size );			// Advance to next field
}


short	CRestoreHL1::ReadShort( void )
{
	short tmp = 0;

	BufferReadBytes( (char *)&tmp, sizeof(short) );

	return tmp;
}

int	CRestoreHL1::ReadInt( void )
{
	int tmp = 0;

	BufferReadBytes( (char *)&tmp, sizeof(int) );

	return tmp;
}

int CRestoreHL1::ReadNamedInt( const char *pName )
{
	HEADER header;

	BufferReadHeader( &header );
	return ((int *)header.pData)[0];
}

char *CRestoreHL1::ReadNamedString( const char *pName )
{
	HEADER header;

	BufferReadHeader( &header );
#ifdef TOKENIZE
	return (char *)(m_pdata->pTokens[*(short *)header.pData]);
#else
	return (char *)header.pData;
#endif
}


char *CRestoreHL1::BufferPointer( void )
{
	if ( !m_pdata )
		return NULL;

	return m_pdata->pCurrentData;
}

void CRestoreHL1::BufferReadBytes( char *pOutput, int size )
{
	ASSERT( m_pdata !=NULL );

	if ( !m_pdata || Empty() )
		return;

	if ( (m_pdata->size + size) > m_pdata->bufferSize )
	{
		ALERT( at_error, "Restore overflow!" );
		m_pdata->size = m_pdata->bufferSize;
		return;
	}

	if ( pOutput )
		memcpy( pOutput, m_pdata->pCurrentData, size );
	m_pdata->pCurrentData += size;
	m_pdata->size += size;
}


void CRestoreHL1::BufferSkipBytes( int bytes )
{
	BufferReadBytes( NULL, bytes );
}

int CRestoreHL1::BufferSkipZString( void )
{
	char *pszSearch;
	int	 len;

	if ( !m_pdata )
		return 0;

	int maxLen = m_pdata->bufferSize - m_pdata->size;

	len = 0;
	pszSearch = m_pdata->pCurrentData;
	while ( *pszSearch++ && len < maxLen )
		len++;

	len++;

	BufferSkipBytes( len );

	return len;
}

int	CRestoreHL1::BufferCheckZString( const char *string )
{
	if ( !m_pdata )
		return 0;

	int maxLen = m_pdata->bufferSize - m_pdata->size;
	int len = strlen( string );
	if ( len <= maxLen )
	{
		if ( !strncmp( string, m_pdata->pCurrentData, len ) )
			return 1;
	}
	return 0;
}




