//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#ifndef ENGINECALLBACK_H
#define ENGINECALLBACK_H

#ifndef EIFACE_H
#include "eiface.h"
#endif

class IFileSystem;				// include filesystem.h
class IEngineSound;				// include engine/IEngineSound.h
class IVEngineServer;			
class IVoiceServer;
class IStaticPropMgrServer;
class ISpatialPartition;
class IVModelInfo;
class IEngineTrace;
class IGameEventManager2;
class IVDebugOverlay;
class IDataCache;
class IMDLCache;
class IServerEngineTools;
class IXboxSystem;
class CSteamAPIContext;
class CSteamGameServerAPIContext;

extern IVEngineServer			*engine;
extern IVoiceServer				*g_pVoiceServer;
extern IFileSystem				*filesystem;
extern IStaticPropMgrServer		*staticpropmgr;
extern ISpatialPartition		*partition;
extern IEngineSound				*enginesound;
extern IVModelInfo				*modelinfo;
extern IEngineTrace				*enginetrace;
extern IGameEventManager2		*gameeventmanager;
extern IVDebugOverlay			*debugoverlay;
extern IDataCache				*datacache;
extern IMDLCache				*mdlcache;
extern IServerEngineTools		*serverenginetools;
extern IXboxSystem				*xboxsystem; // 360 only
extern CSteamAPIContext			*steamapicontext; // available on game clients
extern CSteamGameServerAPIContext *steamgameserverapicontext; //available on game servers



//-----------------------------------------------------------------------------
// Precaches a material
//-----------------------------------------------------------------------------
void PrecacheMaterial( const char *pMaterialName );

//-----------------------------------------------------------------------------
// Converts a previously precached material into an index
//-----------------------------------------------------------------------------
int GetMaterialIndex( const char *pMaterialName );

//-----------------------------------------------------------------------------
// Converts a previously precached material index into a string
//-----------------------------------------------------------------------------
const char *GetMaterialNameFromIndex( int nMaterialIndex );


//-----------------------------------------------------------------------------
// Precache-related methods for particle systems
//-----------------------------------------------------------------------------
void PrecacheParticleSystem( const char *pParticleSystemName );
int GetParticleSystemIndex( const char *pParticleSystemName );
const char *GetParticleSystemNameFromIndex( int nIndex );


class IRecipientFilter;
void EntityMessageBegin( CBaseEntity * entity, bool reliable = false );
void UserMessageBegin( IRecipientFilter& filter, const char *messagename );
void MessageEnd( void );

// bytewise
void MessageWriteByte( int iValue);
void MessageWriteChar( int iValue);
void MessageWriteShort( int iValue);
void MessageWriteWord( int iValue );
void MessageWriteLong( int iValue);
void MessageWriteFloat( float flValue);
void MessageWriteAngle( float flValue);
void MessageWriteCoord( float flValue);
void MessageWriteVec3Coord( const Vector& rgflValue);
void MessageWriteVec3Normal( const Vector& rgflValue);
void MessageWriteAngles( const QAngle& rgflValue);
void MessageWriteString( const char *sz );
void MessageWriteEntity( int iValue);
void MessageWriteEHandle( CBaseEntity *pEntity ); //encoded as a long


// bitwise
void MessageWriteBool( bool bValue );
void MessageWriteUBitLong( unsigned int data, int numbits );
void MessageWriteSBitLong( int data, int numbits );
void MessageWriteBits( const void *pIn, int nBits );

#ifndef NO_STEAM

/// Returns Steam ID, given player index.   Returns an invalid SteamID upon
/// failure
extern CSteamID GetSteamIDForPlayerIndex( int iPlayerIndex );

#endif


// Bytewise
#define WRITE_BYTE		(MessageWriteByte)
#define WRITE_CHAR		(MessageWriteChar)
#define WRITE_SHORT		(MessageWriteShort)
#define WRITE_WORD		(MessageWriteWord)
#define WRITE_LONG		(MessageWriteLong)
#define WRITE_FLOAT		(MessageWriteFloat)
#define WRITE_ANGLE		(MessageWriteAngle)
#define WRITE_COORD		(MessageWriteCoord)
#define WRITE_VEC3COORD	(MessageWriteVec3Coord)
#define WRITE_VEC3NORMAL (MessageWriteVec3Normal)
#define WRITE_ANGLES	(MessageWriteAngles)
#define WRITE_STRING	(MessageWriteString)
#define WRITE_ENTITY	(MessageWriteEntity)
#define WRITE_EHANDLE	(MessageWriteEHandle)

// Bitwise
#define WRITE_BOOL		(MessageWriteBool)
#define WRITE_UBITLONG	(MessageWriteUBitLong)
#define WRITE_SBITLONG	(MessageWriteSBitLong)
#define WRITE_BITS		(MessageWriteBits)

// HL1

#include "event_flags.h"

// Must be provided by user of this code
extern enginefuncs_t g_engfuncsHL1;

// The actual engine callbacks
#define GETPLAYERUSERID (*g_engfuncsHL1.pfnGetPlayerUserId)
#define PRECACHE_MODEL	(*g_engfuncsHL1.pfnPrecacheModel)
#define PRECACHE_SOUND	(*g_engfuncsHL1.pfnPrecacheSound)
#define PRECACHE_GENERIC	(*g_engfuncsHL1.pfnPrecacheGeneric)
#define SET_MODEL		(*g_engfuncsHL1.pfnSetModel)
#define MODEL_INDEX		(*g_engfuncsHL1.pfnModelIndex)
#define MODEL_FRAMES	(*g_engfuncsHL1.pfnModelFrames)
#define SET_SIZE		(*g_engfuncsHL1.pfnSetSize)
#define CHANGE_LEVEL	(*g_engfuncsHL1.pfnChangeLevel)
#define GET_SPAWN_PARMS	(*g_engfuncsHL1.pfnGetSpawnParms)
#define SAVE_SPAWN_PARMS (*g_engfuncsHL1.pfnSaveSpawnParms)
#define VEC_TO_YAW		(*g_engfuncsHL1.pfnVecToYaw)
#define VEC_TO_ANGLES	(*g_engfuncsHL1.pfnVecToAngles)
#define MOVE_TO_ORIGIN  (*g_engfuncsHL1.pfnMoveToOrigin)
#define oldCHANGE_YAW		(*g_engfuncsHL1.pfnChangeYaw)
#define CHANGE_PITCH	(*g_engfuncsHL1.pfnChangePitch)
#define MAKE_VECTORS	(*g_engfuncsHL1.pfnMakeVectors)
#define CREATE_ENTITY	(*g_engfuncsHL1.pfnCreateEntity)
#define REMOVE_ENTITY	(*g_engfuncsHL1.pfnRemoveEntity)
#define CREATE_NAMED_ENTITY		(*g_engfuncsHL1.pfnCreateNamedEntity)
#define MAKE_STATIC		(*g_engfuncsHL1.pfnMakeStatic)
#define ENT_IS_ON_FLOOR	(*g_engfuncsHL1.pfnEntIsOnFloor)
#define DROP_TO_FLOOR	(*g_engfuncsHL1.pfnDropToFloor)
#define WALK_MOVE		(*g_engfuncsHL1.pfnWalkMove)
#define SET_ORIGIN		(*g_engfuncsHL1.pfnSetOrigin)
#define EMIT_SOUND_DYN2 (*g_engfuncsHL1.pfnEmitSound)
#define BUILD_SOUND_MSG (*g_engfuncsHL1.pfnBuildSoundMsg)
#define TRACE_LINE		(*g_engfuncsHL1.pfnTraceLine)
#define TRACE_TOSS		(*g_engfuncsHL1.pfnTraceToss)
#define TRACE_MONSTER_HULL		(*g_engfuncsHL1.pfnTraceMonsterHull)
#define TRACE_HULL		(*g_engfuncsHL1.pfnTraceHull)
#define GET_AIM_VECTOR	(*g_engfuncsHL1.pfnGetAimVector)
#define SERVER_COMMAND	(*g_engfuncsHL1.pfnServerCommand)
#define SERVER_EXECUTE	(*g_engfuncsHL1.pfnServerExecute)
#define CLIENT_COMMAND	(*g_engfuncsHL1.pfnClientCommand)
#define PARTICLE_EFFECT	(*g_engfuncsHL1.pfnParticleEffect)
#define LIGHT_STYLE		(*g_engfuncsHL1.pfnLightStyle)
#define DECAL_INDEX		(*g_engfuncsHL1.pfnDecalIndex)
#define POINT_CONTENTS	(*g_engfuncsHL1.pfnPointContents)
#define CRC32_INIT           (*g_engfuncsHL1.pfnCRC32_Init)
#define CRC32_PROCESS_BUFFER (*g_engfuncsHL1.pfnCRC32_ProcessBuffer)
#define CRC32_PROCESS_BYTE   (*g_engfuncsHL1.pfnCRC32_ProcessByte)
#define CRC32_FINAL          (*g_engfuncsHL1.pfnCRC32_Final)
#define RANDOM_LONG		(*g_engfuncsHL1.pfnRandomLong)
#define RANDOM_FLOAT	(*g_engfuncsHL1.pfnRandomFloat)
#define GETPLAYERAUTHID	(*g_engfuncsHL1.pfnGetPlayerAuthId)

inline void MESSAGE_BEGIN( int msg_dest, int msg_type, const float *pOrigin = NULL, edict_t_hl1 *ed = NULL ) {
	(*g_engfuncsHL1.pfnMessageBegin)(msg_dest, msg_type, pOrigin, ed);
}
#define MESSAGE_END		(*g_engfuncsHL1.pfnMessageEnd)
#define WRITE_BYTE_HL1		(*g_engfuncsHL1.pfnWriteByte)
#define WRITE_CHAR_HL1		(*g_engfuncsHL1.pfnWriteChar)
#define WRITE_SHORT_HL1		(*g_engfuncsHL1.pfnWriteShort)
#define WRITE_LONG_HL1		(*g_engfuncsHL1.pfnWriteLong)
#define WRITE_ANGLE_HL1		(*g_engfuncsHL1.pfnWriteAngle)
#define WRITE_COORD_HL1		(*g_engfuncsHL1.pfnWriteCoord)
#define WRITE_STRING_HL1	(*g_engfuncsHL1.pfnWriteString)
#define WRITE_ENTITY_HL1	(*g_engfuncsHL1.pfnWriteEntity)
#define CVAR_REGISTER	(*g_engfuncsHL1.pfnCVarRegister)
#define CVAR_GET_FLOAT	(*g_engfuncsHL1.pfnCVarGetFloat)
#define CVAR_GET_STRING	(*g_engfuncsHL1.pfnCVarGetString)
#define CVAR_SET_FLOAT	(*g_engfuncsHL1.pfnCVarSetFloat)
#define CVAR_SET_STRING	(*g_engfuncsHL1.pfnCVarSetString)
#define CVAR_GET_POINTER (*g_engfuncsHL1.pfnCVarGetPointer)
#define ALERT			(*g_engfuncsHL1.pfnAlertMessage)
#define ENGINE_FPRINTF	(*g_engfuncsHL1.pfnEngineFprintf)
#define ALLOC_PRIVATE	(*g_engfuncsHL1.pfnPvAllocEntPrivateData)
inline void *GET_PRIVATE( edict_t_hl1 *pent )
{
	if ( pent )
		return pent->pvPrivateData;
	return NULL;
}

#define FREE_PRIVATE	(*g_engfuncsHL1.pfnFreeEntPrivateData)
//#define STRING			(*g_engfuncsHL1.pfnSzFromIndex)
#define ALLOC_STRING	(*g_engfuncsHL1.pfnAllocString)
#define FIND_ENTITY_BY_STRING	(*g_engfuncsHL1.pfnFindEntityByString)
#define GETENTITYILLUM	(*g_engfuncsHL1.pfnGetEntityIllum)
#define FIND_ENTITY_IN_SPHERE		(*g_engfuncsHL1.pfnFindEntityInSphere)
#define FIND_CLIENT_IN_PVS			(*g_engfuncsHL1.pfnFindClientInPVS)
#define EMIT_AMBIENT_SOUND			(*g_engfuncsHL1.pfnEmitAmbientSound)
#define GET_MODEL_PTR				(*g_engfuncsHL1.pfnGetModelPtr)
#define REG_USER_MSG				(*g_engfuncsHL1.pfnRegUserMsg)
#define GET_BONE_POSITION			(*g_engfuncsHL1.pfnGetBonePosition)
#define FUNCTION_FROM_NAME			(*g_engfuncsHL1.pfnFunctionFromName)
#define NAME_FOR_FUNCTION			(*g_engfuncsHL1.pfnNameForFunction)
#define TRACE_TEXTURE				(*g_engfuncsHL1.pfnTraceTexture)
#define CLIENT_PRINTF				(*g_engfuncsHL1.pfnClientPrintf)
#define CMD_ARGS					(*g_engfuncsHL1.pfnCmd_Args)
#define CMD_ARGC					(*g_engfuncsHL1.pfnCmd_Argc)
#define CMD_ARGV					(*g_engfuncsHL1.pfnCmd_Argv)
#define GET_ATTACHMENT			(*g_engfuncsHL1.pfnGetAttachment)
#define SET_VIEW				(*g_engfuncsHL1.pfnSetView)
#define SET_CROSSHAIRANGLE		(*g_engfuncsHL1.pfnCrosshairAngle)
#define LOAD_FILE_FOR_ME		(*g_engfuncsHL1.pfnLoadFileForMe)
#define FREE_FILE				(*g_engfuncsHL1.pfnFreeFile)
#define COMPARE_FILE_TIME		(*g_engfuncsHL1.pfnCompareFileTime)
#define GET_GAME_DIR			(*g_engfuncsHL1.pfnGetGameDir)
#define IS_MAP_VALID			(*g_engfuncsHL1.pfnIsMapValid)
#define NUMBER_OF_ENTITIES		(*g_engfuncsHL1.pfnNumberOfEntities)
#define IS_DEDICATED_SERVER		(*g_engfuncsHL1.pfnIsDedicatedServer)

#define PRECACHE_EVENT			(*g_engfuncsHL1.pfnPrecacheEvent)
#define PLAYBACK_EVENT_FULL		(*g_engfuncsHL1.pfnPlaybackEvent)

#define ENGINE_SET_PVS			(*g_engfuncsHL1.pfnSetFatPVS)
#define ENGINE_SET_PAS			(*g_engfuncsHL1.pfnSetFatPAS)

#define ENGINE_CHECK_VISIBILITY (*g_engfuncsHL1.pfnCheckVisibility)

#define DELTA_SET				( *g_engfuncsHL1.pfnDeltaSetField )
#define DELTA_UNSET				( *g_engfuncsHL1.pfnDeltaUnsetField )
#define DELTA_ADDENCODER		( *g_engfuncsHL1.pfnDeltaAddEncoder )
#define ENGINE_CURRENT_PLAYER   ( *g_engfuncsHL1.pfnGetCurrentPlayer )

#define	ENGINE_CANSKIP			( *g_engfuncsHL1.pfnCanSkipPlayer )

#define DELTA_FINDFIELD			( *g_engfuncsHL1.pfnDeltaFindField )
#define DELTA_SETBYINDEX		( *g_engfuncsHL1.pfnDeltaSetFieldByIndex )
#define DELTA_UNSETBYINDEX		( *g_engfuncsHL1.pfnDeltaUnsetFieldByIndex )

#define ENGINE_GETPHYSINFO		( *g_engfuncsHL1.pfnGetPhysicsInfoString )

#define ENGINE_SETGROUPMASK		( *g_engfuncsHL1.pfnSetGroupMask )

#define ENGINE_INSTANCE_BASELINE ( *g_engfuncsHL1.pfnCreateInstancedBaseline )

#define ENGINE_FORCE_UNMODIFIED	( *g_engfuncsHL1.pfnForceUnmodified )

#define PLAYER_CNX_STATS		( *g_engfuncsHL1.pfnGetPlayerStats )

#endif		//ENGINECALLBACK_H
