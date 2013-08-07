//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Misc utility code.
//
// $NoKeywords: $
//=============================================================================//

#ifndef UTIL_H
#define UTIL_H
#ifdef _WIN32
#pragma once
#endif

#include "ai_activity.h"
#include "steam/steam_gameserver.h"
#include "enginecallback.h"
#include "basetypes.h"
#include "tempentity.h"
#include "string_t.h"
#include "gamestringpool.h"
#include "engine/IEngineTrace.h"
#include "worldsize.h"
#include "dt_send.h"
#include "server_class.h"
#include "shake.h"

#include "vstdlib/random.h"
#include <string.h>

#include "utlvector.h"
#include "util_shared.h"
#include "shareddefs.h"
#include "networkvar.h"

//HL1
#include "activity.h"

struct levellist_t;
class IServerNetworkable;
class IEntityFactory;

#ifdef _WIN32
	#define SETUP_EXTERNC(mapClassName)\
		extern "C" _declspec( dllexport ) IServerNetworkable* mapClassName( void );
#else
	#define SETUP_EXTERNC(mapClassName)
#endif

//
// How did I ever live without ASSERT?
//
#ifdef	DEBUG
void DBG_AssertFunction(bool fExpr, const char* szExpr, const char* szFile, int szLine, const char* szMessage);
#define ASSERT(f)		DBG_AssertFunction((bool)((f)!=0), #f, __FILE__, __LINE__, NULL)
#define ASSERTSZ(f, sz)	DBG_AssertFunction((bool)((f)!=0), #f, __FILE__, __LINE__, sz)
#else	// !DEBUG
#define ASSERT(f)
#define ASSERTSZ(f, sz)
#endif	// !DEBUG

#include "tier0/memdbgon.h"

// entity creation
// creates an entity that has not been linked to a classname
template< class T >
T *_CreateEntityTemplate( T *newEnt, const char *className )
{
	newEnt = new T; // this is the only place 'new' should be used!
	newEnt->PostConstructor( className );
	return newEnt;
}

#include "tier0/memdbgoff.h"

CBaseEntity *CreateEntityByName( const char *className, int iForceEdictIndex );

// creates an entity by name, and ensure it's correctness
// does not spawn the entity
// use the CREATE_ENTITY() macro which wraps this, instead of using it directly
template< class T >
T *_CreateEntity( T *newClass, const char *className )
{
	T *newEnt = dynamic_cast<T*>( CreateEntityByName(className, -1) );
	if ( !newEnt )
	{
		Warning( "classname %s used to create wrong class type\n", className );
		Assert(0);
	}

	return newEnt;
}

#define CREATE_ENTITY( newClass, className ) _CreateEntity( (newClass*)NULL, className )
#define CREATE_UNSAVED_ENTITY( newClass, className ) _CreateEntityTemplate( (newClass*)NULL, className )


// This is the glue that hooks .MAP entity class names to our CPP classes
abstract_class IEntityFactoryDictionary
{
public:
	virtual void InstallFactory( IEntityFactory *pFactory, const char *pClassName ) = 0;
	virtual IServerNetworkable *Create( const char *pClassName ) = 0;
	virtual void Destroy( const char *pClassName, IServerNetworkable *pNetworkable ) = 0;
	virtual IEntityFactory *FindFactory( const char *pClassName ) = 0;
	virtual const char *GetCannonicalName( const char *pClassName ) = 0;
};

IEntityFactoryDictionary *EntityFactoryDictionary();

inline bool CanCreateEntityClass( const char *pszClassname )
{
	return ( EntityFactoryDictionary() != NULL && EntityFactoryDictionary()->FindFactory( pszClassname ) != NULL );
}

abstract_class IEntityFactory
{
public:
	virtual IServerNetworkable *Create( const char *pClassName ) = 0;
	virtual void Destroy( IServerNetworkable *pNetworkable ) = 0;
	virtual size_t GetEntitySize() = 0;
};

template <class T>
class CEntityFactory : public IEntityFactory
{
public:
	CEntityFactory( const char *pClassName )
	{
		EntityFactoryDictionary()->InstallFactory( this, pClassName );
	}

	IServerNetworkable *Create( const char *pClassName )
	{
		T* pEnt = _CreateEntityTemplate((T*)NULL, pClassName);
		return pEnt->NetworkProp();
	}

	void Destroy( IServerNetworkable *pNetworkable )
	{
		if ( pNetworkable )
		{
			pNetworkable->Release();
		}
	}

	virtual size_t GetEntitySize()
	{
		return sizeof(T);
	}
};

#define LINK_ENTITY_TO_CLASS(mapClassName,DLLClassName) \
	static CEntityFactory<DLLClassName> mapClassName( #mapClassName );


//
// Conversion among the three types of "entity", including identity-conversions.
//
inline int	  ENTINDEX( edict_t *pEdict)			
{ 
	int nResult = pEdict ? pEdict->m_EdictIndex : 0;
	Assert( nResult == engine->IndexOfEdict(pEdict) );
	return nResult;
}

int	  ENTINDEX( CBaseEntity *pEnt );

inline edict_t* INDEXENT( int iEdictNum )		
{ 
	return engine->PEntityOfEntIndex(iEdictNum); 
}

// Testing the three types of "entity" for nullity
inline bool FNullEnt(const edict_t* pent)
{ 
	return pent == NULL || ENTINDEX((edict_t*)pent) == 0; 
}

// Dot products for view cone checking
#define VIEW_FIELD_FULL		(float)-1.0 // +-180 degrees
#define	VIEW_FIELD_WIDE		(float)-0.7 // +-135 degrees 0.1 // +-85 degrees, used for full FOV checks 
#define	VIEW_FIELD_NARROW	(float)0.7 // +-45 degrees, more narrow check used to set up ranged attacks
#define	VIEW_FIELD_ULTRA_NARROW	(float)0.9 // +-25 degrees, more narrow check used to set up ranged attacks

class CBaseEntity;
class CBasePlayer;

extern CGlobalVars *gpGlobals;

// Misc useful
inline bool FStrEq(const char *sz1, const char *sz2)
{
	return ( sz1 == sz2 || V_stricmp(sz1, sz2) == 0 );
}

#if 0
// UNDONE: Remove/alter MAKE_STRING so we can do this?
inline bool FStrEq( string_t str1, string_t str2 )
{
	// now that these are pooled, we can compare them with 
	// integer equality
	return str1 == str2;
}
#endif

const char *nexttoken(char *token, const char *str, char sep);

// Misc. Prototypes
void		UTIL_SetSize			(CBaseEntity *pEnt, const Vector &vecMin, const Vector &vecMax);
void		UTIL_ClearTrace			( trace_t &trace );
void		UTIL_SetTrace			(trace_t& tr, const Ray_t &ray, edict_t* edict, float fraction, int hitgroup, unsigned int contents, const Vector& normal, float intercept );

int			UTIL_PrecacheDecal		( const char *name, bool preload = false );

//-----------------------------------------------------------------------------

float		UTIL_GetSimulationInterval();

//-----------------------------------------------------------------------------
// Purpose: Gets a player pointer by 1-based index
//			If player is not yet spawned or connected, returns NULL
// Input  : playerIndex - index of the player - first player is index 1
//-----------------------------------------------------------------------------

// NOTENOTE: Use UTIL_GetLocalPlayer instead of UTIL_PlayerByIndex IF you're in single player
// and you want the player.
CBasePlayer	*UTIL_PlayerByIndex( int playerIndex );

// NOTENOTE: Use this instead of UTIL_PlayerByIndex IF you're in single player
// and you want the player.
// not useable in multiplayer - see UTIL_GetListenServerHost()
CBasePlayer* UTIL_GetLocalPlayer( void );

// get the local player on a listen server
CBasePlayer *UTIL_GetListenServerHost( void );

CBasePlayer* UTIL_PlayerByUserId( int userID );
CBasePlayer* UTIL_PlayerByName( const char *name ); // not case sensitive

// Returns true if the command was issued by the listenserver host, or by the dedicated server, via rcon or the server console.
// This is valid during ConCommand execution.
bool UTIL_IsCommandIssuedByServerAdmin( void );

CBaseEntity* UTIL_EntityByIndex( int entityIndex );

void		UTIL_GetPlayerConnectionInfo( int playerIndex, int& ping, int &packetloss );

void		UTIL_SetClientVisibilityPVS( edict_t *pClient, const unsigned char *pvs, int pvssize );
bool		UTIL_ClientPVSIsExpanded();

edict_t		*UTIL_FindClientInPVS( edict_t *pEdict );
edict_t		*UTIL_FindClientInVisibilityPVS( edict_t *pEdict );

// This is a version which finds any clients whose PVS intersects the box
CBaseEntity *UTIL_FindClientInPVS( const Vector &vecBoxMins, const Vector &vecBoxMaxs );

CBaseEntity *UTIL_EntitiesInPVS( CBaseEntity *pPVSEntity, CBaseEntity *pStartingEntity );

//-----------------------------------------------------------------------------
// class CFlaggedEntitiesEnum
//-----------------------------------------------------------------------------
// enumerate entities that match a set of edict flags into a static array
class CFlaggedEntitiesEnum : public IPartitionEnumerator
{
public:
	CFlaggedEntitiesEnum( CBaseEntity **pList, int listMax, int flagMask );

	// This gets called	by the enumeration methods with each element
	// that passes the test.
	virtual IterationRetval_t EnumElement( IHandleEntity *pHandleEntity );
	
	int GetCount() { return m_count; }
	bool AddToList( CBaseEntity *pEntity );
	
private:
	CBaseEntity		**m_pList;
	int				m_listMax;
	int				m_flagMask;
	int				m_count;
};

// Pass in an array of pointers and an array size, it fills the array and returns the number inserted
int			UTIL_EntitiesInBox( const Vector &mins, const Vector &maxs, CFlaggedEntitiesEnum *pEnum  );
int			UTIL_EntitiesAlongRay( const Ray_t &ray, CFlaggedEntitiesEnum *pEnum  );
int			UTIL_EntitiesInSphere( const Vector &center, float radius, CFlaggedEntitiesEnum *pEnum  );

inline int UTIL_EntitiesInBox( CBaseEntity **pList, int listMax, const Vector &mins, const Vector &maxs, int flagMask )
{
	CFlaggedEntitiesEnum boxEnum( pList, listMax, flagMask );
	return UTIL_EntitiesInBox( mins, maxs, &boxEnum );
}

inline int UTIL_EntitiesAlongRay( CBaseEntity **pList, int listMax, const Ray_t &ray, int flagMask )
{
	CFlaggedEntitiesEnum rayEnum( pList, listMax, flagMask );
	return UTIL_EntitiesAlongRay( ray, &rayEnum );
}

inline int UTIL_EntitiesInSphere( CBaseEntity **pList, int listMax, const Vector &center, float radius, int flagMask )
{
	CFlaggedEntitiesEnum sphereEnum( pList, listMax, flagMask );
	return UTIL_EntitiesInSphere( center, radius, &sphereEnum );
}

// marks the entity for deletion so it will get removed next frame
void UTIL_Remove( IServerNetworkable *oldObj );
void UTIL_Remove( CBaseEntity *oldObj );

// deletes an entity, without any delay.  Only use this when sure no pointers rely on this entity.
void UTIL_DisableRemoveImmediate();
void UTIL_EnableRemoveImmediate();
void UTIL_RemoveImmediate( CBaseEntity *oldObj );

// make this a fixed size so it just sits on the stack
#define MAX_SPHERE_QUERY	512
class CEntitySphereQuery
{
public:
	// currently this builds the list in the constructor
	// UNDONE: make an iterative query of ISpatialPartition so we could
	// make queries like this optimal
	CEntitySphereQuery( const Vector &center, float radius, int flagMask=0 );
	CBaseEntity *GetCurrentEntity();
	inline void NextEntity() { m_listIndex++; }

private:
	int			m_listIndex;
	int			m_listCount;
	CBaseEntity *m_pList[MAX_SPHERE_QUERY];
};

enum soundlevel_t;

// Drops an entity onto the floor
int			UTIL_DropToFloor( CBaseEntity *pEntity, unsigned int mask, CBaseEntity *pIgnore = NULL );

// Returns false if any part of the bottom of the entity is off an edge that is not a staircase.
bool		UTIL_CheckBottom( CBaseEntity *pEntity, ITraceFilter *pTraceFilter, float flStepSize );

void		UTIL_SetOrigin			( CBaseEntity *entity, const Vector &vecOrigin, bool bFireTriggers = false );
void		UTIL_EmitAmbientSound	( int entindex, const Vector &vecOrigin, const char *samp, float vol, soundlevel_t soundlevel, int fFlags, int pitch, float soundtime = 0.0f, float *duration = NULL );
void		UTIL_ParticleEffect		( const Vector &vecOrigin, const Vector &vecDirection, ULONG ulColor, ULONG ulCount );
void		UTIL_ScreenShake		( const Vector &center, float amplitude, float frequency, float duration, float radius, ShakeCommand_t eCommand, bool bAirShake=false );
void		UTIL_ScreenShakeObject	( CBaseEntity *pEnt, const Vector &center, float amplitude, float frequency, float duration, float radius, ShakeCommand_t eCommand, bool bAirShake=false );
void		UTIL_ViewPunch			( const Vector &center, QAngle angPunch, float radius, bool bInAir );
void		UTIL_ShowMessage		( const char *pString, CBasePlayer *pPlayer );
void		UTIL_ShowMessageAll		( const char *pString );
void		UTIL_ScreenFadeAll		( const color32 &color, float fadeTime, float holdTime, int flags );
void		UTIL_ScreenFade			( CBaseEntity *pEntity, const color32 &color, float fadeTime, float fadeHold, int flags );
void		UTIL_MuzzleFlash		( const Vector &origin, const QAngle &angles, int scale, int type );
Vector		UTIL_PointOnLineNearestPoint(const Vector& vStartPos, const Vector& vEndPos, const Vector& vPoint, bool clampEnds = false );

int			UTIL_EntityInSolid( CBaseEntity *ent );

bool		UTIL_IsMasterTriggered	(string_t sMaster, CBaseEntity *pActivator);
void		UTIL_BloodStream( const Vector &origin, const Vector &direction, int color, int amount );
void		UTIL_BloodSpray( const Vector &pos, const Vector &dir, int color, int amount, int flags );
Vector		UTIL_RandomBloodVector( void );
void		UTIL_ImpactTrace( trace_t *pTrace, int iDamageType, const char *pCustomImpactName = NULL );
void		UTIL_PlayerDecalTrace( trace_t *pTrace, int playernum );
void		UTIL_Smoke( const Vector &origin, const float scale, const float framerate );
void		UTIL_AxisStringToPointDir( Vector &start, Vector &dir, const char *pString );
void		UTIL_AxisStringToPointPoint( Vector &start, Vector &end, const char *pString );
void		UTIL_AxisStringToUnitDir( Vector &dir, const char *pString );
void		UTIL_ClipPunchAngleOffset( QAngle &in, const QAngle &punch, const QAngle &clip );
void		UTIL_PredictedPosition( CBaseEntity *pTarget, float flTimeDelta, Vector *vecPredictedPosition );
void		UTIL_Beam( Vector &Start, Vector &End, int nModelIndex, int nHaloIndex, unsigned char FrameStart, unsigned char FrameRate,
				float Life, unsigned char Width, unsigned char EndWidth, unsigned char FadeLength, unsigned char Noise, unsigned char Red, unsigned char Green,
				unsigned char Blue, unsigned char Brightness, unsigned char Speed);

char		*UTIL_VarArgs( PRINTF_FORMAT_STRING const char *format, ... );
bool		UTIL_IsValidEntity( CBaseEntity *pEnt );
bool		UTIL_TeamsMatch( const char *pTeamName1, const char *pTeamName2 );

// snaps a vector to the nearest axis vector (if within epsilon)
void		UTIL_SnapDirectionToAxis( Vector &direction, float epsilon = 0.002f );

//Set the entity to point at the target specified
bool UTIL_PointAtEntity( CBaseEntity *pEnt, CBaseEntity *pTarget );
void UTIL_PointAtNamedEntity( CBaseEntity *pEnt, string_t strTarget );

// Copy the pose parameter values from one entity to the other
bool UTIL_TransferPoseParameters( CBaseEntity *pSourceEntity, CBaseEntity *pDestEntity );

// Search for water transition along a vertical line
float UTIL_WaterLevel( const Vector &position, float minz, float maxz );

// Like UTIL_WaterLevel, but *way* less expensive.
// I didn't replace UTIL_WaterLevel everywhere to avoid breaking anything.
float UTIL_FindWaterSurface( const Vector &position, float minz, float maxz );

void UTIL_Bubbles( const Vector& mins, const Vector& maxs, int count );
void UTIL_BubbleTrail( const Vector& from, const Vector& to, int count );

// allows precacheing of other entities
void UTIL_PrecacheOther( const char *szClassname, const char *modelName = NULL );

// prints a message to each client
void			UTIL_ClientPrintAll( int msg_dest, const char *msg_name, const char *param1 = NULL, const char *param2 = NULL, const char *param3 = NULL, const char *param4 = NULL );
inline void		UTIL_CenterPrintAll( const char *msg_name, const char *param1 = NULL, const char *param2 = NULL, const char *param3 = NULL, const char *param4 = NULL ) 
{
	UTIL_ClientPrintAll( HUD_PRINTCENTER, msg_name, param1, param2, param3, param4 );
}

void UTIL_ValidateSoundName( string_t &name, const char *defaultStr );

void UTIL_ClientPrintFilter( IRecipientFilter& filter, int msg_dest, const char *msg_name, const char *param1 = NULL, const char *param2 = NULL, const char *param3 = NULL, const char *param4 = NULL );

// prints messages through the HUD
void ClientPrint( CBasePlayer *player, int msg_dest, const char *msg_name, const char *param1 = NULL, const char *param2 = NULL, const char *param3 = NULL, const char *param4 = NULL );

// prints a message to the HUD say (chat)
void		UTIL_SayText( const char *pText, CBasePlayer *pEntity );
void		UTIL_SayTextAll( const char *pText, CBasePlayer *pEntity = NULL, bool bChat = false );
void		UTIL_SayTextFilter( IRecipientFilter& filter, const char *pText, CBasePlayer *pEntity, bool bChat );
void		UTIL_SayText2Filter( IRecipientFilter& filter, CBasePlayer *pEntity, bool bChat, const char *msg_name, const char *param1 = NULL, const char *param2 = NULL, const char *param3 = NULL, const char *param4 = NULL );

byte		*UTIL_LoadFileForMe( const char *filename, int *pLength );
void        UTIL_FreeFile( byte *buffer );

class CGameTrace;
typedef CGameTrace trace_t;

//-----------------------------------------------------------------------------
// These are inlined for backwards compatibility
//-----------------------------------------------------------------------------
inline float UTIL_Approach( float target, float value, float speed )
{
	return Approach( target, value, speed );
}

inline float UTIL_ApproachAngle( float target, float value, float speed )
{
	return ApproachAngle( target, value, speed );
}

inline float UTIL_AngleDistance( float next, float cur )
{
	return AngleDistance( next, cur );
}

inline float UTIL_AngleMod(float a)
{
	return anglemod(a);
}

inline float UTIL_AngleDiff( float destAngle, float srcAngle )
{
	return AngleDiff( destAngle, srcAngle );
}

typedef struct hudtextparms_s
{
	float		x;
	float		y;
	int			effect;
	byte		r1, g1, b1, a1;
	byte		r2, g2, b2, a2;
	float		fadeinTime;
	float		fadeoutTime;
	float		holdTime;
	float		fxTime;
	int			channel;
} hudtextparms_t;


//-----------------------------------------------------------------------------
// Sets the model to be associated with an entity
//-----------------------------------------------------------------------------
void UTIL_SetModel( CBaseEntity *pEntity, const char *pModelName );


// prints as transparent 'title' to the HUD
void			UTIL_HudMessageAll( const hudtextparms_t &textparms, const char *pMessage );
void			UTIL_HudMessage( CBasePlayer *pToPlayer, const hudtextparms_t &textparms, const char *pMessage );

// brings up hud keyboard hints display
void			UTIL_HudHintText( CBaseEntity *pEntity, const char *pMessage );

// Writes message to console with timestamp and FragLog header.
void			UTIL_LogPrintf( PRINTF_FORMAT_STRING const char *fmt, ... );

// Sorta like FInViewCone, but for nonNPCs. 
float UTIL_DotPoints ( const Vector &vecSrc, const Vector &vecCheck, const Vector &vecDir );

void UTIL_StripToken( const char *pKey, char *pDest );// for redundant keynames

// Misc functions
int BuildChangeList( levellist_t *pLevelList, int maxList );

// computes gravity scale for an absolute gravity.  Pass the result into CBaseEntity::SetGravity()
float UTIL_ScaleForGravity( float desiredGravity );



//
// Constants that were used only by QC (maybe not used at all now)
//
// Un-comment only as needed
//

#include "globals.h"

#define	LFO_SQUARE			1
#define LFO_TRIANGLE		2
#define LFO_RANDOM			3

// func_rotating
#define SF_BRUSH_ROTATE_Y_AXIS		0
#define SF_BRUSH_ROTATE_START_ON	1
#define SF_BRUSH_ROTATE_BACKWARDS	2
#define SF_BRUSH_ROTATE_Z_AXIS		4
#define SF_BRUSH_ROTATE_X_AXIS		8
#define SF_BRUSH_ROTATE_CLIENTSIDE	16


#define SF_BRUSH_ROTATE_SMALLRADIUS	128
#define SF_BRUSH_ROTATE_MEDIUMRADIUS 256
#define SF_BRUSH_ROTATE_LARGERADIUS 512

#define PUSH_BLOCK_ONLY_X	1
#define PUSH_BLOCK_ONLY_Y	2

#define SF_LIGHT_START_OFF		1

#define SPAWNFLAG_NOMESSAGE	1
#define SPAWNFLAG_NOTOUCH	1
#define SPAWNFLAG_DROIDONLY	4

#define SPAWNFLAG_USEONLY	1		// can't be touched, must be used (buttons)

#define TELE_PLAYER_ONLY	1
#define TELE_SILENT			2

// Sound Utilities

enum soundlevel_t;

void SENTENCEG_Init();
void SENTENCEG_Stop(edict_t *entity, int isentenceg, int ipick);
int SENTENCEG_PlayRndI(edict_t *entity, int isentenceg, float volume, soundlevel_t soundlevel, int flags, int pitch);
int SENTENCEG_PlayRndSz(edict_t *entity, const char *szrootname, float volume, soundlevel_t soundlevel, int flags, int pitch);
int SENTENCEG_PlaySequentialSz(edict_t *entity, const char *szrootname, float volume, soundlevel_t soundlevel, int flags, int pitch, int ipick, int freset);
void SENTENCEG_PlaySentenceIndex( edict_t *entity, int iSentenceIndex, float volume, soundlevel_t soundlevel, int flags, int pitch );
int SENTENCEG_PickRndSz(const char *szrootname);
int SENTENCEG_GetIndex(const char *szrootname);
int SENTENCEG_Lookup(const char *sample);

char TEXTURETYPE_Find( trace_t *ptr );

void UTIL_EmitSoundSuit(edict_t *entity, const char *sample);
int  UTIL_EmitGroupIDSuit(edict_t *entity, int isentenceg);
int  UTIL_EmitGroupnameSuit(edict_t *entity, const char *groupname);
void UTIL_RestartAmbientSounds( void );

class EntityMatrix : public VMatrix
{
public:
	void InitFromEntity( CBaseEntity *pEntity, int iAttachment=0 );
	void InitFromEntityLocal( CBaseEntity *entity );

	inline Vector LocalToWorld( const Vector &vVec ) const
	{
		return VMul4x3( vVec );
	}

	inline Vector WorldToLocal( const Vector &vVec ) const
	{
		return VMul4x3Transpose( vVec );
	}

	inline Vector LocalToWorldRotation( const Vector &vVec ) const
	{
		return VMul3x3( vVec );
	}

	inline Vector WorldToLocalRotation( const Vector &vVec ) const
	{
		return VMul3x3Transpose( vVec );
	}
};

inline float UTIL_DistApprox( const Vector &vec1, const Vector &vec2 );
inline float UTIL_DistApprox2D( const Vector &vec1, const Vector &vec2 );

//---------------------------------------------------------
//---------------------------------------------------------
inline float UTIL_DistApprox( const Vector &vec1, const Vector &vec2 )
{
	float dx;
	float dy;
	float dz;

	dx = vec1.x - vec2.x;
	dy = vec1.y - vec2.y;
	dz = vec1.z - vec2.z;

	return fabs(dx) + fabs(dy) + fabs(dz);
}

//---------------------------------------------------------
//---------------------------------------------------------
inline float UTIL_DistApprox2D( const Vector &vec1, const Vector &vec2 )
{
	float dx;
	float dy;

	dx = vec1.x - vec2.x;
	dy = vec1.y - vec2.y;

	return fabs(dx) + fabs(dy);
}

// Find out if an entity is facing another entity or position within a given tolerance range
bool UTIL_IsFacingWithinTolerance( CBaseEntity *pViewer, const Vector &vecPosition, float flDotTolerance, float *pflDot = NULL );
bool UTIL_IsFacingWithinTolerance( CBaseEntity *pViewer, CBaseEntity *pTarget, float flDotTolerance, float *pflDot = NULL );

void UTIL_GetDebugColorForRelationship( int nRelationship, int &r, int &g, int &b );

struct datamap_t;
extern const char	*UTIL_FunctionToName( datamap_t *pMap, inputfunc_t *function );

int UTIL_GetCommandClientIndex( void );
CBasePlayer *UTIL_GetCommandClient( void );
bool UTIL_GetModDir( char *lpszTextOut, unsigned int nSize );

AngularImpulse WorldToLocalRotation( const VMatrix &localToWorld, const Vector &worldAxis, float rotation );
void UTIL_WorldToParentSpace( CBaseEntity *pEntity, Vector &vecPosition, QAngle &vecAngles );
void UTIL_WorldToParentSpace( CBaseEntity *pEntity, Vector &vecPosition, Quaternion &quat );
void UTIL_ParentToWorldSpace( CBaseEntity *pEntity, Vector &vecPosition, QAngle &vecAngles );
void UTIL_ParentToWorldSpace( CBaseEntity *pEntity, Vector &vecPosition, Quaternion &quat );

bool UTIL_LoadAndSpawnEntitiesFromScript( CUtlVector <CBaseEntity*> &entities, const char *pScriptFile, const char *pBlock, bool bActivate = true );

// Given a vector, clamps the scalar axes to MAX_COORD_FLOAT ranges from worldsize.h
void UTIL_BoundToWorldSize( Vector *pVecPos );

// HL1

inline void MESSAGE_BEGIN_HL1( int msg_dest, int msg_type, const float *pOrigin, entvars_t *ent );  // implementation later in this file

extern globalvars_t				*gpGlobalsHL1;

// Use this instead of ALLOC_STRING on constant strings
#define STRING_HL1(offset)		(const char *)(gpGlobalsHL1->pStringBase + (int)offset)
#define MAKE_STRING_HL1(str)	((int)str - (int)STRING_HL1(0))

inline edict_t_hl1 *FIND_ENTITY_BY_CLASSNAME(edict_t_hl1 *entStart, const char *pszName) 
{
	return FIND_ENTITY_BY_STRING(entStart, "classname", pszName);
}	

inline edict_t_hl1 *FIND_ENTITY_BY_TARGETNAME(edict_t_hl1 *entStart, const char *pszName) 
{
	return FIND_ENTITY_BY_STRING(entStart, "targetname", pszName);
}	

// for doing a reverse lookup. Say you have a door, and want to find its button.
inline edict_t_hl1 *FIND_ENTITY_BY_TARGET(edict_t_hl1 *entStart, const char *pszName) 
{
	return FIND_ENTITY_BY_STRING(entStart, "target", pszName);
}	

// Keeps clutter down a bit, when writing key-value pairs
#define WRITEKEY_INT(pf, szKeyName, iKeyValue) ENGINE_FPRINTF(pf, "\"%s\" \"%d\"\n", szKeyName, iKeyValue)
#define WRITEKEY_FLOAT(pf, szKeyName, flKeyValue)								\
		ENGINE_FPRINTF(pf, "\"%s\" \"%f\"\n", szKeyName, flKeyValue)
#define WRITEKEY_STRING(pf, szKeyName, szKeyValue)								\
		ENGINE_FPRINTF(pf, "\"%s\" \"%s\"\n", szKeyName, szKeyValue)
#define WRITEKEY_VECTOR(pf, szKeyName, flX, flY, flZ)							\
		ENGINE_FPRINTF(pf, "\"%s\" \"%f %f %f\"\n", szKeyName, flX, flY, flZ)

// Keeps clutter down a bit, when using a float as a bit-vector
#define SetBits(flBitVector, bits)		((flBitVector) = (int)(flBitVector) | (bits))
#define ClearBits(flBitVector, bits)	((flBitVector) = (int)(flBitVector) & ~(bits))
#define FBitSetHL1(flBitVector, bit)		((int)(flBitVector) & (bit))

// Makes these more explicit, and easier to find
#define FILE_GLOBAL static
#define DLL_GLOBAL

// Until we figure out why "const" gives the compiler problems, we'll just have to use
// this bogus "empty" define to mark things as constant.
#define CONSTANT

// More explicit than "int"
typedef int EOFFSET;

// In case it's not alread defined
typedef int BOOL;

// In case this ever changes
#define M_PI			3.14159265358979323846

// Keeps clutter down a bit, when declaring external entity/global method prototypes
#define DECLARE_GLOBAL_METHOD(MethodName)  extern void DLLEXPORT MethodName( void )
#define GLOBAL_METHOD(funcname)					void DLLEXPORT funcname(void)

// This is the glue that hooks .MAP entity class names to our CPP classes
// The _declspec forces them to be exported by name so we can do a lookup with GetProcAddress()
// The function is used to intialize / allocate the object for the entity
#ifdef _WIN32
#define LINK_ENTITY_TO_CLASS(mapClassName,DLLClassName) \
	extern "C" _declspec( dllexport ) void mapClassName( entvars_t *pev ); \
	void mapClassName( entvars_t *pev ) { GetClassPtr( (DLLClassName *)pev ); }
#else
#define LINK_ENTITY_TO_CLASS_HL1(mapClassName,DLLClassName) extern "C" void mapClassName( entvars_t *pev ); void mapClassName( entvars_t *pev ) { GetClassPtr( (DLLClassName *)pev ); }
#endif


//
// Conversion among the three types of "entity", including identity-conversions.
//
#ifdef DEBUG
	extern edict_t_hl1 *DBG_EntOfVars(const entvars_t *pev);
	inline edict_t_hl1 *ENT(const entvars_t *pev)	{ return DBG_EntOfVars(pev); }
#else
	inline edict_t_hl1 *ENT(const entvars_t *pev)	{ return pev->pContainingEntity; }
#endif
inline edict_t_hl1 *ENT(edict_t_hl1 *pent)		{ return pent; }
inline edict_t_hl1 *ENT(EOFFSET eoffset)			{ return (*g_engfuncsHL1.pfnPEntityOfEntOffset)(eoffset); }
inline EOFFSET OFFSET(EOFFSET eoffset)			{ return eoffset; }
inline EOFFSET OFFSET(const edict_t_hl1 *pent)	
{ 
#if _DEBUG
	if ( !pent )
		ALERT( at_error, "Bad ent in OFFSET()\n" );
#endif
	return (*g_engfuncsHL1.pfnEntOffsetOfPEntity)(pent); 
}
inline EOFFSET OFFSET(entvars_t *pev)				
{ 
#if _DEBUG
	if ( !pev )
		ALERT( at_error, "Bad pev in OFFSET()\n" );
#endif
	return OFFSET(ENT(pev)); 
}
inline entvars_t *VARS(entvars_t *pev)					{ return pev; }

inline entvars_t *VARS(edict_t_hl1 *pent)			
{ 
	if ( !pent )
		return NULL;

	return &pent->v; 
}

inline entvars_t* VARS(EOFFSET eoffset)				{ return VARS(ENT(eoffset)); }
inline int	  ENTINDEXHL1(edict_t_hl1 *pEdict)			{ return (*g_engfuncsHL1.pfnIndexOfEdict)(pEdict); }
inline edict_t_hl1* INDEXENTHL1( int iEdictNum )		{ return (*g_engfuncsHL1.pfnPEntityOfEntIndex)(iEdictNum); }
inline void MESSAGE_BEGIN_HL1( int msg_dest, int msg_type, const float *pOrigin, entvars_t *ent ) {
	(*g_engfuncsHL1.pfnMessageBegin)(msg_dest, msg_type, pOrigin, ENT(ent));
}

// Testing the three types of "entity" for nullity
#define eoNullEntity 0
inline BOOL FNullEnt(EOFFSET eoffset)			{ return eoffset == 0; }
inline BOOL FNullEnt(const edict_t_hl1* pent)	{ return pent == NULL || FNullEnt(OFFSET(pent)); }
inline BOOL FNullEnt(entvars_t* pev)				{ return pev == NULL || FNullEnt(OFFSET(pev)); }

// Testing strings for nullity
#define iStringNull 0
inline BOOL FStringNull(int iString)			{ return iString == iStringNull; }

#define cchMapNameMost 32

// Dot products for view cone checking
#define VIEW_FIELD_FULL		(float)-1.0 // +-180 degrees
#define	VIEW_FIELD_WIDE		(float)-0.7 // +-135 degrees 0.1 // +-85 degrees, used for full FOV checks 
#define	VIEW_FIELD_NARROW	(float)0.7 // +-45 degrees, more narrow check used to set up ranged attacks
#define	VIEW_FIELD_ULTRA_NARROW	(float)0.9 // +-25 degrees, more narrow check used to set up ranged attacks

// All monsters need this data
#define		DONT_BLEED			-1
#define		BLOOD_COLOR_RED		(BYTE)247
#define		BLOOD_COLOR_YELLOW	(BYTE)195
#define		BLOOD_COLOR_GREEN	BLOOD_COLOR_YELLOW

typedef enum 
{

	MONSTERSTATE_NONE = 0,
	MONSTERSTATE_IDLE,
	MONSTERSTATE_COMBAT,
	MONSTERSTATE_ALERT,
	MONSTERSTATE_HUNT,
	MONSTERSTATE_PRONE,
	MONSTERSTATE_SCRIPT,
	MONSTERSTATE_PLAYDEAD,
	MONSTERSTATE_DEAD

} MONSTERSTATE;



// Things that toggle (buttons/triggers/doors) need this
typedef enum
	{
	TS_AT_TOP,
	TS_AT_BOTTOM,
	TS_GOING_UP,
	TS_GOING_DOWN
	} TOGGLE_STATE;

// Misc useful
inline BOOL FStrEqHL1(const char*sz1, const char*sz2)
	{ return (strcmp(sz1, sz2) == 0); }
inline BOOL FClassnameIs(edict_t_hl1* pent, const char* szClassname)
	{ return FStrEq(STRING_HL1(VARS(pent)->classname), szClassname); }
inline BOOL FClassnameIs(entvars_t* pev, const char* szClassname)
	{ return FStrEq(STRING_HL1(pev->classname), szClassname); }

class CBaseEntityHL1;

// Misc. Prototypes
extern void			UTIL_SetSizeHL1			(entvars_t* pev, const VectorHL1 &vecMin, const VectorHL1 &vecMax);
extern float		UTIL_VecToYaw			(const VectorHL1 &vec);
extern VectorHL1	UTIL_VecToAngles		(const VectorHL1 &vec);
extern float		UTIL_AngleMod			(float a);
extern float		UTIL_AngleDiff			( float destAngle, float srcAngle );

extern CBaseEntityHL1	*UTIL_FindEntityInSphere(CBaseEntityHL1 *pStartEntity, const VectorHL1 &vecCenter, float flRadius);
extern CBaseEntityHL1	*UTIL_FindEntityByString(CBaseEntityHL1 *pStartEntity, const char *szKeyword, const char *szValue );
extern CBaseEntityHL1	*UTIL_FindEntityByClassname(CBaseEntityHL1 *pStartEntity, const char *szName );
extern CBaseEntityHL1	*UTIL_FindEntityByTargetname(CBaseEntityHL1 *pStartEntity, const char *szName );
extern CBaseEntityHL1	*UTIL_FindEntityGeneric(const char *szName, VectorHL1 &vecSrc, float flRadius );

// returns a CBaseEntityHL1 pointer to a player by index.  Only returns if the player is spawned and connected
// otherwise returns NULL
// Index is 1 based
extern CBaseEntityHL1	*UTIL_PlayerByIndexHL1( int playerIndex );

#define UTIL_EntitiesInPVSHL1(pent)			(*g_engfuncsHL1.pfnEntitiesInPVS)(pent)
extern void			UTIL_MakeVectors		(const VectorHL1 &vecAngles);

// Pass in an array of pointers and an array size, it fills the array and returns the number inserted
extern int			UTIL_MonstersInSphere( CBaseEntityHL1 **pList, int listMax, const VectorHL1 &center, float radius );
extern int			UTIL_EntitiesInBox( CBaseEntityHL1 **pList, int listMax, const VectorHL1 &mins, const VectorHL1 &maxs, int flagMask );

inline void UTIL_MakeVectorsPrivate( const VectorHL1 &vecAngles, float *p_vForward, float *p_vRight, float *p_vUp )
{
	g_engfuncsHL1.pfnAngleVectors( vecAngles, p_vForward, p_vRight, p_vUp );
}

extern void			UTIL_MakeAimVectors		( const VectorHL1 &vecAngles ); // like MakeVectors, but assumes pitch isn't inverted
extern void			UTIL_MakeInvVectors		( const VectorHL1 &vec, globalvars_t *pgv );

extern void			UTIL_SetOrigin			( entvars_t* pev, const VectorHL1 &vecOrigin );
extern void			UTIL_EmitAmbientSound	( edict_t_hl1 *entity, const VectorHL1 &vecOrigin, const char *samp, float vol, float attenuation, int fFlags, int pitch );
extern void			UTIL_ParticleEffect		( const VectorHL1 &vecOrigin, const VectorHL1 &vecDirection, ULONG ulColor, ULONG ulCount );
extern void			UTIL_ScreenShake		( const VectorHL1 &center, float amplitude, float frequency, float duration, float radius );
extern void			UTIL_ScreenShakeAll		( const VectorHL1 &center, float amplitude, float frequency, float duration );
extern void			UTIL_ShowMessageHL1		( const char *pString, CBaseEntityHL1 *pPlayer );
extern void			UTIL_ShowMessageAllHL1	( const char *pString );
extern void			UTIL_ScreenFadeAll		( const VectorHL1 &color, float fadeTime, float holdTime, int alpha, int flags );
extern void			UTIL_ScreenFade			( CBaseEntityHL1 *pEntity, const VectorHL1 &color, float fadeTime, float fadeHold, int alpha, int flags );

typedef enum { ignore_monsters=1, dont_ignore_monsters=0, missile=2 } IGNORE_MONSTERS;
typedef enum { ignore_glass=1, dont_ignore_glass=0 } IGNORE_GLASS;
extern void			UTIL_TraceLine			(const VectorHL1 &vecStart, const VectorHL1 &vecEnd, IGNORE_MONSTERS igmon, edict_t_hl1 *pentIgnore, TraceResult *ptr);
extern void			UTIL_TraceLine			(const VectorHL1 &vecStart, const VectorHL1 &vecEnd, IGNORE_MONSTERS igmon, IGNORE_GLASS ignoreGlass, edict_t_hl1 *pentIgnore, TraceResult *ptr);
typedef enum { point_hull=0, human_hull=1, large_hull=2, head_hull=3 };
extern void			UTIL_TraceHull			(const VectorHL1 &vecStart, const VectorHL1 &vecEnd, IGNORE_MONSTERS igmon, int hullNumber, edict_t_hl1 *pentIgnore, TraceResult *ptr);
extern TraceResult	UTIL_GetGlobalTrace		(void);
extern void			UTIL_TraceModel			(const VectorHL1 &vecStart, const VectorHL1 &vecEnd, int hullNumber, edict_t_hl1 *pentModel, TraceResult *ptr);
extern VectorHL1	UTIL_GetAimVectorHL1		(edict_t_hl1* pent, float flSpeed);
extern int			UTIL_PointContents		(const VectorHL1 &vec);

extern int			UTIL_IsMasterTriggered	(string_t sMaster, CBaseEntityHL1 *pActivator);
extern void			UTIL_BloodStream( const VectorHL1 &origin, const VectorHL1 &direction, int color, int amount );
extern void			UTIL_BloodDrips( const VectorHL1 &origin, const VectorHL1 &direction, int color, int amount );
extern VectorHL1	UTIL_RandomBloodVectorHL1( void );
extern BOOL			UTIL_ShouldShowBloodHL1( int bloodColor );
extern void			UTIL_BloodDecalTrace( TraceResult *pTrace, int bloodColor );
extern void			UTIL_DecalTrace( TraceResult *pTrace, int decalNumber );
extern void			UTIL_PlayerDecalTrace( TraceResult *pTrace, int playernum, int decalNumber, BOOL bIsCustom );
extern void			UTIL_GunshotDecalTrace( TraceResult *pTrace, int decalNumber );
extern void			UTIL_Sparks( const VectorHL1 &position );
extern void			UTIL_Ricochet( const VectorHL1 &position, float scale );
extern void			UTIL_StringToVector( float *pVector, const char *pString );
extern void			UTIL_StringToIntArray( int *pVector, int count, const char *pString );
extern VectorHL1	UTIL_ClampVectorToBoxHL1( const VectorHL1 &input, const VectorHL1 &clampSize );
extern float		UTIL_Approach( float target, float value, float speed );
extern float		UTIL_ApproachAngle( float target, float value, float speed );
extern float		UTIL_AngleDistance( float next, float cur );

extern char			*UTIL_VarArgs( char *format, ... );
extern void			UTIL_Remove( CBaseEntityHL1 *pEntity );
extern BOOL			UTIL_IsValidEntity( edict_t_hl1 *pent );
extern BOOL			UTIL_TeamsMatchHL1( const char *pTeamName1, const char *pTeamName2 );

// Use for ease-in, ease-out style interpolation (accel/decel)
extern float		UTIL_SplineFraction( float value, float scale );

// Search for water transition along a vertical line
extern float		UTIL_WaterLevel( const VectorHL1 &position, float minz, float maxz );
extern void			UTIL_BubblesHL1( VectorHL1 mins, VectorHL1 maxs, int count );
extern void			UTIL_BubbleTrailHL1( VectorHL1 from, VectorHL1 to, int count );

// allows precacheing of other entities
extern void			UTIL_PrecacheOtherHL1( const char *szClassname );

// prints a message to each client
extern void			UTIL_ClientPrintAllHL1( int msg_dest, const char *msg_name, const char *param1 = NULL, const char *param2 = NULL, const char *param3 = NULL, const char *param4 = NULL );
inline void			UTIL_CenterPrintAllHL1( const char *msg_name, const char *param1 = NULL, const char *param2 = NULL, const char *param3 = NULL, const char *param4 = NULL ) 
{
	UTIL_ClientPrintAllHL1( HUD_PRINTCENTER, msg_name, param1, param2, param3, param4 );
}

class CBasePlayerItemHL1;
class CBasePlayerHL1;
extern BOOL UTIL_GetNextBestWeapon( CBasePlayerHL1 *pPlayer, CBasePlayerItemHL1 *pCurrentWeapon );

// prints messages through the HUD
extern void ClientPrint( entvars_t *client, int msg_dest, const char *msg_name, const char *param1 = NULL, const char *param2 = NULL, const char *param3 = NULL, const char *param4 = NULL );

// prints a message to the HUD say (chat)
extern void			UTIL_SayText( const char *pText, CBaseEntityHL1 *pEntity );
extern void			UTIL_SayTextAll( const char *pText, CBaseEntityHL1 *pEntity );


typedef struct hudtextparms_s
{
	float		x;
	float		y;
	int			effect;
	byte		r1, g1, b1, a1;
	byte		r2, g2, b2, a2;
	float		fadeinTime;
	float		fadeoutTime;
	float		holdTime;
	float		fxTime;
	int			channel;
} hudtextparms_t;

// prints as transparent 'title' to the HUD
extern void			UTIL_HudMessageAllHL1( const hudtextparms_t &textparms, const char *pMessage );
extern void			UTIL_HudMessageHL1( CBaseEntityHL1 *pEntity, const hudtextparms_t &textparms, const char *pMessage );

// for handy use with ClientPrint params
extern char *UTIL_dtos1( int d );
extern char *UTIL_dtos2( int d );
extern char *UTIL_dtos3( int d );
extern char *UTIL_dtos4( int d );

// Writes message to console with timestamp and FragLog header.
extern void			UTIL_LogPrintf( char *fmt, ... );

// Sorta like FInViewCone, but for nonmonsters. 
extern float UTIL_DotPointsHL1 ( const VectorHL1 &vecSrc, const VectorHL1 &vecCheck, const VectorHL1 &vecDir );

extern void UTIL_StripToken( const char *pKey, char *pDest );// for redundant keynames

// Misc functions
extern void SetMovedir(entvars_t* pev);
extern VectorHL1 VecBModelOriginHL1( entvars_t* pevBModel );
extern int BuildChangeList( LEVELLIST *pLevelList, int maxList );

//
// How did I ever live without ASSERT?
//
#ifdef	DEBUG
void DBG_AssertFunction(BOOL fExpr, const char* szExpr, const char* szFile, int szLine, const char* szMessage);
#define ASSERT(f)		DBG_AssertFunction(f, #f, __FILE__, __LINE__, NULL)
#define ASSERTSZ(f, sz)	DBG_AssertFunction(f, #f, __FILE__, __LINE__, sz)
#else	// !DEBUG
#define ASSERT(f)
#define ASSERTSZ(f, sz)
#endif	// !DEBUG


extern DLL_GLOBAL const VectorHL1 g_vecZero;

//
// Constants that were used only by QC (maybe not used at all now)
//
// Un-comment only as needed
//
#define LANGUAGE_ENGLISH				0
#define LANGUAGE_GERMAN					1
#define LANGUAGE_FRENCH					2
#define LANGUAGE_BRITISH				3

extern DLL_GLOBAL int			g_LanguageHL1;

#define AMBIENT_SOUND_STATIC			0	// medium radius attenuation
#define AMBIENT_SOUND_EVERYWHERE		1
#define AMBIENT_SOUND_SMALLRADIUS		2
#define AMBIENT_SOUND_MEDIUMRADIUS		4
#define AMBIENT_SOUND_LARGERADIUS		8
#define AMBIENT_SOUND_START_SILENT		16
#define AMBIENT_SOUND_NOT_LOOPING		32

#define SPEAKER_START_SILENT			1	// wait for trigger 'on' to start announcements

#define SND_SPAWNING_HL1		(1<<8)		// duplicated in protocol.h we're spawing, used in some cases for ambients 
#define SND_STOP_HL1			(1<<5)		// duplicated in protocol.h stop sound
#define SND_CHANGE_VOL_HL1		(1<<6)		// duplicated in protocol.h change sound vol
#define SND_CHANGE_PITCH_HL1	(1<<7)		// duplicated in protocol.h change sound pitch

#define	LFO_SQUARE			1
#define LFO_TRIANGLE		2
#define LFO_RANDOM			3

// func_rotating
#define SF_BRUSH_ROTATE_Y_AXIS		0
#define SF_BRUSH_ROTATE_INSTANT		1
#define SF_BRUSH_ROTATE_BACKWARDS	2
#define SF_BRUSH_ROTATE_Z_AXIS		4
#define SF_BRUSH_ROTATE_X_AXIS		8
#define SF_PENDULUM_AUTO_RETURN		16
#define	SF_PENDULUM_PASSABLE		32


#define SF_BRUSH_ROTATE_SMALLRADIUS	128
#define SF_BRUSH_ROTATE_MEDIUMRADIUS 256
#define SF_BRUSH_ROTATE_LARGERADIUS 512

#define PUSH_BLOCK_ONLY_X	1
#define PUSH_BLOCK_ONLY_Y	2

#define VEC_HULL_MIN_HL1		VectorHL1(-16, -16, -36)
#define VEC_HULL_MAX_HL1		VectorHL1( 16,  16,  36)
#define VEC_HUMAN_HULL_MIN_HL1	VectorHL1( -16, -16, 0 )
#define VEC_HUMAN_HULL_MAX_HL1	VectorHL1( 16, 16, 72 )
#define VEC_HUMAN_HULL_DUCK_HL1	VectorHL1( 16, 16, 36 )

#define VEC_VIEW_HL1			VectorHL1( 0, 0, 28 )

#define VEC_DUCK_HULL_MIN_HL1	VectorHL1(-16, -16, -18 )
#define VEC_DUCK_HULL_MAX_HL1	VectorHL1( 16,  16,  18)
#define VEC_DUCK_VIEW_HL1		VectorHL1( 0, 0, 12 )

#define SVC_TEMPENTITY		23
#define SVC_INTERMISSION	30
#define SVC_CDTRACK			32
#define SVC_WEAPONANIM		35
#define SVC_ROOMTYPE		37
#define	SVC_DIRECTOR		51



// triggers
#define	SF_TRIGGER_ALLOWMONSTERS	1// monsters allowed to fire this trigger
#define	SF_TRIGGER_NOCLIENTS		2// players not allowed to fire this trigger
#define SF_TRIGGER_PUSHABLES		4// only pushables can fire this trigger

// func breakable
#define SF_BREAK_TRIGGER_ONLY	1// may only be broken by trigger
#define	SF_BREAK_TOUCH			2// can be 'crashed through' by running player (plate glass)
#define SF_BREAK_PRESSURE		4// can be broken by a player standing on it
#define SF_BREAK_CROWBAR		256// instant break if hit with crowbar

// func_pushable (it's also func_breakable, so don't collide with those flags)
#define SF_PUSH_BREAKABLE		128

#define SF_LIGHT_START_OFF		1

#define SPAWNFLAG_NOMESSAGE	1
#define SPAWNFLAG_NOTOUCH	1
#define SPAWNFLAG_DROIDONLY	4

#define SPAWNFLAG_USEONLY	1		// can't be touched, must be used (buttons)

#define TELE_PLAYER_ONLY	1
#define TELE_SILENT			2

#define SF_TRIG_PUSH_ONCE_HL1		1


// Sound Utilities

// sentence groups
#define CBSENTENCENAME_MAX 16
#define CVOXFILESENTENCEMAX		1536		// max number of sentences in game. NOTE: this must match
											// CVOXFILESENTENCEMAX in engine\sound.h!!!

extern char gszallsentencenames[CVOXFILESENTENCEMAX][CBSENTENCENAME_MAX];
extern int gcallsentences;

int USENTENCEG_Pick(int isentenceg, char *szfound);
int USENTENCEG_PickSequential(int isentenceg, char *szfound, int ipick, int freset);
void USENTENCEG_InitLRU(unsigned char *plru, int count);

void SENTENCEG_Init();
void SENTENCEG_Stop(edict_t_hl1 *entity, int isentenceg, int ipick);
int SENTENCEG_PlayRndI(edict_t_hl1 *entity, int isentenceg, float volume, float attenuation, int flags, int pitch);
int SENTENCEG_PlayRndSz(edict_t_hl1 *entity, const char *szrootname, float volume, float attenuation, int flags, int pitch);
int SENTENCEG_PlaySequentialSz(edict_t_hl1 *entity, const char *szrootname, float volume, float attenuation, int flags, int pitch, int ipick, int freset);
int SENTENCEG_GetIndex(const char *szrootname);
int SENTENCEG_Lookup(const char *sample, char *sentencenum);

void TEXTURETYPE_Init();
char TEXTURETYPE_Find(char *name);
float TEXTURETYPE_PlaySound(TraceResult *ptr,  VectorHL1 vecSrc, VectorHL1 vecEnd, int iBulletType);

// NOTE: use EMIT_SOUND_DYN to set the pitch of a sound. Pitch of 100
// is no pitch shift.  Pitch > 100 up to 255 is a higher pitch, pitch < 100
// down to 1 is a lower pitch.   150 to 70 is the realistic range.
// EMIT_SOUND_DYN with pitch != 100 should be used sparingly, as it's not quite as
// fast as EMIT_SOUND (the pitchshift mixer is not native coded).

void EMIT_SOUND_DYN(edict_t_hl1 *entity, int channel, const char *sample, float volume, float attenuation,
						   int flags, int pitch);


inline void EMIT_SOUND(edict_t_hl1 *entity, int channel, const char *sample, float volume, float attenuation)
{
	EMIT_SOUND_DYN(entity, channel, sample, volume, attenuation, 0, PITCH_NORM);
}

inline void STOP_SOUND(edict_t_hl1 *entity, int channel, const char *sample)
{
	EMIT_SOUND_DYN(entity, channel, sample, 0, 0, SND_STOP, PITCH_NORM);
}

void EMIT_SOUND_SUIT(edict_t_hl1 *entity, const char *sample);
void EMIT_GROUPID_SUIT(edict_t_hl1 *entity, int isentenceg);
void EMIT_GROUPNAME_SUIT(edict_t_hl1 *entity, const char *groupname);

#define PRECACHE_SOUND_ARRAY( a ) \
	{ for (int i = 0; i < ARRAYSIZE( a ); i++ ) PRECACHE_SOUND((char *) a [i]); }

#define EMIT_SOUND_ARRAY_DYN( chan, array ) \
	EMIT_SOUND_DYN ( ENT(pev), chan , array [ RANDOM_LONG(0,ARRAYSIZE( array )-1) ], 1.0, ATTN_NORM, 0, RANDOM_LONG(95,105) ); 

#define RANDOM_SOUND_ARRAY( array ) (array) [ RANDOM_LONG(0,ARRAYSIZE( (array) )-1) ]

#define PLAYBACK_EVENT( flags, who, index ) PLAYBACK_EVENT_FULL( flags, who, index, 0, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 0, 0, 0, 0 );
#define PLAYBACK_EVENT_DELAY( flags, who, index, delay ) PLAYBACK_EVENT_FULL( flags, who, index, delay, (float *)&g_vecZero, (float *)&g_vecZero, 0.0, 0.0, 0, 0, 0, 0 );

#define GROUP_OP_AND	0
#define GROUP_OP_NAND	1

extern int g_groupmask;
extern int g_groupop;

class UTIL_GroupTrace
{
public:
	UTIL_GroupTrace( int groupmask, int op );
	~UTIL_GroupTrace( void );

private:
	int m_oldgroupmask, m_oldgroupop;
};

void UTIL_SetGroupTrace( int groupmask, int op );
void UTIL_UnsetGroupTrace( void );

int UTIL_SharedRandomLong( unsigned int seed, int low, int high );
float UTIL_SharedRandomFloat( unsigned int seed, float low, float high );

float UTIL_WeaponTimeBase( void );

#endif // UTIL_H
