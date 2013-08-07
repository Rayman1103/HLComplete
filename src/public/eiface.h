//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef EIFACE_H
#define EIFACE_H

#ifdef _WIN32
#pragma once
#endif

#include "convar.h"
#include "icvar.h"
#include "edict.h"
#include "mathlib/vplane.h"
#include "iserverentity.h"
#include "engine/ivmodelinfo.h"
#include "soundflags.h"
#include "bitvec.h"
#include "engine/iserverplugin.h"
#include "tier1/bitbuf.h"

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
class	SendTable;
class	ServerClass;
class	IMoveHelper;
struct  Ray_t;
class	CGameTrace;
typedef	CGameTrace trace_t;
struct	typedescription_t;
class	CSaveRestoreData;
struct	datamap_t;
class	SendTable;
class	ServerClass;
class	IMoveHelper;
struct  Ray_t;
struct	studiohdr_t;
class	CBaseEntity;
class	CRestore;
class	CSave;
class	variant_t;
struct	vcollide_t;
class	IRecipientFilter;
class	CBaseEntity;
class	ITraceFilter;
struct	client_textmessage_t;
class	INetChannelInfo;
class	ISpatialPartition;
class IScratchPad3D;
class CStandardSendProxies;
class IAchievementMgr;
class CGamestatsData;
class CSteamID;
class IReplayFactory;
class IReplaySystem;

typedef struct player_info_s player_info_t;

//-----------------------------------------------------------------------------
// defines
//-----------------------------------------------------------------------------

#ifdef _WIN32
#define DLLEXPORT __stdcall
#else
#define DLLEXPORT /* */
#endif

#define INTERFACEVERSION_VENGINESERVER	"VEngineServer021"

struct bbox_t
{
	Vector mins;
	Vector maxs;
};

//-----------------------------------------------------------------------------
// Purpose: Interface the engine exposes to the game DLL
//-----------------------------------------------------------------------------
abstract_class IVEngineServer
{
public:
	// Tell engine to change level ( "changelevel s1\n" or "changelevel2 s1 s2\n" )
	virtual void		ChangeLevel( const char *s1, const char *s2 ) = 0;
	
	// Ask engine whether the specified map is a valid map file (exists and has valid version number).
	virtual int			IsMapValid( const char *filename ) = 0;
	
	// Is this a dedicated server?
	virtual bool		IsDedicatedServer( void ) = 0;
	
	// Is in Hammer editing mode?
	virtual int			IsInEditMode( void ) = 0;
	
	// Add to the server/client lookup/precache table, the specified string is given a unique index
	// NOTE: The indices for PrecacheModel are 1 based
	//  a 0 returned from those methods indicates the model or sound was not correctly precached
	// However, generic and decal are 0 based
	// If preload is specified, the file is loaded into the server/client's cache memory before level startup, otherwise
	//  it'll only load when actually used (which can cause a disk i/o hitch if it occurs during play of a level).
	virtual int			PrecacheModel( const char *s, bool preload = false ) = 0;
	virtual int			PrecacheSentenceFile( const char *s, bool preload = false ) = 0;
	virtual int			PrecacheDecal( const char *name, bool preload = false ) = 0;
	virtual int			PrecacheGeneric( const char *s, bool preload = false ) = 0;

	// Check's if the name is precached, but doesn't actually precache the name if not...
	virtual bool		IsModelPrecached( char const *s ) const = 0;
	virtual bool		IsDecalPrecached( char const *s ) const = 0;
	virtual bool		IsGenericPrecached( char const *s ) const = 0;

	// Note that sounds are precached using the IEngineSound interface

	// Special purpose PVS checking
	// Get the cluster # for the specified position
	virtual int			GetClusterForOrigin( const Vector &org ) = 0;
	// Get the PVS bits for a specified cluster and copy the bits into outputpvs.  Returns the number of bytes needed to pack the PVS
	virtual int			GetPVSForCluster( int cluster, int outputpvslength, unsigned char *outputpvs ) = 0;
	// Check whether the specified origin is inside the specified PVS
	virtual bool		CheckOriginInPVS( const Vector &org, const unsigned char *checkpvs, int checkpvssize ) = 0;
	// Check whether the specified worldspace bounding box is inside the specified PVS
	virtual bool		CheckBoxInPVS( const Vector &mins, const Vector &maxs, const unsigned char *checkpvs, int checkpvssize ) = 0;

	// Returns the server assigned userid for this player.  Useful for logging frags, etc.  
	//  returns -1 if the edict couldn't be found in the list of players.
	virtual int			GetPlayerUserId( const edict_t *e ) = 0; 
	virtual const char	*GetPlayerNetworkIDString( const edict_t *e ) = 0;

	// Return the current number of used edict slots
	virtual int			GetEntityCount( void ) = 0;
	// Given an edict, returns the entity index
	virtual int			IndexOfEdict( const edict_t *pEdict ) = 0;
	// Given and entity index, returns the corresponding edict pointer
	virtual edict_t		*PEntityOfEntIndex( int iEntIndex ) = 0;
	
	// Get stats info interface for a client netchannel
	virtual INetChannelInfo* GetPlayerNetInfo( int playerIndex ) = 0;
	
	// Allocate space for string and return index/offset of string in global string list
	// If iForceEdictIndex is not -1, then it will return the edict with that index. If that edict index
	// is already used, it'll return null.
	virtual edict_t		*CreateEdict( int iForceEdictIndex = -1 ) = 0;
	// Remove the specified edict and place back into the free edict list
	virtual void		RemoveEdict( edict_t *e ) = 0;
	
	// Memory allocation for entity class data
	virtual void		*PvAllocEntPrivateData( long cb ) = 0;
	virtual void		FreeEntPrivateData( void *pEntity ) = 0;

	// Save/restore uses a special memory allocator (which zeroes newly allocated memory, etc.)
	virtual void		*SaveAllocMemory( size_t num, size_t size ) = 0;
	virtual void		SaveFreeMemory( void *pSaveMem ) = 0;
	
	// Emit an ambient sound associated with the specified entity
	virtual void		EmitAmbientSound( int entindex, const Vector &pos, const char *samp, float vol, soundlevel_t soundlevel, int fFlags, int pitch, float delay = 0.0f ) = 0;

	// Fade out the client's volume level toward silence (or fadePercent)
	virtual void        FadeClientVolume( const edict_t *pEdict, float fadePercent, float fadeOutSeconds, float holdTime, float fadeInSeconds ) = 0;
	
	// Sentences / sentence groups
	virtual int			SentenceGroupPick( int groupIndex, char *name, int nameBufLen ) = 0;
	virtual int			SentenceGroupPickSequential( int groupIndex, char *name, int nameBufLen, int sentenceIndex, int reset ) = 0;
	virtual int			SentenceIndexFromName( const char *pSentenceName ) = 0;
	virtual const char *SentenceNameFromIndex( int sentenceIndex ) = 0;
	virtual int			SentenceGroupIndexFromName( const char *pGroupName ) = 0;
	virtual const char *SentenceGroupNameFromIndex( int groupIndex ) = 0;
	virtual float		SentenceLength( int sentenceIndex ) = 0;

	// Issue a command to the command parser as if it was typed at the server console.	
	virtual void		ServerCommand( const char *str ) = 0;
	// Execute any commands currently in the command parser immediately (instead of once per frame)
	virtual void		ServerExecute( void ) = 0;
	// Issue the specified command to the specified client (mimics that client typing the command at the console).
	virtual void		ClientCommand( edict_t *pEdict, PRINTF_FORMAT_STRING const char *szFmt, ... ) = 0;

	// Set the lightstyle to the specified value and network the change to any connected clients.  Note that val must not 
	//  change place in memory (use MAKE_STRING) for anything that's not compiled into your mod.
	virtual void		LightStyle( int style, const char *val ) = 0;

	// Project a static decal onto the specified entity / model (for level placed decals in the .bsp)
	virtual void		StaticDecal( const Vector &originInEntitySpace, int decalIndex, int entityIndex, int modelIndex, bool lowpriority ) = 0;
	
	// Given the current PVS(or PAS) and origin, determine which players should hear/receive the message
	virtual void		Message_DetermineMulticastRecipients( bool usepas, const Vector& origin, CBitVec< ABSOLUTE_PLAYER_LIMIT >& playerbits ) = 0;

	// Begin a message from a server side entity to its client side counterpart (func_breakable glass, e.g.)
	virtual bf_write	*EntityMessageBegin( int ent_index, ServerClass * ent_class, bool reliable ) = 0;
	// Begin a usermessage from the server to the client .dll
	virtual bf_write	*UserMessageBegin( IRecipientFilter *filter, int msg_type ) = 0;
	// Finish the Entity or UserMessage and dispatch to network layer
	virtual void		MessageEnd( void ) = 0;

	// Print szMsg to the client console.
	virtual void		ClientPrintf( edict_t *pEdict, const char *szMsg ) = 0;

	// SINGLE PLAYER/LISTEN SERVER ONLY (just matching the client .dll api for this)
	// Prints the formatted string to the notification area of the screen ( down the right hand edge
	//  numbered lines starting at position 0
	virtual void		Con_NPrintf( int pos, PRINTF_FORMAT_STRING const char *fmt, ... ) = 0;
	// SINGLE PLAYER/LISTEN SERVER ONLY(just matching the client .dll api for this)
	// Similar to Con_NPrintf, but allows specifying custom text color and duration information
	virtual void		Con_NXPrintf( const struct con_nprint_s *info, PRINTF_FORMAT_STRING const char *fmt, ... ) = 0;

	// Change a specified player's "view entity" (i.e., use the view entity position/orientation for rendering the client view)
	virtual void		SetView( const edict_t *pClient, const edict_t *pViewent ) = 0;

	// Get a high precision timer for doing profiling work
	virtual float		Time( void ) = 0;

	// Set the player's crosshair angle
	virtual void		CrosshairAngle( const edict_t *pClient, float pitch, float yaw ) = 0;

	// Get the current game directory (hl2, tf2, hl1, cstrike, etc.)
	virtual void        GetGameDir( char *szGetGameDir, int maxlength ) = 0;

	// Used by AI node graph code to determine if .bsp and .ain files are out of date
	virtual int 		CompareFileTime( const char *filename1, const char *filename2, int *iCompare ) = 0;

	// Locks/unlocks the network string tables (.e.g, when adding bots to server, this needs to happen).
	// Be sure to reset the lock after executing your code!!!
	virtual bool		LockNetworkStringTables( bool lock ) = 0;

	// Create a bot with the given name.  Returns NULL if fake client can't be created
	virtual edict_t		*CreateFakeClient( const char *netname ) = 0;

	// Get a convar keyvalue for s specified client
	virtual const char	*GetClientConVarValue( int clientIndex, const char *name ) = 0;
	
	// Parse a token from a file
	virtual const char	*ParseFile( const char *data, char *token, int maxlen ) = 0;
	// Copies a file
	virtual bool		CopyFile( const char *source, const char *destination ) = 0;

	// Reset the pvs, pvssize is the size in bytes of the buffer pointed to by pvs.
	// This should be called right before any calls to AddOriginToPVS
	virtual void		ResetPVS( byte *pvs, int pvssize ) = 0;
	// Merge the pvs bits into the current accumulated pvs based on the specified origin ( not that each pvs origin has an 8 world unit fudge factor )
	virtual void		AddOriginToPVS( const Vector &origin ) = 0;
	
	// Mark a specified area portal as open/closed.
	// Use SetAreaPortalStates if you want to set a bunch of them at a time.
	virtual void		SetAreaPortalState( int portalNumber, int isOpen ) = 0;
	
	// Queue a temp entity for transmission
	virtual void		PlaybackTempEntity( IRecipientFilter& filter, float delay, const void *pSender, const SendTable *pST, int classID  ) = 0;
	// Given a node number and the specified PVS, return with the node is in the PVS
	virtual int			CheckHeadnodeVisible( int nodenum, const byte *pvs, int vissize ) = 0;
	// Using area bits, cheeck whether area1 flows into area2 and vice versa (depends on area portal state)
	virtual int			CheckAreasConnected( int area1, int area2 ) = 0;
	// Given an origin, determine which area index the origin is within
	virtual int			GetArea( const Vector &origin ) = 0;
	// Get area portal bit set
	virtual void		GetAreaBits( int area, unsigned char *bits, int buflen ) = 0;
	// Given a view origin (which tells us the area to start looking in) and a portal key,
	// fill in the plane that leads out of this area (it points into whatever area it leads to).
	virtual bool		GetAreaPortalPlane( Vector const &vViewOrigin, int portalKey, VPlane *pPlane ) = 0;

	// Save/restore wrapper - FIXME:  At some point we should move this to it's own interface
	virtual bool		LoadGameState( char const *pMapName, bool createPlayers ) = 0;
	virtual void		LoadAdjacentEnts( const char *pOldLevel, const char *pLandmarkName ) = 0;
	virtual void		ClearSaveDir() = 0;

	// Get the pristine map entity lump string.  (e.g., used by CS to reload the map entities when restarting a round.)
	virtual const char*	GetMapEntitiesString() = 0;

	// Text message system -- lookup the text message of the specified name
	virtual client_textmessage_t *TextMessageGet( const char *pName ) = 0;

	// Print a message to the server log file
	virtual void		LogPrint( const char *msg ) = 0;

	// Builds PVS information for an entity
	virtual void		BuildEntityClusterList( edict_t *pEdict, PVSInfo_t *pPVSInfo ) = 0;

	// A solid entity moved, update spatial partition
	virtual void SolidMoved( edict_t *pSolidEnt, ICollideable *pSolidCollide, const Vector* pPrevAbsOrigin, bool testSurroundingBoundsOnly ) = 0;
	// A trigger entity moved, update spatial partition
	virtual void TriggerMoved( edict_t *pTriggerEnt, bool testSurroundingBoundsOnly ) = 0;
	
	// Create/destroy a custom spatial partition
	virtual ISpatialPartition *CreateSpatialPartition( const Vector& worldmin, const Vector& worldmax ) = 0;
	virtual void 		DestroySpatialPartition( ISpatialPartition * ) = 0;

	// Draw the brush geometry in the map into the scratch pad.
	// Flags is currently unused.
	virtual void		DrawMapToScratchPad( IScratchPad3D *pPad, unsigned long iFlags ) = 0;

	// This returns which entities, to the best of the server's knowledge, the client currently knows about.
	// This is really which entities were in the snapshot that this client last acked.
	// This returns a bit vector with one bit for each entity.
	//
	// USE WITH CARE. Whatever tick the client is really currently on is subject to timing and
	// ordering differences, so you should account for about a quarter-second discrepancy in here.
	// Also, this will return NULL if the client doesn't exist or if this client hasn't acked any frames yet.
	// 
	// iClientIndex is the CLIENT index, so if you use pPlayer->entindex(), subtract 1.
	virtual const CBitVec<MAX_EDICTS>* GetEntityTransmitBitsForClient( int iClientIndex ) = 0;
	
	// Is the game paused?
	virtual bool		IsPaused() = 0;
	
	// Marks the filename for consistency checking.  This should be called after precaching the file.
	virtual void		ForceExactFile( const char *s ) = 0;
	virtual void		ForceModelBounds( const char *s, const Vector &mins, const Vector &maxs ) = 0;
	virtual void		ClearSaveDirAfterClientLoad() = 0;

	// Sets a USERINFO client ConVar for a fakeclient
	virtual void		SetFakeClientConVarValue( edict_t *pEntity, const char *cvar, const char *value ) = 0;
	
	// Marks the material (vmt file) for consistency checking.  If the client and server have different
	// contents for the file, the client's vmt can only use the VertexLitGeneric shader, and can only
	// contain $baseTexture and $bumpmap vars.
	virtual void		ForceSimpleMaterial( const char *s ) = 0;

	// Is the engine in Commentary mode?
	virtual int			IsInCommentaryMode( void ) = 0;
	

	// Mark some area portals as open/closed. It's more efficient to use this
	// than a bunch of individual SetAreaPortalState calls.
	virtual void		SetAreaPortalStates( const int *portalNumbers, const int *isOpen, int nPortals ) = 0;

	// Called when relevant edict state flags change.
	virtual void		NotifyEdictFlagsChange( int iEdict ) = 0;
	
	// Only valid during CheckTransmit. Also, only the PVS, networked areas, and
	// m_pTransmitInfo are valid in the returned strucutre.
	virtual const CCheckTransmitInfo* GetPrevCheckTransmitInfo( edict_t *pPlayerEdict ) = 0;
	
	virtual CSharedEdictChangeInfo* GetSharedEdictChangeInfo() = 0;

	// Tells the engine we can immdiately re-use all edict indices
	// even though we may not have waited enough time
	virtual void			AllowImmediateEdictReuse( ) = 0;

	// Returns true if the engine is an internal build. i.e. is using the internal bugreporter.
	virtual bool		IsInternalBuild( void ) = 0;

	virtual IChangeInfoAccessor *GetChangeAccessor( const edict_t *pEdict ) = 0;	

	// Name of most recently load .sav file
	virtual char const *GetMostRecentlyLoadedFileName() = 0;
	virtual char const *GetSaveFileName() = 0;

	// Matchmaking
	virtual void MultiplayerEndGame() = 0;
	virtual void ChangeTeam( const char *pTeamName ) = 0;

	// Cleans up the cluster list
	virtual void CleanUpEntityClusterList( PVSInfo_t *pPVSInfo ) = 0;

	virtual void SetAchievementMgr( IAchievementMgr *pAchievementMgr ) =0;
	virtual IAchievementMgr *GetAchievementMgr() = 0;

	virtual int	GetAppID() = 0;
	
	virtual bool IsLowViolence() = 0;
	
	// Call this to find out the value of a cvar on the client.
	//
	// It is an asynchronous query, and it will call IServerGameDLL::OnQueryCvarValueFinished when
	// the value comes in from the client.
	//
	// Store the return value if you want to match this specific query to the OnQueryCvarValueFinished call.
	// Returns InvalidQueryCvarCookie if the entity is invalid.
	virtual QueryCvarCookie_t StartQueryCvarValue( edict_t *pPlayerEntity, const char *pName ) = 0;

	virtual void InsertServerCommand( const char *str ) = 0;

	// Fill in the player info structure for the specified player index (name, model, etc.)
	virtual bool GetPlayerInfo( int ent_num, player_info_t *pinfo ) = 0;

	// Returns true if this client has been fully authenticated by Steam
	virtual bool IsClientFullyAuthenticated( edict_t *pEdict ) = 0;

	// This makes the host run 1 tick per frame instead of checking the system timer to see how many ticks to run in a certain frame.
	// i.e. it does the same thing timedemo does.
	virtual void SetDedicatedServerBenchmarkMode( bool bBenchmarkMode ) = 0;

	// Methods to set/get a gamestats data container so client & server running in same process can send combined data
	virtual void SetGamestatsData( CGamestatsData *pGamestatsData ) = 0;
	virtual CGamestatsData *GetGamestatsData() = 0;

	// Returns the SteamID of the specified player. It'll be NULL if the player hasn't authenticated yet.
	virtual const CSteamID	*GetClientSteamID( edict_t *pPlayerEdict ) = 0;

	// Returns the SteamID of the game server
	virtual const CSteamID	*GetGameServerSteamID() = 0;

	// Send a client command keyvalues
	// keyvalues are deleted inside the function
	virtual void ClientCommandKeyValues( edict_t *pEdict, KeyValues *pCommand ) = 0;

	// Returns the SteamID of the specified player. It'll be NULL if the player hasn't authenticated yet.
	virtual const CSteamID	*GetClientSteamIDByPlayerIndex( int entnum ) = 0;
	// Gets a list of all clusters' bounds.  Returns total number of clusters.
	virtual int GetClusterCount() = 0;
	virtual int GetAllClusterBounds( bbox_t *pBBoxList, int maxBBox ) = 0;

	// Create a bot with the given name.  Returns NULL if fake client can't be created
	virtual edict_t		*CreateFakeClientEx( const char *netname, bool bReportFakeClient = true ) = 0;

	// Server version from the steam.inf, this will be compared to the GC version
	virtual int GetServerVersion() const = 0;

	// Get sv.GetTime()
	virtual float GetServerTime() const = 0;
};


#define INTERFACEVERSION_SERVERGAMEDLL_VERSION_8	"ServerGameDLL008"
#define INTERFACEVERSION_SERVERGAMEDLL				"ServerGameDLL009"
#define INTERFACEVERSION_SERVERGAMEDLL_INT			9

class IServerGCLobby;

//-----------------------------------------------------------------------------
// Purpose: These are the interfaces that the game .dll exposes to the engine
//-----------------------------------------------------------------------------
abstract_class IServerGameDLL
{
public:
	// Initialize the game (one-time call when the DLL is first loaded )
	// Return false if there is an error during startup.
	virtual bool			DLLInit(	CreateInterfaceFn engineFactory, 
										CreateInterfaceFn physicsFactory, 
										CreateInterfaceFn fileSystemFactory, 
										CGlobalVars *pGlobals) = 0;

	// Setup replay interfaces on the server
	virtual bool			ReplayInit( CreateInterfaceFn fnReplayFactory ) = 0;

	// This is called when a new game is started. (restart, map)
	virtual bool			GameInit( void ) = 0;

	// Called any time a new level is started (after GameInit() also on level transitions within a game)
	virtual bool			LevelInit( char const *pMapName, 
									char const *pMapEntities, char const *pOldLevel, 
									char const *pLandmarkName, bool loadGame, bool background ) = 0;

	// The server is about to activate
	virtual void			ServerActivate( edict_t *pEdictList, int edictCount, int clientMax ) = 0;

	// The server should run physics/think on all edicts
	virtual void			GameFrame( bool simulating ) = 0;

	// Called once per simulation frame on the final tick
	virtual void			PreClientUpdate( bool simulating ) = 0;

	// Called when a level is shutdown (including changing levels)
	virtual void			LevelShutdown( void ) = 0;
	// This is called when a game ends (server disconnect, death, restart, load)
	// NOT on level transitions within a game
	virtual void			GameShutdown( void ) = 0;

	// Called once during DLL shutdown
	virtual void			DLLShutdown( void ) = 0;

	// Get the simulation interval (must be compiled with identical values into both client and game .dll for MOD!!!)
	// Right now this is only requested at server startup time so it can't be changed on the fly, etc.
	virtual float			GetTickInterval( void ) const = 0;

	// Give the list of datatable classes to the engine.  The engine matches class names from here with
	//  edict_t::classname to figure out how to encode a class's data for networking
	virtual ServerClass*	GetAllServerClasses( void ) = 0;

	// Returns string describing current .dll.  e.g., TeamFortress 2, Half-Life 2.  
	//  Hey, it's more descriptive than just the name of the game directory
	virtual const char     *GetGameDescription( void ) = 0;      
	
	// Let the game .dll allocate it's own network/shared string tables
	virtual void			CreateNetworkStringTables( void ) = 0;
	
	// Save/restore system hooks
	virtual CSaveRestoreData  *SaveInit( int size ) = 0;
	virtual void			SaveWriteFields( CSaveRestoreData *, const char *, void *, datamap_t *, typedescription_t *, int ) = 0;
	virtual void			SaveReadFields( CSaveRestoreData *, const char *, void *, datamap_t *, typedescription_t *, int ) = 0;
	virtual void			SaveGlobalState( CSaveRestoreData * ) = 0;
	virtual void			RestoreGlobalState( CSaveRestoreData * ) = 0;
	virtual void			PreSave( CSaveRestoreData * ) = 0;
	virtual void			Save( CSaveRestoreData * ) = 0;
	virtual void			GetSaveComment( char *comment, int maxlength, float flMinutes, float flSeconds, bool bNoTime = false ) = 0;
	virtual void			WriteSaveHeaders( CSaveRestoreData * ) = 0;
	virtual void			ReadRestoreHeaders( CSaveRestoreData * ) = 0;
	virtual void			Restore( CSaveRestoreData *, bool ) = 0;
	virtual bool			IsRestoring() = 0;

	// Returns the number of entities moved across the transition
	virtual int				CreateEntityTransitionList( CSaveRestoreData *, int ) = 0;
	// Build the list of maps adjacent to the current map
	virtual void			BuildAdjacentMapList( void ) = 0;

	// Retrieve info needed for parsing the specified user message
	virtual bool			GetUserMessageInfo( int msg_type, char *name, int maxnamelength, int& size ) = 0;

	// Hand over the StandardSendProxies in the game DLL's module.
	virtual CStandardSendProxies*	GetStandardSendProxies() = 0;

	// Called once during startup, after the game .dll has been loaded and after the client .dll has also been loaded
	virtual void			PostInit() = 0;
	// Called once per frame even when no level is loaded...
	virtual void			Think( bool finalTick ) = 0;

#ifdef _XBOX
	virtual void			GetTitleName( const char *pMapName, char* pTitleBuff, int titleBuffSize ) = 0;
#endif

	virtual void			PreSaveGameLoaded( char const *pSaveName, bool bCurrentlyInGame ) = 0;

	// Returns true if the game DLL wants the server not to be made public.
	// Used by commentary system to hide multiplayer commentary servers from the master.
	virtual bool			ShouldHideServer( void ) = 0;

	virtual void			InvalidateMdlCache() = 0;

	// * This function is new with version 6 of the interface.
	//
	// This is called when a query from IServerPluginHelpers::StartQueryCvarValue is finished.
	// iCookie is the value returned by IServerPluginHelpers::StartQueryCvarValue.
	// Added with version 2 of the interface.
	virtual void			OnQueryCvarValueFinished( QueryCvarCookie_t iCookie, edict_t *pPlayerEntity, EQueryCvarValueStatus eStatus, const char *pCvarName, const char *pCvarValue ) = 0;

	// Called after the steam API has been activated post-level startup
	virtual void			GameServerSteamAPIActivated( void ) = 0;

	// Called after the steam API has been shutdown post-level startup
	virtual void			GameServerSteamAPIShutdown( void ) = 0;

	virtual void			SetServerHibernation( bool bHibernating ) = 0;

	// interface to the new GC based lobby system
	virtual IServerGCLobby *GetServerGCLobby() = 0;

	// Return override string to show in the server browser
	// "map" column, or NULL to just use the default value
	// (the map name)
	virtual const char *GetServerBrowserMapOverride() = 0;

	// Get gamedata string to send to the master serer updater.
	virtual const char *GetServerBrowserGameData() = 0;
};

typedef IServerGameDLL IServerGameDLL008;

//-----------------------------------------------------------------------------
// Just an interface version name for the random number interface
// See vstdlib/random.h for the interface definition
// NOTE: If you change this, also change VENGINE_CLIENT_RANDOM_INTERFACE_VERSION in cdll_int.h
//-----------------------------------------------------------------------------
#define VENGINE_SERVER_RANDOM_INTERFACE_VERSION	"VEngineRandom001"

#define INTERFACEVERSION_SERVERGAMEENTS			"ServerGameEnts001"
//-----------------------------------------------------------------------------
// Purpose: Interface to get at server entities
//-----------------------------------------------------------------------------
abstract_class IServerGameEnts
{
public:
	virtual					~IServerGameEnts()	{}

	// Only for debugging. Set the edict base so you can get an edict's index in the debugger while debugging the game .dll
	virtual void			SetDebugEdictBase(edict_t *base) = 0;
	
	// The engine wants to mark two entities as touching
	virtual void			MarkEntitiesAsTouching( edict_t *e1, edict_t *e2 ) = 0;

	// Frees the entity attached to this edict
	virtual void			FreeContainingEntity( edict_t * ) = 0; 

	// This allows the engine to get at edicts in a CGameTrace.
	virtual edict_t*		BaseEntityToEdict( CBaseEntity *pEnt ) = 0;
	virtual CBaseEntity*	EdictToBaseEntity( edict_t *pEdict ) = 0;

	// This sets a bit in pInfo for each edict in the list that wants to be transmitted to the 
	// client specified in pInfo.
	//
	// This is also where an entity can force other entities to be transmitted if it refers to them
	// with ehandles.
	virtual void			CheckTransmit( CCheckTransmitInfo *pInfo, const unsigned short *pEdictIndices, int nEdicts ) = 0;
};

#define INTERFACEVERSION_SERVERGAMECLIENTS_VERSION_3	"ServerGameClients003"
#define INTERFACEVERSION_SERVERGAMECLIENTS				"ServerGameClients004"

//-----------------------------------------------------------------------------
// Purpose: Player / Client related functions
//-----------------------------------------------------------------------------
abstract_class IServerGameClients
{
public:
	// Get server maxplayers and lower bound for same
	virtual void			GetPlayerLimits( int& minplayers, int& maxplayers, int &defaultMaxPlayers ) const = 0;

	// Client is connecting to server ( return false to reject the connection )
	//	You can specify a rejection message by writing it into reject
	virtual bool			ClientConnect( edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen ) = 0;

	// Client is going active
	// If bLoadGame is true, don't spawn the player because its state is already setup.
	virtual void			ClientActive( edict_t *pEntity, bool bLoadGame ) = 0;
	
	// Client is disconnecting from server
	virtual void			ClientDisconnect( edict_t *pEntity ) = 0;
	
	// Client is connected and should be put in the game
	virtual void			ClientPutInServer( edict_t *pEntity, char const *playername ) = 0;
	
	// The client has typed a command at the console
	virtual void			ClientCommand( edict_t *pEntity, const CCommand &args ) = 0;

	// Sets the client index for the client who typed the command into his/her console
	virtual void			SetCommandClient( int index ) = 0;
	
	// A player changed one/several replicated cvars (name etc)
	virtual void			ClientSettingsChanged( edict_t *pEdict ) = 0;
	
	// Determine PVS origin and set PVS for the player/viewentity
	virtual void			ClientSetupVisibility( edict_t *pViewEntity, edict_t *pClient, unsigned char *pvs, int pvssize ) = 0;
	
	// A block of CUserCmds has arrived from the user, decode them and buffer for execution during player simulation
	virtual float			ProcessUsercmds( edict_t *player, bf_read *buf, int numcmds, int totalcmds,
								int dropped_packets, bool ignore, bool paused ) = 0;
	
	// Let the game .dll do stuff after messages have been sent to all of the clients once the server frame is complete
	virtual void			PostClientMessagesSent_DEPRECIATED( void ) = 0;

	// For players, looks up the CPlayerState structure corresponding to the player
	virtual CPlayerState	*GetPlayerState( edict_t *player ) = 0;

	// Get the ear position for a specified client
	virtual void			ClientEarPosition( edict_t *pEntity, Vector *pEarOrigin ) = 0;

	// returns number of delay ticks if player is in Replay mode (0 = no delay)
	virtual int				GetReplayDelay( edict_t *player, int& entity ) = 0;

	// Anything this game .dll wants to add to the bug reporter text (e.g., the entity/model under the picker crosshair)
	//  can be added here
	virtual void			GetBugReportInfo( char *buf, int buflen ) = 0;

	// A user has had their network id setup and validated 
	virtual void			NetworkIDValidated( const char *pszUserName, const char *pszNetworkID ) = 0;

	// The client has submitted a keyvalues command
	virtual void			ClientCommandKeyValues( edict_t *pEntity, KeyValues *pKeyValues ) = 0;

	// Hook for player spawning
	virtual void			ClientSpawned( edict_t *pPlayer ) = 0;
};

typedef IServerGameClients IServerGameClients003;


#define INTERFACEVERSION_UPLOADGAMESTATS		"ServerUploadGameStats001"

abstract_class IUploadGameStats
{
public:
	// Note that this call will block the server until the upload is completed, so use only at levelshutdown if at all.
	virtual bool UploadGameStats( 
		char const *mapname,				// Game map name
		unsigned int blobversion,			// Version of the binary blob data
		unsigned int blobsize,				// Size in bytes of blob data
		const void *pvBlobData ) = 0;		// Pointer to the blob data.

	// Call when created to init the CSER connection
	virtual void InitConnection( void ) = 0;

	// Call periodically to poll steam for a CSER connection
	virtual void UpdateConnection( void ) = 0;

	// If user has disabled stats tracking, do nothing
	virtual bool IsGameStatsLoggingEnabled() = 0;

	// Gets a non-personally identifiable unique ID for this steam user, used for tracking total gameplay time across
	//  multiple stats sessions, but isn't trackable back to their Steam account or id.
	// Buffer should be 16 bytes, ID will come back as a hexadecimal string version of a GUID
	virtual void GetPseudoUniqueId( char *buf, size_t bufsize ) = 0;

	// For determining general % of users running using cyber cafe accounts...
	virtual bool IsCyberCafeUser( void ) = 0;

	// Only works in single player
	virtual bool IsHDREnabled( void ) = 0;
};

#define INTERFACEVERSION_PLUGINHELPERSCHECK		"PluginHelpersCheck001"

//-----------------------------------------------------------------------------
// Purpose: allows the game dll to control which plugin functions can be run
//-----------------------------------------------------------------------------
abstract_class IPluginHelpersCheck
{
public:
	virtual bool CreateMessage( const char *plugin, edict_t *pEntity, DIALOG_TYPE type, KeyValues *data ) = 0;
};

//-----------------------------------------------------------------------------
// Purpose: Interface exposed from the client .dll back to the engine for specifying shared .dll IAppSystems (e.g., ISoundEmitterSystem)
//-----------------------------------------------------------------------------
abstract_class IServerDLLSharedAppSystems
{
public:
	virtual int	Count() = 0;
	virtual char const *GetDllName( int idx ) = 0;
	virtual char const *GetInterfaceName( int idx ) = 0;
};

#define SERVER_DLL_SHARED_APPSYSTEMS		"VServerDllSharedAppSystems001"

#define INTERFACEVERSION_SERVERGAMETAGS		"ServerGameTags001"

//-----------------------------------------------------------------------------
// Purpose: querying the game dll for Server cvar tags
//-----------------------------------------------------------------------------
abstract_class IServerGameTags
{
public:
	// Get the list of cvars that require tags to show differently in the server browser
	virtual void			GetTaggedConVarList( KeyValues *pCvarTagList ) = 0;
};

//-----------------------------------------------------------------------------
// Purpose: Provide hooks for the GC based lobby system
//-----------------------------------------------------------------------------
abstract_class IServerGCLobby
{
public:
	virtual bool HasLobby() const = 0;
	virtual bool SteamIDAllowedToConnect( const CSteamID &steamId ) const = 0;
	virtual void UpdateServerDetails(void) = 0;
	virtual bool ShouldHibernate() = 0;
};

// HL1

#include <stdio.h>
#include "custom.h"
#include "cvardef.h"
//
// Defines entity interface between engine and DLLs.
// This header file included by engine files and DLL files.
//
// Before including this header, DLLs must:
//		include progdefs.h
// This is conveniently done for them in extdll.h
//

typedef enum
	{
	at_notice,
	at_console,		// same as at_notice, but forces a ConPrintf, not a message box
	at_aiconsole,	// same as at_console, but only shown if developer level is 2!
	at_warning,
	at_error,
	at_logged		// Server print to console ( only in multiplayer games ).
	} ALERT_TYPE;

// 4-22-98  JOHN: added for use in pfnClientPrintf
typedef enum
	{
	print_console,
	print_center,
	print_chat,
	} PRINT_TYPE;

// For integrity checking of content on clients
typedef enum
{
	force_exactfile,			// File on client must exactly match server's file
	force_model_samebounds,		// For model files only, the geometry must fit in the same bbox
	force_model_specifybounds,	// For model files only, the geometry must fit in the specified bbox
} FORCE_TYPE;

// Returned by TraceLine
typedef struct
	{
	int		fAllSolid;			// if true, plane is not valid
	int		fStartSolid;		// if true, the initial point was in a solid area
	int		fInOpen;
	int		fInWater;
	float	flFraction;			// time completed, 1.0 = didn't hit anything
	vec3_t	vecEndPos;			// final position
	float	flPlaneDist;
	vec3_t	vecPlaneNormal;		// surface normal at impact
	edict_t_hl1	*pHit;				// entity the surface is on
	int		iHitgroup;			// 0 == generic, non zero is specific body part
	} TraceResult;

// CD audio status
typedef struct 
{
	int	fPlaying;// is sound playing right now?
	int	fWasPlaying;// if not, CD is paused if WasPlaying is true.
	int	fInitialized;
	int	fEnabled;
	int	fPlayLooping;
	float	cdvolume;
	//BYTE 	remap[100];
	int	fCDRom;
	int	fPlayTrack;
} CDStatus;

#include "../common/crc.h"

// Engine hands this to DLLs for functionality callbacks
typedef struct enginefuncs_s
{
	int			(*pfnPrecacheModel)			(char* s);
	int			(*pfnPrecacheSound)			(char* s);
	void		(*pfnSetModel)				(edict_t_hl1 *e, const char *m);
	int			(*pfnModelIndex)			(const char *m);
	int			(*pfnModelFrames)			(int modelIndex);
	void		(*pfnSetSize)				(edict_t_hl1 *e, const float *rgflMin, const float *rgflMax);
	void		(*pfnChangeLevel)			(char* s1, char* s2);
	void		(*pfnGetSpawnParms)			(edict_t_hl1 *ent);
	void		(*pfnSaveSpawnParms)		(edict_t_hl1 *ent);
	float		(*pfnVecToYaw)				(const float *rgflVector);
	void		(*pfnVecToAngles)			(const float *rgflVectorIn, float *rgflVectorOut);
	void		(*pfnMoveToOrigin)			(edict_t_hl1 *ent, const float *pflGoal, float dist, int iMoveType);
	void		(*pfnChangeYaw)				(edict_t_hl1* ent);
	void		(*pfnChangePitch)			(edict_t_hl1* ent);
	edict_t_hl1*	(*pfnFindEntityByString)	(edict_t_hl1 *pEdictStartSearchAfter, const char *pszField, const char *pszValue);
	int			(*pfnGetEntityIllum)		(edict_t_hl1* pEnt);
	edict_t_hl1*	(*pfnFindEntityInSphere)	(edict_t_hl1 *pEdictStartSearchAfter, const float *org, float rad);
	edict_t_hl1*	(*pfnFindClientInPVS)		(edict_t_hl1 *pEdict);
	edict_t_hl1* (*pfnEntitiesInPVS)			(edict_t_hl1 *pplayer);
	void		(*pfnMakeVectors)			(const float *rgflVector);
	void		(*pfnAngleVectors)			(const float *rgflVector, float *forward, float *right, float *up);
	edict_t_hl1*	(*pfnCreateEntity)			(void);
	void		(*pfnRemoveEntity)			(edict_t_hl1* e);
	edict_t_hl1*	(*pfnCreateNamedEntity)		(int className);
	void		(*pfnMakeStatic)			(edict_t_hl1 *ent);
	int			(*pfnEntIsOnFloor)			(edict_t_hl1 *e);
	int			(*pfnDropToFloor)			(edict_t_hl1* e);
	int			(*pfnWalkMove)				(edict_t_hl1 *ent, float yaw, float dist, int iMode);
	void		(*pfnSetOrigin)				(edict_t_hl1 *e, const float *rgflOrigin);
	void		(*pfnEmitSound)				(edict_t_hl1 *entity, int channel, const char *sample, /*int*/float volume, float attenuation, int fFlags, int pitch);
	void		(*pfnEmitAmbientSound)		(edict_t_hl1 *entity, float *pos, const char *samp, float vol, float attenuation, int fFlags, int pitch);
	void		(*pfnTraceLine)				(const float *v1, const float *v2, int fNoMonsters, edict_t_hl1 *pentToSkip, TraceResult *ptr);
	void		(*pfnTraceToss)				(edict_t_hl1* pent, edict_t_hl1* pentToIgnore, TraceResult *ptr);
	int			(*pfnTraceMonsterHull)		(edict_t_hl1 *pEdict, const float *v1, const float *v2, int fNoMonsters, edict_t_hl1 *pentToSkip, TraceResult *ptr);
	void		(*pfnTraceHull)				(const float *v1, const float *v2, int fNoMonsters, int hullNumber, edict_t_hl1 *pentToSkip, TraceResult *ptr);
	void		(*pfnTraceModel)			(const float *v1, const float *v2, int hullNumber, edict_t_hl1 *pent, TraceResult *ptr);
	const char *(*pfnTraceTexture)			(edict_t_hl1 *pTextureEntity, const float *v1, const float *v2 );
	void		(*pfnTraceSphere)			(const float *v1, const float *v2, int fNoMonsters, float radius, edict_t_hl1 *pentToSkip, TraceResult *ptr);
	void		(*pfnGetAimVector)			(edict_t_hl1* ent, float speed, float *rgflReturn);
	void		(*pfnServerCommand)			(char* str);
	void		(*pfnServerExecute)			(void);
	void		(*pfnClientCommand)			(edict_t_hl1* pEdict, char* szFmt, ...);
	void		(*pfnParticleEffect)		(const float *org, const float *dir, float color, float count);
	void		(*pfnLightStyle)			(int style, char* val);
	int			(*pfnDecalIndex)			(const char *name);
	int			(*pfnPointContents)			(const float *rgflVector);
	void		(*pfnMessageBegin)			(int msg_dest, int msg_type, const float *pOrigin, edict_t_hl1 *ed);
	void		(*pfnMessageEnd)			(void);
	void		(*pfnWriteByte)				(int iValue);
	void		(*pfnWriteChar)				(int iValue);
	void		(*pfnWriteShort)			(int iValue);
	void		(*pfnWriteLong)				(int iValue);
	void		(*pfnWriteAngle)			(float flValue);
	void		(*pfnWriteCoord)			(float flValue);
	void		(*pfnWriteString)			(const char *sz);
	void		(*pfnWriteEntity)			(int iValue);
	void		(*pfnCVarRegister)			(cvar_t *pCvar);
	float		(*pfnCVarGetFloat)			(const char *szVarName);
	const char*	(*pfnCVarGetString)			(const char *szVarName);
	void		(*pfnCVarSetFloat)			(const char *szVarName, float flValue);
	void		(*pfnCVarSetString)			(const char *szVarName, const char *szValue);
	void		(*pfnAlertMessage)			(ALERT_TYPE atype, char *szFmt, ...);
	void		(*pfnEngineFprintf)			(FILE *pfile, char *szFmt, ...);
	void*		(*pfnPvAllocEntPrivateData)	(edict_t_hl1 *pEdict, long cb);
	void*		(*pfnPvEntPrivateData)		(edict_t_hl1 *pEdict);
	void		(*pfnFreeEntPrivateData)	(edict_t_hl1 *pEdict);
	const char*	(*pfnSzFromIndex)			(int iString);
	int			(*pfnAllocString)			(const char *szValue);
	struct entvars_s*	(*pfnGetVarsOfEnt)			(edict_t_hl1 *pEdict);
	edict_t_hl1*	(*pfnPEntityOfEntOffset)	(int iEntOffset);
	int			(*pfnEntOffsetOfPEntity)	(const edict_t_hl1 *pEdict);
	int			(*pfnIndexOfEdict)			(const edict_t_hl1 *pEdict);
	edict_t_hl1*	(*pfnPEntityOfEntIndex)		(int iEntIndex);
	edict_t_hl1*	(*pfnFindEntityByVars)		(struct entvars_s* pvars);
	void*		(*pfnGetModelPtr)			(edict_t_hl1* pEdict);
	int			(*pfnRegUserMsg)			(const char *pszName, int iSize);
	void		(*pfnAnimationAutomove)		(const edict_t_hl1* pEdict, float flTime);
	void		(*pfnGetBonePosition)		(const edict_t_hl1* pEdict, int iBone, float *rgflOrigin, float *rgflAngles );
	unsigned long (*pfnFunctionFromName)	( const char *pName );
	const char *(*pfnNameForFunction)		( unsigned long function );
	void		(*pfnClientPrintf)			( edict_t_hl1* pEdict, PRINT_TYPE ptype, const char *szMsg ); // JOHN: engine callbacks so game DLL can print messages to individual clients
	void		(*pfnServerPrint)			( const char *szMsg );
	const char *(*pfnCmd_Args)				( void );		// these 3 added 
	const char *(*pfnCmd_Argv)				( int argc );	// so game DLL can easily 
	int			(*pfnCmd_Argc)				( void );		// access client 'cmd' strings
	void		(*pfnGetAttachment)			(const edict_t_hl1 *pEdict, int iAttachment, float *rgflOrigin, float *rgflAngles );
	void		(*pfnCRC32_Init)			(CRC32_t *pulCRC);
	void        (*pfnCRC32_ProcessBuffer)   (CRC32_t *pulCRC, void *p, int len);
	void		(*pfnCRC32_ProcessByte)     (CRC32_t *pulCRC, unsigned char ch);
	CRC32_t		(*pfnCRC32_Final)			(CRC32_t pulCRC);
	long		(*pfnRandomLong)			(long  lLow,  long  lHigh);
	float		(*pfnRandomFloat)			(float flLow, float flHigh);
	void		(*pfnSetView)				(const edict_t_hl1 *pClient, const edict_t_hl1 *pViewent );
	float		(*pfnTime)					( void );
	void		(*pfnCrosshairAngle)		(const edict_t_hl1 *pClient, float pitch, float yaw);
	byte *      (*pfnLoadFileForMe)         (char *filename, int *pLength);
	void        (*pfnFreeFile)              (void *buffer);
	void        (*pfnEndSection)            (const char *pszSectionName); // trigger_endsection
	int 		(*pfnCompareFileTime)       (char *filename1, char *filename2, int *iCompare);
	void        (*pfnGetGameDir)            (char *szGetGameDir);
	void		(*pfnCvar_RegisterVariable) (cvar_t *variable);
	void        (*pfnFadeClientVolume)      (const edict_t_hl1 *pEdict, int fadePercent, int fadeOutSeconds, int holdTime, int fadeInSeconds);
	void        (*pfnSetClientMaxspeed)     (const edict_t_hl1 *pEdict, float fNewMaxspeed);
	edict_t_hl1 *	(*pfnCreateFakeClient)		(const char *netname);	// returns NULL if fake client can't be created
	void		(*pfnRunPlayerMove)			(edict_t_hl1 *fakeclient, const float *viewangles, float forwardmove, float sidemove, float upmove, unsigned short buttons, byte impulse, byte msec );
	int			(*pfnNumberOfEntities)		(void);
	char*		(*pfnGetInfoKeyBuffer)		(edict_t_hl1 *e);	// passing in NULL gets the serverinfo
	char*		(*pfnInfoKeyValue)			(char *infobuffer, char *key);
	void		(*pfnSetKeyValue)			(char *infobuffer, char *key, char *value);
	void		(*pfnSetClientKeyValue)		(int clientIndex, char *infobuffer, char *key, char *value);
	int			(*pfnIsMapValid)			(char *filename);
	void		(*pfnStaticDecal)			( const float *origin, int decalIndex, int entityIndex, int modelIndex );
	int			(*pfnPrecacheGeneric)		(char* s);
	int			(*pfnGetPlayerUserId)		(edict_t_hl1 *e ); // returns the server assigned userid for this player.  useful for logging frags, etc.  returns -1 if the edict couldn't be found in the list of clients
	void		(*pfnBuildSoundMsg)			(edict_t_hl1 *entity, int channel, const char *sample, /*int*/float volume, float attenuation, int fFlags, int pitch, int msg_dest, int msg_type, const float *pOrigin, edict_t_hl1 *ed);
	int			(*pfnIsDedicatedServer)		(void);// is this a dedicated server?
	cvar_t		*(*pfnCVarGetPointer)		(const char *szVarName);
	unsigned int (*pfnGetPlayerWONId)		(edict_t_hl1 *e); // returns the server assigned WONid for this player.  useful for logging frags, etc.  returns -1 if the edict couldn't be found in the list of clients

	// YWB 8/1/99 TFF Physics additions
	void		(*pfnInfo_RemoveKey)		( char *s, const char *key );
	const char *(*pfnGetPhysicsKeyValue)	( const edict_t_hl1 *pClient, const char *key );
	void		(*pfnSetPhysicsKeyValue)	( const edict_t_hl1 *pClient, const char *key, const char *value );
	const char *(*pfnGetPhysicsInfoString)	( const edict_t_hl1 *pClient );
	unsigned short (*pfnPrecacheEvent)		( int type, const char*psz );
	void		(*pfnPlaybackEvent)			( int flags, const edict_t_hl1 *pInvoker, unsigned short eventindex, float delay, float *origin, float *angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2 );

	unsigned char *(*pfnSetFatPVS)			( float *org );
	unsigned char *(*pfnSetFatPAS)			( float *org );

	int			(*pfnCheckVisibility )		( const edict_t_hl1 *entity, unsigned char *pset );

	void		(*pfnDeltaSetField)			( struct delta_s *pFields, const char *fieldname );
	void		(*pfnDeltaUnsetField)		( struct delta_s *pFields, const char *fieldname );
	void		(*pfnDeltaAddEncoder)		( char *name, void (*conditionalencode)( struct delta_s *pFields, const unsigned char *from, const unsigned char *to ) );
	int			(*pfnGetCurrentPlayer)		( void );
	int			(*pfnCanSkipPlayer)			( const edict_t_hl1 *player );
	int			(*pfnDeltaFindField)		( struct delta_s *pFields, const char *fieldname );
	void		(*pfnDeltaSetFieldByIndex)	( struct delta_s *pFields, int fieldNumber );
	void		(*pfnDeltaUnsetFieldByIndex)( struct delta_s *pFields, int fieldNumber );

	void		(*pfnSetGroupMask)			( int mask, int op );

	int			(*pfnCreateInstancedBaseline) ( int classname, struct entity_state_s *baseline );
	void		(*pfnCvar_DirectSet)		( struct cvar_s *var, char *value );

	// Forces the client and server to be running with the same version of the specified file
	//  ( e.g., a player model ).
	// Calling this has no effect in single player
	void		(*pfnForceUnmodified)		( FORCE_TYPE type, float *mins, float *maxs, const char *filename );

	void		(*pfnGetPlayerStats)		( const edict_t_hl1 *pClient, int *ping, int *packet_loss );

	void		(*pfnAddServerCommand)		( char *cmd_name, void (*function) (void) );

	// For voice communications, set which clients hear eachother.
	// NOTE: these functions take player entity indices (starting at 1).
	qboolean	(*pfnVoice_GetClientListening)(int iReceiver, int iSender);
	qboolean	(*pfnVoice_SetClientListening)(int iReceiver, int iSender, qboolean bListen);

	const char *(*pfnGetPlayerAuthId)		( edict_t_hl1 *e );
} enginefuncs_t;
// ONLY ADD NEW FUNCTIONS TO THE END OF THIS STRUCT.  INTERFACE VERSION IS FROZEN AT 138

// Passed to pfnKeyValue
typedef struct KeyValueData_s
{
	char	*szClassName;	// in: entity classname
	char	*szKeyName;		// in: name of key
	char	*szValue;		// in: value of key
	long	fHandled;		// out: DLL sets to true if key-value pair was understood
} KeyValueData;


typedef struct
{
	char		mapName[ 32 ];
	char		landmarkName[ 32 ];
	edict_t_hl1	*pentLandmark;
	vec3_t		vecLandmarkOrigin;
} LEVELLIST;
#define MAX_LEVEL_CONNECTIONS	16		// These are encoded in the lower 16bits of ENTITYTABLE->flags

typedef struct 
{
	int			id;				// Ordinal ID of this entity (used for entity <--> pointer conversions)
	edict_t_hl1	*pent;			// Pointer to the in-game entity

	int			location;		// Offset from the base data of this entity
	int			size;			// Byte size of this entity's data
	int			flags;			// This could be a short -- bit mask of transitions that this entity is in the PVS of
	string_t	classname;		// entity class name

} ENTITYTABLE;

#define FENTTABLE_PLAYER		0x80000000
#define FENTTABLE_REMOVED		0x40000000
#define FENTTABLE_MOVEABLE		0x20000000
#define FENTTABLE_GLOBAL		0x10000000

typedef struct saverestore_s SAVERESTOREDATA;

#ifdef _WIN32
typedef 
#endif
struct saverestore_s
{
	char		*pBaseData;		// Start of all entity save data
	char		*pCurrentData;	// Current buffer pointer for sequential access
	int			size;			// Current data size
	int			bufferSize;		// Total space for data
	int			tokenSize;		// Size of the linear list of tokens
	int			tokenCount;		// Number of elements in the pTokens table
	char		**pTokens;		// Hash table of entity strings (sparse)
	int			currentIndex;	// Holds a global entity table ID
	int			tableCount;		// Number of elements in the entity table
	int			connectionCount;// Number of elements in the levelList[]
	ENTITYTABLE	*pTable;		// Array of ENTITYTABLE elements (1 for each entity)
	LEVELLIST	levelList[ MAX_LEVEL_CONNECTIONS ];		// List of connections from this level

	// smooth transition
	int			fUseLandmark;
	char		szLandmarkName[20];// landmark we'll spawn near in next level
	vec3_t		vecLandmarkOffset;// for landmark transitions
	float		time;
	char		szCurrentMapName[32];	// To check global entities

} 
#ifdef _WIN32
SAVERESTOREDATA 
#endif
;

typedef enum _fieldtypeshl1
{
	FIELD_FLOAT_HL1 = 0,		// Any floating point value
	FIELD_STRING_HL1,			// A string ID (return from ALLOC_STRING)
	FIELD_ENTITY_HL1,			// An entity offset (EOFFSET)
	FIELD_CLASSPTR_HL1,			// CBaseEntity *
	FIELD_EHANDLE_HL1,			// Entity handle
	FIELD_EVARS_HL1,			// EVARS *
	FIELD_EDICT_HL1,			// edict_t_hl1 *, or edict_t_hl1 *  (same thing)
	FIELD_VECTOR_HL1,			// Any vector
	FIELD_POSITION_VECTOR_HL1,	// A world coordinate (these are fixed up across level transitions automagically)
	FIELD_POINTER_HL1,			// Arbitrary data pointer... to be removed, use an array of FIELD_CHARACTER
	FIELD_INTEGER_HL1,			// Any integer or enum
	FIELD_FUNCTION_HL1,			// A class function pointer (Think, Use, etc)
	FIELD_BOOLEAN_HL1,			// boolean, implemented as an int, I may use this as a hint for compression
	FIELD_SHORT_HL1,			// 2 byte integer
	FIELD_CHARACTER_HL1,		// a byte
	FIELD_TIME_HL1,				// a floating point time (these are fixed up automatically too!)
	FIELD_MODELNAME_HL1,		// Engine string that is a model name (needs precache)
	FIELD_SOUNDNAME_HL1,		// Engine string that is a sound name (needs precache)

	FIELD_TYPECOUNT_HL1,		// MUST BE LAST
} FIELDTYPEHL1;

#ifndef offsetof
#define offsetof(s,m)	(size_t)&(((s *)0)->m)
#endif

#define _FIELD_HL1(type,name,fieldtype,count,flags)		{ fieldtype, #name, offsetof(type, name), count, flags }
#define DEFINE_FIELD_HL1(type,name,fieldtype)			_FIELD_HL1(type, name, fieldtype, 1, 0)
#define DEFINE_ARRAY_HL1(type,name,fieldtype,count)		_FIELD_HL1(type, name, fieldtype, count, 0)
#define DEFINE_ENTITY_FIELD_HL1(name,fieldtype)			_FIELD_HL1(entvars_t, name, fieldtype, 1, 0 )
#define DEFINE_ENTITY_GLOBAL_FIELD_HL1(name,fieldtype)	_FIELD_HL1(entvars_t, name, fieldtype, 1, FTYPEDESC_GLOBAL_HL1 )
#define DEFINE_GLOBAL_FIELD_HL1(type,name,fieldtype)	_FIELD_HL1(type, name, fieldtype, 1, FTYPEDESC_GLOBAL_HL1 )


#define FTYPEDESC_GLOBAL_HL1			0x0001		// This field is masked for global entity save/restore

typedef struct 
{
	FIELDTYPEHL1	fieldType;
	char			*fieldName;
	int				fieldOffset;
	short			fieldSize;
	short			flags;
} TYPEDESCRIPTION;

#define ARRAYSIZE_HL1(p)		(sizeof(p)/sizeof(p[0]))

typedef struct 
{
	// Initialize/shutdown the game (one-time call after loading of game .dll )
	void			(*pfnGameInit)			( void );				
	int				(*pfnSpawn)				( edict_t_hl1 *pent );
	void			(*pfnThink)				( edict_t_hl1 *pent );
	void			(*pfnUse)				( edict_t_hl1 *pentUsed, edict_t_hl1 *pentOther );
	void			(*pfnTouch)				( edict_t_hl1 *pentTouched, edict_t_hl1 *pentOther );
	void			(*pfnBlocked)			( edict_t_hl1 *pentBlocked, edict_t_hl1 *pentOther );
	void			(*pfnKeyValue)			( edict_t_hl1 *pentKeyvalue, KeyValueData *pkvd );
	void			(*pfnSave)				( edict_t_hl1 *pent, SAVERESTOREDATA *pSaveData );
	int 			(*pfnRestore)			( edict_t_hl1 *pent, SAVERESTOREDATA *pSaveData, int globalEntity );
	void			(*pfnSetAbsBox)			( edict_t_hl1 *pent );

	void			(*pfnSaveWriteFields)	( SAVERESTOREDATA *, const char *, void *, TYPEDESCRIPTION *, int );
	void			(*pfnSaveReadFields)	( SAVERESTOREDATA *, const char *, void *, TYPEDESCRIPTION *, int );

	void			(*pfnSaveGlobalState)		( SAVERESTOREDATA * );
	void			(*pfnRestoreGlobalState)	( SAVERESTOREDATA * );
	void			(*pfnResetGlobalState)		( void );

	qboolean		(*pfnClientConnect)		( edict_t_hl1 *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[ 128 ] );
	
	void			(*pfnClientDisconnect)	( edict_t_hl1 *pEntity );
	void			(*pfnClientKill)		( edict_t_hl1 *pEntity );
	void			(*pfnClientPutInServer)	( edict_t_hl1 *pEntity );
	void			(*pfnClientCommand)		( edict_t_hl1 *pEntity );
	void			(*pfnClientUserInfoChanged)( edict_t_hl1 *pEntity, char *infobuffer );

	void			(*pfnServerActivate)	( edict_t_hl1 *pEdictList, int edictCount, int clientMax );
	void			(*pfnServerDeactivate)	( void );

	void			(*pfnPlayerPreThink)	( edict_t_hl1 *pEntity );
	void			(*pfnPlayerPostThink)	( edict_t_hl1 *pEntity );

	void			(*pfnStartFrame)		( void );
	void			(*pfnParmsNewLevel)		( void );
	void			(*pfnParmsChangeLevel)	( void );

	 // Returns string describing current .dll.  E.g., TeamFotrress 2, Half-Life
	const char     *(*pfnGetGameDescription)( void );     

	// Notify dll about a player customization.
	void            (*pfnPlayerCustomization) ( edict_t_hl1 *pEntity, customization_t *pCustom );  

	// Spectator funcs
	void			(*pfnSpectatorConnect)		( edict_t_hl1 *pEntity );
	void			(*pfnSpectatorDisconnect)	( edict_t_hl1 *pEntity );
	void			(*pfnSpectatorThink)		( edict_t_hl1 *pEntity );

	// Notify game .dll that engine is going to shut down.  Allows mod authors to set a breakpoint.
	void			(*pfnSys_Error)			( const char *error_string );

	void			(*pfnPM_Move) ( struct playermove_s *ppmove, qboolean server );
	void			(*pfnPM_Init) ( struct playermove_s *ppmove );
	char			(*pfnPM_FindTextureType)( char *name );
	void			(*pfnSetupVisibility)( struct edict_s *pViewEntity, struct edict_s *pClient, unsigned char **pvs, unsigned char **pas );
	void			(*pfnUpdateClientData) ( const struct edict_s *ent, int sendweapons, struct clientdata_s *cd );
	int				(*pfnAddToFullPack)( struct entity_state_s *state, int e, edict_t_hl1 *ent, edict_t_hl1 *host, int hostflags, int player, unsigned char *pSet );
	void			(*pfnCreateBaseline) ( int player, int eindex, struct entity_state_s *baseline, struct edict_s *entity, int playermodelindex, vec3_t player_mins, vec3_t player_maxs );
	void			(*pfnRegisterEncoders)	( void );
	int				(*pfnGetWeaponData)		( struct edict_s *player, struct weapon_data_s *info );

	void			(*pfnCmdStart)			( const edict_t_hl1 *player, const struct usercmd_s *cmd, unsigned int random_seed );
	void			(*pfnCmdEnd)			( const edict_t_hl1 *player );

	// Return 1 if the packet is valid.  Set response_buffer_size if you want to send a response packet.  Incoming, it holds the max
	//  size of the response_buffer, so you must zero it out if you choose not to respond.
	int				(*pfnConnectionlessPacket )	( const struct netadr_s *net_from, const char *args, char *response_buffer, int *response_buffer_size );

	// Enumerates player hulls.  Returns 0 if the hull number doesn't exist, 1 otherwise
	int				(*pfnGetHullBounds)	( int hullnumber, float *mins, float *maxs );

	// Create baselines for certain "unplaced" items.
	void			(*pfnCreateInstancedBaselines) ( void );

	// One of the pfnForceUnmodified files failed the consistency check for the specified player
	// Return 0 to allow the client to continue, 1 to force immediate disconnection ( with an optional disconnect message of up to 256 characters )
	int				(*pfnInconsistentFile)( const struct edict_s *player, const char *filename, char *disconnect_message );

	// The game .dll should return 1 if lag compensation should be allowed ( could also just set
	//  the sv_unlag cvar.
	// Most games right now should return 0, until client-side weapon prediction code is written
	//  and tested for them.
	int				(*pfnAllowLagCompensation)( void );
} DLL_FUNCTIONS;

extern DLL_FUNCTIONS		gEntityInterface;

// Current version.
#define NEW_DLL_FUNCTIONS_VERSION	1

typedef struct
{
	// Called right before the object's memory is freed. 
	// Calls its destructor.
	void			(*pfnOnFreeEntPrivateData)(edict_t_hl1 *pEnt);
	void			(*pfnGameShutdown)(void);
	int				(*pfnShouldCollide)( edict_t_hl1 *pentTouched, edict_t_hl1 *pentOther );
} NEW_DLL_FUNCTIONS;
typedef int	(*NEW_DLL_FUNCTIONS_FN)( NEW_DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion );

// Pointers will be null if the game DLL doesn't support this API.
extern NEW_DLL_FUNCTIONS	gNewDLLFunctions;

typedef int	(*APIFUNCTION)( DLL_FUNCTIONS *pFunctionTable, int interfaceVersion );
typedef int	(*APIFUNCTION2)( DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion );

#endif // EIFACE_H
