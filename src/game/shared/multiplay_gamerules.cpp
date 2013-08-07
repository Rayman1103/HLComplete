//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Contains the implementation of game rules for multiplayer.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "cdll_int.h"
#include "multiplay_gamerules.h"
#include "viewport_panel_names.h"
#include "gameeventdefs.h"
#include <KeyValues.h>
#include "filesystem.h"
#include "mp_shareddefs.h"
#include "utlbuffer.h"

//HL1
#include	"extdll.h"
#include	"util.h"
//#include	"player.h"
#include	"weapons.h"
//#include	"gamerules.h"
 
#include	"skill.h"
//#include	"game.h"
//#include	"items.h"
//#include	"voice_gamemgr.h"
#include	"hltv.h"

#ifdef CLIENT_DLL

#else

	#include "eventqueue.h"
	#include "player.h"
	#include "basecombatweapon.h"
	#include "gamerules.h"
	#include "game.h"
	#include "items.h"
	#include "entitylist.h"
	#include "in_buttons.h" 
	#include <ctype.h>
	#include "voice_gamemgr.h"
	#include "iscorer.h"
	#include "hltvdirector.h"
	#include "AI_Criteria.h"
	#include "sceneentity.h"
	#include "basemultiplayerplayer.h"
	#include "team.h"
	#include "usermessages.h"
	#include "tier0/icommandline.h"

#ifdef NEXT_BOT
	#include "NextBotManager.h"
#endif

#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


REGISTER_GAMERULES_CLASS( CMultiplayRules );

ConVar mp_chattime(
		"mp_chattime", 
		"10", 
		FCVAR_REPLICATED,
		"amount of time players can chat after the game is over",
		true, 1,
		true, 120 );

#ifdef GAME_DLL
void MPTimeLimitCallback( IConVar *var, const char *pOldString, float flOldValue )
{
	if ( mp_timelimit.GetInt() < 0 )
	{
		mp_timelimit.SetValue( 0 );
	}

	if ( MultiplayRules() )
	{
		MultiplayRules()->HandleTimeLimitChange();
	}
}
#endif 

ConVar mp_timelimit( "mp_timelimit", "0", FCVAR_NOTIFY|FCVAR_REPLICATED, "game time per map in minutes"
#ifdef GAME_DLL
					, MPTimeLimitCallback 
#endif
					);

ConVar fraglimit( "mp_fraglimit","0", FCVAR_NOTIFY|FCVAR_REPLICATED, "The number of kills at which the map ends");

ConVar mp_show_voice_icons( "mp_show_voice_icons", "1", FCVAR_REPLICATED, "Show overhead player voice icons when players are speaking.\n" );

#ifdef GAME_DLL

ConVar tv_delaymapchange( "tv_delaymapchange", "0", 0, "Delays map change until broadcast is complete" );

ConVar mp_restartgame( "mp_restartgame", "0", FCVAR_GAMEDLL, "If non-zero, game will restart in the specified number of seconds" );
ConVar mp_restartgame_immediate( "mp_restartgame_immediate", "0", FCVAR_GAMEDLL, "If non-zero, game will restart immediately" );

ConVar mp_mapcycle_empty_timeout_seconds( "mp_mapcycle_empty_timeout_seconds", "0", FCVAR_REPLICATED, "If nonzero, server will cycle to the next map if it has been empty on the current map for N seconds");

void cc_SkipNextMapInCycle()
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( MultiplayRules() )
	{
		MultiplayRules()->SkipNextMapInCycle();
	}
}

void cc_GotoNextMapInCycle()
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	if ( MultiplayRules() )
	{
		MultiplayRules()->ChangeLevel();
	}
}

ConCommand skip_next_map( "skip_next_map", cc_SkipNextMapInCycle, "Skips the next map in the map rotation for the server." );
ConCommand changelevel_next( "changelevel_next", cc_GotoNextMapInCycle, "Immediately changes to the next map in the map rotation for the server." );

#ifndef TF_DLL		// TF overrides the default value of this convar
ConVar mp_waitingforplayers_time( "mp_waitingforplayers_time", "0", FCVAR_GAMEDLL, "WaitingForPlayers time length in seconds" );
#endif

ConVar mp_waitingforplayers_restart( "mp_waitingforplayers_restart", "0", FCVAR_GAMEDLL, "Set to 1 to start or restart the WaitingForPlayers period." );
ConVar mp_waitingforplayers_cancel( "mp_waitingforplayers_cancel", "0", FCVAR_GAMEDLL, "Set to 1 to end the WaitingForPlayers period." );
ConVar mp_clan_readyrestart( "mp_clan_readyrestart", "0", FCVAR_GAMEDLL, "If non-zero, game will restart once someone from each team gives the ready signal" );
ConVar mp_clan_ready_signal( "mp_clan_ready_signal", "ready", FCVAR_GAMEDLL, "Text that team leader from each team must speak for the match to begin" );

ConVar nextlevel( "nextlevel", 
				  "", 
				  FCVAR_GAMEDLL | FCVAR_NOTIFY,
#if defined( CSTRIKE_DLL ) || defined( TF_DLL )
				  "If set to a valid map name, will trigger a changelevel to the specified map at the end of the round" );
#else
				  "If set to a valid map name, will change to this map during the next changelevel" );
#endif // CSTRIKE_DLL || TF_DLL
					  					  
#endif

#ifndef CLIENT_DLL
int CMultiplayRules::m_nMapCycleTimeStamp = 0;
int CMultiplayRules::m_nMapCycleindex = 0;
CUtlVector<char*> CMultiplayRules::m_MapList;
#endif

//=========================================================
//=========================================================
bool CMultiplayRules::IsMultiplayer( void )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CMultiplayRules::Damage_GetTimeBased( void )
{
	int iDamage = ( DMG_PARALYZE | DMG_NERVEGAS | DMG_POISON | DMG_RADIATION | DMG_DROWNRECOVER | DMG_ACID | DMG_SLOWBURN );
	return iDamage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CMultiplayRules::Damage_GetShouldGibCorpse( void )
{
	int iDamage = ( DMG_CRUSH | DMG_FALL | DMG_BLAST | DMG_SONIC | DMG_CLUB );
	return iDamage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CMultiplayRules::Damage_GetShowOnHud( void )
{
	int iDamage = ( DMG_POISON | DMG_ACID | DMG_DROWN | DMG_BURN | DMG_SLOWBURN | DMG_NERVEGAS | DMG_RADIATION | DMG_SHOCK );
	return iDamage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CMultiplayRules::Damage_GetNoPhysicsForce( void )
{
	int iTimeBasedDamage = Damage_GetTimeBased();
	int iDamage = ( DMG_FALL | DMG_BURN | DMG_PLASMA | DMG_DROWN | iTimeBasedDamage | DMG_CRUSH | DMG_PHYSGUN | DMG_PREVENT_PHYSICS_FORCE );
	return iDamage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CMultiplayRules::Damage_GetShouldNotBleed( void )
{
	int iDamage = ( DMG_POISON | DMG_ACID );
	return iDamage;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iDmgType - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMultiplayRules::Damage_IsTimeBased( int iDmgType )
{
	// Damage types that are time-based.
	return ( ( iDmgType & ( DMG_PARALYZE | DMG_NERVEGAS | DMG_POISON | DMG_RADIATION | DMG_DROWNRECOVER | DMG_ACID | DMG_SLOWBURN ) ) != 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iDmgType - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMultiplayRules::Damage_ShouldGibCorpse( int iDmgType )
{
	// Damage types that gib the corpse.
	return ( ( iDmgType & ( DMG_CRUSH | DMG_FALL | DMG_BLAST | DMG_SONIC | DMG_CLUB ) ) != 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iDmgType - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMultiplayRules::Damage_ShowOnHUD( int iDmgType )
{
	// Damage types that have client HUD art.
	return ( ( iDmgType & ( DMG_POISON | DMG_ACID | DMG_DROWN | DMG_BURN | DMG_SLOWBURN | DMG_NERVEGAS | DMG_RADIATION | DMG_SHOCK ) ) != 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iDmgType - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMultiplayRules::Damage_NoPhysicsForce( int iDmgType )
{
	// Damage types that don't have to supply a physics force & position.
	int iTimeBasedDamage = Damage_GetTimeBased();
	return ( ( iDmgType & ( DMG_FALL | DMG_BURN | DMG_PLASMA | DMG_DROWN | iTimeBasedDamage | DMG_CRUSH | DMG_PHYSGUN | DMG_PREVENT_PHYSICS_FORCE ) ) != 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : iDmgType - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CMultiplayRules::Damage_ShouldNotBleed( int iDmgType )
{
	// Damage types that don't make the player bleed.
	return ( ( iDmgType & ( DMG_POISON | DMG_ACID ) ) != 0 );
}

//*********************************************************
// Rules for the half-life multiplayer game.
//*********************************************************
CMultiplayRules::CMultiplayRules()
{
#ifndef CLIENT_DLL
	m_flTimeLastMapChangeOrPlayerWasConnected = 0.0f;

	RefreshSkillData( true );

	// 11/8/98
	// Modified by YWB:  Server .cfg file is now a cvar, so that 
	//  server ops can run multiple game servers, with different server .cfg files,
	//  from a single installed directory.
	// Mapcyclefile is already a cvar.

	// 3/31/99
	// Added lservercfg file cvar, since listen and dedicated servers should not
	// share a single config file. (sjb)
	if ( engine->IsDedicatedServer() )
	{
		// dedicated server
		const char *cfgfile = servercfgfile.GetString();

		if ( cfgfile && cfgfile[0] )
		{
			char szCommand[256];

			Log( "Executing dedicated server config file %s\n", cfgfile );
			Q_snprintf( szCommand,sizeof(szCommand), "exec %s\n", cfgfile );
			engine->ServerCommand( szCommand );
		}
	}
	else
	{
		// listen server
		const char *cfgfile = lservercfgfile.GetString();

		if ( cfgfile && cfgfile[0] )
		{
			char szCommand[256];

			Log( "Executing listen server config file %s\n", cfgfile );
			Q_snprintf( szCommand,sizeof(szCommand), "exec %s\n", cfgfile );
			engine->ServerCommand( szCommand );
		}
	}

	nextlevel.SetValue( "" );
	LoadMapCycleFile();

#endif

	LoadVoiceCommandScript();
}

bool CMultiplayRules::Init()
{
#ifdef GAME_DLL

	// Initialize the custom response rule dictionaries.
	InitCustomResponseRulesDicts();

#endif

	return BaseClass::Init();
}


#ifdef CLIENT_DLL


#else 

	extern bool			g_fGameOver;

	#define ITEM_RESPAWN_TIME	30
	#define WEAPON_RESPAWN_TIME	20
	#define AMMO_RESPAWN_TIME	20

	//=========================================================
	//=========================================================
	void CMultiplayRules::RefreshSkillData( bool forceUpdate )
	{
	// load all default values
		BaseClass::RefreshSkillData( forceUpdate );

	// override some values for multiplay.

		// suitcharger
#ifndef TF_DLL
//=============================================================================
// HPE_BEGIN:
// [menglish] CS doesn't have the suitcharger either
//=============================================================================
#ifndef CSTRIKE_DLL
ConVarRef suitcharger( "sk_suitcharger" );
		suitcharger.SetValue( 30 );
 #endif
//=============================================================================
// HPE_END
//=============================================================================
#endif
	}


	//=========================================================
	//=========================================================
	void CMultiplayRules::Think ( void )
	{
		BaseClass::Think();
		
		///// Check game rules /////

		if ( g_fGameOver )   // someone else quit the game already
		{
			ChangeLevel(); // intermission is over
			return;
		}

		float flTimeLimit = mp_timelimit.GetFloat() * 60;
		float flFragLimit = fraglimit.GetFloat();
		
		if ( flTimeLimit != 0 && gpGlobals->curtime >= flTimeLimit )
		{
			GoToIntermission();
			return;
		}

		if ( flFragLimit )
		{
			// check if any player is over the frag limit
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

				if ( pPlayer && pPlayer->FragCount() >= flFragLimit )
				{
					GoToIntermission();
					return;
				}
			}
		}
	}

	//=========================================================
	//=========================================================
	void CMultiplayRules::FrameUpdatePostEntityThink()
	{
		BaseClass::FrameUpdatePostEntityThink();

		float flNow = Plat_FloatTime();

		// Update time when client was last connected
		if ( m_flTimeLastMapChangeOrPlayerWasConnected <= 0.0f )
		{
			m_flTimeLastMapChangeOrPlayerWasConnected = flNow;
		}
		else
		{
			for( int iPlayerIndex = 1 ; iPlayerIndex <= MAX_PLAYERS; iPlayerIndex++ )
			{
				player_info_t pi;
				if ( !engine->GetPlayerInfo( iPlayerIndex, &pi ) )
					continue;
#if defined( REPLAY_ENABLED )
				if ( pi.ishltv || pi.isreplay || pi.fakeplayer )
#else
				if ( pi.ishltv || pi.fakeplayer )
#endif
					continue;

				m_flTimeLastMapChangeOrPlayerWasConnected = flNow;
				break;
			}
		}

		// Check if we should cycle the map because we've been empty
		// for long enough
		if ( mp_mapcycle_empty_timeout_seconds.GetInt() > 0 )
		{
			int iIdleSeconds = (int)( flNow - m_flTimeLastMapChangeOrPlayerWasConnected );
			if ( iIdleSeconds >= mp_mapcycle_empty_timeout_seconds.GetInt() )
			{

				Log( "Server has been empty for %d seconds on this map, cycling map as per mp_mapcycle_empty_timeout_seconds\n", iIdleSeconds );
				ChangeLevel();
			}
		}
	}


	//=========================================================
	//=========================================================
	bool CMultiplayRules::IsDeathmatch( void )
	{
		return true;
	}

	//=========================================================
	//=========================================================
	bool CMultiplayRules::IsCoOp( void )
	{
		return false;
	}

	//=========================================================
	//=========================================================
	bool CMultiplayRules::FShouldSwitchWeapon( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon )
	{
		if ( !pPlayer->Weapon_CanSwitchTo( pWeapon ) )
		{
			// Can't switch weapons for some reason.
			return false;
		}

		if ( !pPlayer->GetActiveWeapon() )
		{
			// Player doesn't have an active item, might as well switch.
			return true;
		}

		if ( !pWeapon->AllowsAutoSwitchTo() )
		{
			// The given weapon should not be auto switched to from another weapon.
			return false;
		}

		if ( !pPlayer->GetActiveWeapon()->AllowsAutoSwitchFrom() )
		{
			// The active weapon does not allow autoswitching away from it.
			return false;
		}

		if ( pWeapon->GetWeight() > pPlayer->GetActiveWeapon()->GetWeight() )
		{
			return true;
		}

		return false;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Returns the weapon in the player's inventory that would be better than
	//			the given weapon.
	//-----------------------------------------------------------------------------
	CBaseCombatWeapon *CMultiplayRules::GetNextBestWeapon( CBaseCombatCharacter *pPlayer, CBaseCombatWeapon *pCurrentWeapon )
	{
		CBaseCombatWeapon *pCheck;
		CBaseCombatWeapon *pBest;// this will be used in the event that we don't find a weapon in the same category.

		int iCurrentWeight = -1;
		int iBestWeight = -1;// no weapon lower than -1 can be autoswitched to
		pBest = NULL;

		// If I have a weapon, make sure I'm allowed to holster it
		if ( pCurrentWeapon )
		{
			if ( !pCurrentWeapon->AllowsAutoSwitchFrom() || !pCurrentWeapon->CanHolster() )
			{
				// Either this weapon doesn't allow autoswitching away from it or I
				// can't put this weapon away right now, so I can't switch.
				return NULL;
			}

			iCurrentWeight = pCurrentWeapon->GetWeight();
		}

		for ( int i = 0 ; i < pPlayer->WeaponCount(); ++i )
		{
			pCheck = pPlayer->GetWeapon( i );
			if ( !pCheck )
				continue;

			// If we have an active weapon and this weapon doesn't allow autoswitching away
			// from another weapon, skip it.
			if ( pCurrentWeapon && !pCheck->AllowsAutoSwitchTo() )
				continue;

			if ( pCheck->GetWeight() > -1 && pCheck->GetWeight() == iCurrentWeight && pCheck != pCurrentWeapon )
			{
				// this weapon is from the same category. 
				if ( pCheck->HasAnyAmmo() )
				{
					if ( pPlayer->Weapon_CanSwitchTo( pCheck ) )
					{
						return pCheck;
					}
				}
			}
			else if ( pCheck->GetWeight() > iBestWeight && pCheck != pCurrentWeapon )// don't reselect the weapon we're trying to get rid of
			{
				//Msg( "Considering %s\n", STRING( pCheck->GetClassname() );
				// we keep updating the 'best' weapon just in case we can't find a weapon of the same weight
				// that the player was using. This will end up leaving the player with his heaviest-weighted 
				// weapon. 
				if ( pCheck->HasAnyAmmo() )
				{
					// if this weapon is useable, flag it as the best
					iBestWeight = pCheck->GetWeight();
					pBest = pCheck;
				}
			}
		}

		// if we make it here, we've checked all the weapons and found no useable 
		// weapon in the same catagory as the current weapon. 
		
		// if pBest is null, we didn't find ANYTHING. Shouldn't be possible- should always 
		// at least get the crowbar, but ya never know.
		return pBest;
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	// Output : Returns true on success, false on failure.
	//-----------------------------------------------------------------------------
	bool CMultiplayRules::SwitchToNextBestWeapon( CBaseCombatCharacter *pPlayer, CBaseCombatWeapon *pCurrentWeapon )
	{
		CBaseCombatWeapon *pWeapon = GetNextBestWeapon( pPlayer, pCurrentWeapon );

		if ( pWeapon != NULL )
			return pPlayer->Weapon_Switch( pWeapon );
		
		return false;
	}

	//=========================================================
	//=========================================================
	bool CMultiplayRules::ClientConnected( edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen )
	{
		GetVoiceGameMgr()->ClientConnected( pEntity );
		return true;
	}

	void CMultiplayRules::InitHUD( CBasePlayer *pl )
	{
	} 

	//=========================================================
	//=========================================================
	void CMultiplayRules::ClientDisconnected( edict_t *pClient )
	{
		if ( pClient )
		{
			CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pClient );

			if ( pPlayer )
			{
				FireTargets( "game_playerleave", pPlayer, pPlayer, USE_TOGGLE, 0 );

				pPlayer->RemoveAllItems( true );// destroy all of the players weapons and items

				// Kill off view model entities
				pPlayer->DestroyViewModels();

				pPlayer->SetConnected( PlayerDisconnected );
			}
		}
	}

	//=========================================================
	//=========================================================
	float CMultiplayRules::FlPlayerFallDamage( CBasePlayer *pPlayer )
	{
		int iFallDamage = (int)falldamage.GetFloat();

		switch ( iFallDamage )
		{
		case 1://progressive
			pPlayer->m_Local.m_flFallVelocity -= PLAYER_MAX_SAFE_FALL_SPEED;
			return pPlayer->m_Local.m_flFallVelocity * DAMAGE_FOR_FALL_SPEED;
			break;
		default:
		case 0:// fixed
			return 10;
			break;
		}
	} 

	//=========================================================
	//=========================================================
	bool CMultiplayRules::AllowDamage( CBaseEntity *pVictim, const CTakeDamageInfo &info )
	{
		return true;
	}

	//=========================================================
	//=========================================================
	bool CMultiplayRules::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
	{
		return true;
	}

	//=========================================================
	//=========================================================
	void CMultiplayRules::PlayerThink( CBasePlayer *pPlayer )
	{
		if ( g_fGameOver )
		{
			// clear attack/use commands from player
			pPlayer->m_afButtonPressed = 0;
			pPlayer->m_nButtons = 0;
			pPlayer->m_afButtonReleased = 0;
		}
	}

	//=========================================================
	//=========================================================
	void CMultiplayRules::PlayerSpawn( CBasePlayer *pPlayer )
	{
		bool		addDefault;
		CBaseEntity	*pWeaponEntity = NULL;

		pPlayer->EquipSuit();
		
		addDefault = true;

		while ( (pWeaponEntity = gEntList.FindEntityByClassname( pWeaponEntity, "game_player_equip" )) != NULL)
		{
			pWeaponEntity->Touch( pPlayer );
			addDefault = false;
		}
	}

	//=========================================================
	//=========================================================
	bool CMultiplayRules::FPlayerCanRespawn( CBasePlayer *pPlayer )
	{
		return true;
	}

	//=========================================================
	//=========================================================
	float CMultiplayRules::FlPlayerSpawnTime( CBasePlayer *pPlayer )
	{
		return gpGlobals->curtime;//now!
	}

	bool CMultiplayRules::AllowAutoTargetCrosshair( void )
	{
		return ( aimcrosshair.GetInt() != 0 );
	}

	//=========================================================
	// IPointsForKill - how many points awarded to anyone
	// that kills this player?
	//=========================================================
	int CMultiplayRules::IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled )
	{
		return 1;
	}

	//-----------------------------------------------------------------------------
	// Purpose: 
	//-----------------------------------------------------------------------------
	CBasePlayer *CMultiplayRules::GetDeathScorer( CBaseEntity *pKiller, CBaseEntity *pInflictor )
	{
		if ( pKiller)
		{
			if ( pKiller->Classify() == CLASS_PLAYER )
				return (CBasePlayer*)pKiller;

			// Killing entity might be specifying a scorer player
			IScorer *pScorerInterface = dynamic_cast<IScorer*>( pKiller );
			if ( pScorerInterface )
			{
				CBasePlayer *pPlayer = pScorerInterface->GetScorer();
				if ( pPlayer )
					return pPlayer;
			}

			// Inflicting entity might be specifying a scoring player
			pScorerInterface = dynamic_cast<IScorer*>( pInflictor );
			if ( pScorerInterface )
			{
				CBasePlayer *pPlayer = pScorerInterface->GetScorer();
				if ( pPlayer )
					return pPlayer;
			}
		}

		return NULL;
	}

	//-----------------------------------------------------------------------------
	// Purpose: Returns player who should receive credit for kill
	//-----------------------------------------------------------------------------
	CBasePlayer *CMultiplayRules::GetDeathScorer( CBaseEntity *pKiller, CBaseEntity *pInflictor, CBaseEntity *pVictim )
	{
		// if this method not overridden by subclass, just call our default implementation
		return GetDeathScorer( pKiller, pInflictor );
	}

	//=========================================================
	// PlayerKilled - someone/something killed this player
	//=========================================================
	void CMultiplayRules::PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info )
	{
		DeathNotice( pVictim, info );

		// Find the killer & the scorer
		CBaseEntity *pInflictor = info.GetInflictor();
		CBaseEntity *pKiller = info.GetAttacker();
		CBasePlayer *pScorer = GetDeathScorer( pKiller, pInflictor, pVictim );
		
		pVictim->IncrementDeathCount( 1 );

		// dvsents2: uncomment when removing all FireTargets
		// variant_t value;
		// g_EventQueue.AddEvent( "game_playerdie", "Use", value, 0, pVictim, pVictim );
		FireTargets( "game_playerdie", pVictim, pVictim, USE_TOGGLE, 0 );

		// Did the player kill himself?
		if ( pVictim == pScorer )  
		{			
			if ( UseSuicidePenalty() )
			{
				// Players lose a frag for killing themselves
				pVictim->IncrementFragCount( -1 );
			}			
		}
		else if ( pScorer )
		{
			// if a player dies in a deathmatch game and the killer is a client, award the killer some points
			pScorer->IncrementFragCount( IPointsForKill( pScorer, pVictim ) );
			
			// Allow the scorer to immediately paint a decal
			pScorer->AllowImmediateDecalPainting();

			// dvsents2: uncomment when removing all FireTargets
			//variant_t value;
			//g_EventQueue.AddEvent( "game_playerkill", "Use", value, 0, pScorer, pScorer );
			FireTargets( "game_playerkill", pScorer, pScorer, USE_TOGGLE, 0 );
		}
		else
		{  
			if ( UseSuicidePenalty() )
			{
				// Players lose a frag for letting the world kill them			
				pVictim->IncrementFragCount( -1 );
			}					
		}
	}

	//=========================================================
	// Deathnotice. 
	//=========================================================
	void CMultiplayRules::DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info )
	{
		// Work out what killed the player, and send a message to all clients about it
		const char *killer_weapon_name = "world";		// by default, the player is killed by the world
		int killer_ID = 0;

		// Find the killer & the scorer
		CBaseEntity *pInflictor = info.GetInflictor();
		CBaseEntity *pKiller = info.GetAttacker();
		CBasePlayer *pScorer = GetDeathScorer( pKiller, pInflictor, pVictim );

		// Custom damage type?
		if ( info.GetDamageCustom() )
		{
			killer_weapon_name = GetDamageCustomString( info );
			if ( pScorer )
			{
				killer_ID = pScorer->GetUserID();
			}
		}
		else
		{
			// Is the killer a client?
			if ( pScorer )
			{
				killer_ID = pScorer->GetUserID();
				
				if ( pInflictor )
				{
					if ( pInflictor == pScorer )
					{
						// If the inflictor is the killer,  then it must be their current weapon doing the damage
						if ( pScorer->GetActiveWeapon() )
						{
							killer_weapon_name = pScorer->GetActiveWeapon()->GetDeathNoticeName();
						}
					}
					else
					{
						killer_weapon_name = STRING( pInflictor->m_iClassname );  // it's just that easy
					}
				}
			}
			else
			{
				killer_weapon_name = STRING( pInflictor->m_iClassname );
			}

			// strip the NPC_* or weapon_* from the inflictor's classname
			if ( strncmp( killer_weapon_name, "weapon_", 7 ) == 0 )
			{
				killer_weapon_name += 7;
			}
			else if ( strncmp( killer_weapon_name, "NPC_", 8 ) == 0 )
			{
				killer_weapon_name += 8;
			}
			else if ( strncmp( killer_weapon_name, "func_", 5 ) == 0 )
			{
				killer_weapon_name += 5;
			}
		}

		IGameEvent * event = gameeventmanager->CreateEvent( "player_death" );
		if ( event )
		{
			event->SetInt("userid", pVictim->GetUserID() );
			event->SetInt("attacker", killer_ID );
			event->SetInt("customkill", info.GetDamageCustom() );
			event->SetInt("priority", 7 );	// HLTV event priority, not transmitted
			
			gameeventmanager->FireEvent( event );
		}

	}

	//=========================================================
	// FlWeaponRespawnTime - what is the time in the future
	// at which this weapon may spawn?
	//=========================================================
	float CMultiplayRules::FlWeaponRespawnTime( CBaseCombatWeapon *pWeapon )
	{
		if ( weaponstay.GetInt() > 0 )
		{
			// make sure it's only certain weapons
			if ( !(pWeapon->GetWeaponFlags() & ITEM_FLAG_LIMITINWORLD) )
			{
				return gpGlobals->curtime + 0;		// weapon respawns almost instantly
			}
		}

		return gpGlobals->curtime + WEAPON_RESPAWN_TIME;
	}

	// when we are within this close to running out of entities,  items 
	// marked with the ITEM_FLAG_LIMITINWORLD will delay their respawn
	#define ENTITY_INTOLERANCE	100

	//=========================================================
	// FlWeaponRespawnTime - Returns 0 if the weapon can respawn 
	// now,  otherwise it returns the time at which it can try
	// to spawn again.
	//=========================================================
	float CMultiplayRules::FlWeaponTryRespawn( CBaseCombatWeapon *pWeapon )
	{
		if ( pWeapon && (pWeapon->GetWeaponFlags() & ITEM_FLAG_LIMITINWORLD) )
		{
			if ( gEntList.NumberOfEntities() < (gpGlobals->maxEntities - ENTITY_INTOLERANCE) )
				return 0;

			// we're past the entity tolerance level,  so delay the respawn
			return FlWeaponRespawnTime( pWeapon );
		}

		return 0;
	}

	//=========================================================
	// VecWeaponRespawnSpot - where should this weapon spawn?
	// Some game variations may choose to randomize spawn locations
	//=========================================================
	Vector CMultiplayRules::VecWeaponRespawnSpot( CBaseCombatWeapon *pWeapon )
	{
		return pWeapon->GetAbsOrigin();
	}

	//=========================================================
	// WeaponShouldRespawn - any conditions inhibiting the
	// respawning of this weapon?
	//=========================================================
	int CMultiplayRules::WeaponShouldRespawn( CBaseCombatWeapon *pWeapon )
	{
		if ( pWeapon->HasSpawnFlags( SF_NORESPAWN ) )
		{
			return GR_WEAPON_RESPAWN_NO;
		}

		return GR_WEAPON_RESPAWN_YES;
	}

	//=========================================================
	// CanHaveWeapon - returns false if the player is not allowed
	// to pick up this weapon
	//=========================================================
	bool CMultiplayRules::CanHavePlayerItem( CBasePlayer *pPlayer, CBaseCombatWeapon *pItem )
	{
		if ( weaponstay.GetInt() > 0 )
		{
			if ( pItem->GetWeaponFlags() & ITEM_FLAG_LIMITINWORLD )
				return BaseClass::CanHavePlayerItem( pPlayer, pItem );

			// check if the player already has this weapon
			for ( int i = 0 ; i < pPlayer->WeaponCount() ; i++ )
			{
				if ( pPlayer->GetWeapon(i) == pItem )
				{
					return false;
				}
			}
		}

		return BaseClass::CanHavePlayerItem( pPlayer, pItem );
	}

	//=========================================================
	//=========================================================
	bool CMultiplayRules::CanHaveItem( CBasePlayer *pPlayer, CItem *pItem )
	{
		return true;
	}

	//=========================================================
	//=========================================================
	void CMultiplayRules::PlayerGotItem( CBasePlayer *pPlayer, CItem *pItem )
	{
	}

	//=========================================================
	//=========================================================
	int CMultiplayRules::ItemShouldRespawn( CItem *pItem )
	{
		if ( pItem->HasSpawnFlags( SF_NORESPAWN ) )
		{
			return GR_ITEM_RESPAWN_NO;
		}

		return GR_ITEM_RESPAWN_YES;
	}


	//=========================================================
	// At what time in the future may this Item respawn?
	//=========================================================
	float CMultiplayRules::FlItemRespawnTime( CItem *pItem )
	{
		return gpGlobals->curtime + ITEM_RESPAWN_TIME;
	}

	//=========================================================
	// Where should this item respawn?
	// Some game variations may choose to randomize spawn locations
	//=========================================================
	Vector CMultiplayRules::VecItemRespawnSpot( CItem *pItem )
	{
		return pItem->GetAbsOrigin();
	}

	//=========================================================
	// What angles should this item use to respawn?
	//=========================================================
	QAngle CMultiplayRules::VecItemRespawnAngles( CItem *pItem )
	{
		return pItem->GetAbsAngles();
	}

	//=========================================================
	//=========================================================
	void CMultiplayRules::PlayerGotAmmo( CBaseCombatCharacter *pPlayer, char *szName, int iCount )
	{
	}

	//=========================================================
	//=========================================================
	bool CMultiplayRules::IsAllowedToSpawn( CBaseEntity *pEntity )
	{
	//	if ( pEntity->GetFlags() & FL_NPC )
	//		return false;

		return true;
	}


	//=========================================================
	//=========================================================
	float CMultiplayRules::FlHealthChargerRechargeTime( void )
	{
		return 60;
	}


	float CMultiplayRules::FlHEVChargerRechargeTime( void )
	{
		return 30;
	}

	//=========================================================
	//=========================================================
	int CMultiplayRules::DeadPlayerWeapons( CBasePlayer *pPlayer )
	{
		return GR_PLR_DROP_GUN_ACTIVE;
	}

	//=========================================================
	//=========================================================
	int CMultiplayRules::DeadPlayerAmmo( CBasePlayer *pPlayer )
	{
		return GR_PLR_DROP_AMMO_ACTIVE;
	}

	CBaseEntity *CMultiplayRules::GetPlayerSpawnSpot( CBasePlayer *pPlayer )
	{
		CBaseEntity *pentSpawnSpot = BaseClass::GetPlayerSpawnSpot( pPlayer );	

	//!! replace this with an Event
	/*
		if ( IsMultiplayer() && pentSpawnSpot->m_target )
		{
			FireTargets( STRING(pentSpawnSpot->m_target), pPlayer, pPlayer, USE_TOGGLE, 0 ); // dvsents2: what is this code supposed to do?
		}
	*/

		return pentSpawnSpot;
	}


	//=========================================================
	//=========================================================
	bool CMultiplayRules::PlayerCanHearChat( CBasePlayer *pListener, CBasePlayer *pSpeaker )
	{
		return ( PlayerRelationship( pListener, pSpeaker ) == GR_TEAMMATE );
	}

	int CMultiplayRules::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
	{
		// half life deathmatch has only enemies
		return GR_NOTTEAMMATE;
	}

	bool CMultiplayRules::PlayFootstepSounds( CBasePlayer *pl )
	{
		if ( footsteps.GetInt() == 0 )
			return false;

		if ( pl->IsOnLadder() || pl->GetAbsVelocity().Length2D() > 220 )
			return true;  // only make step sounds in multiplayer if the player is moving fast enough

		return false;
	}

	bool CMultiplayRules::FAllowFlashlight( void ) 
	{ 
		return flashlight.GetInt() != 0; 
	}

	//=========================================================
	//=========================================================
	bool CMultiplayRules::FAllowNPCs( void )
	{
		return true; // E3 hack
		return ( allowNPCs.GetInt() != 0 );
	}

	//=========================================================
	//======== CMultiplayRules private functions ===========

	void CMultiplayRules::GoToIntermission( void )
	{
		if ( g_fGameOver )
			return;

		g_fGameOver = true;

		float flWaitTime = mp_chattime.GetInt();

		if ( tv_delaymapchange.GetBool() )
		{
			if ( HLTVDirector()->IsActive() )	
				flWaitTime = MAX( flWaitTime, HLTVDirector()->GetDelay() );
		}
				
		m_flIntermissionEndTime = gpGlobals->curtime + flWaitTime;

		for ( int i = 1; i <= MAX_PLAYERS; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

			if ( !pPlayer )
				continue;

			pPlayer->ShowViewPortPanel( PANEL_SCOREBOARD );
		}
	}

	void StripChar(char *szBuffer, const char cWhiteSpace )
	{

		while ( char *pSpace = strchr( szBuffer, cWhiteSpace ) )
		{
			char *pNextChar = pSpace + sizeof(char);
			V_strcpy( pSpace, pNextChar );
		}
	}

	void CMultiplayRules::GetNextLevelName( char *pszNextMap, int bufsize, bool bRandom /* = false */ )
	{
		char mapcfile[256];
		DetermineMapCycleFilename( mapcfile, sizeof(mapcfile), false );

		// Check the time of the mapcycle file and re-populate the list of level names if the file has been modified
		const int nMapCycleTimeStamp = filesystem->GetPathTime( mapcfile, "GAME" );

		if ( 0 == nMapCycleTimeStamp )
		{
			// Map cycle file does not exist, make a list containing only the current map
			char *szCurrentMapName = new char[MAX_MAP_NAME];
			Q_strncpy( szCurrentMapName, STRING(gpGlobals->mapname), MAX_MAP_NAME );
			m_MapList.AddToTail( szCurrentMapName );
		}
		else
		{
			// If map cycle file has changed or this is the first time through ...
			if ( m_nMapCycleTimeStamp != nMapCycleTimeStamp )
			{
				// Reset map index and map cycle timestamp
				m_nMapCycleTimeStamp = nMapCycleTimeStamp;
				m_nMapCycleindex = 0;

				LoadMapCycleFile();
			}
		}

		// If somehow we have no maps in the list then add the current one
		if ( 0 == m_MapList.Count() )
		{
			char *szDefaultMapName = new char[MAX_MAP_NAME];
			Q_strncpy( szDefaultMapName, STRING(gpGlobals->mapname), MAX_MAP_NAME );
			m_MapList.AddToTail( szDefaultMapName );
		}

		if ( bRandom )
		{
			m_nMapCycleindex = RandomInt( 0, m_MapList.Count() - 1 );
		}

		// Here's the return value
		Q_strncpy( pszNextMap, m_MapList[m_nMapCycleindex], bufsize);
	}

	void CMultiplayRules::DetermineMapCycleFilename( char *pszResult, int nSizeResult, bool bForceSpew )
	{
		static char szLastResult[ 256];

		const char *pszVar = mapcyclefile.GetString();
		if ( *pszVar == '\0' )
		{
			if ( bForceSpew || V_stricmp( szLastResult, "__novar") )
			{
				Msg( "mapcyclefile convar not set.\n" );
				V_strcpy_safe( szLastResult, "__novar" );
			}
			*pszResult = '\0';
			return;
		}

		char szRecommendedName[ 256 ];
		V_sprintf_safe( szRecommendedName, "cfg/%s", pszVar );

		// First, look for a mapcycle file in the cfg directory, which is preferred
		V_strncpy( pszResult, szRecommendedName, nSizeResult );
		if ( filesystem->FileExists( pszResult, "GAME" ) )
		{
			if ( bForceSpew || V_stricmp( szLastResult, pszResult) )
			{
				Msg( "Using map cycle file '%s'.\n", pszResult );
				V_strcpy_safe( szLastResult, pszResult );
			}
			return;
		}

		// Nope?  Try the root.
		V_strncpy( pszResult, pszVar, nSizeResult );
		if ( filesystem->FileExists( pszResult, "GAME" ) )
		{
			if ( bForceSpew || V_stricmp( szLastResult, pszResult) )
			{
				Msg( "Using map cycle file '%s'.  ('%s' was not found.)\n", pszResult, szRecommendedName );
				V_strcpy_safe( szLastResult, pszResult );
			}
			return;
		}

		// Nope?  Use the default.
		if ( !V_stricmp( pszVar, "mapcycle.txt" ) )
		{
			V_strncpy( pszResult, "cfg/mapcycle_default.txt", nSizeResult );
			if ( filesystem->FileExists( pszResult, "GAME" ) )
			{
				if ( bForceSpew || V_stricmp( szLastResult, pszResult) )
				{
					Msg( "Using map cycle file '%s'.  ('%s' was not found.)\n", pszResult, szRecommendedName );
					V_strcpy_safe( szLastResult, pszResult );
				}
				return;
			}
		}

		// Failed
		*pszResult = '\0';
		if ( bForceSpew || V_stricmp( szLastResult, "__notfound") )
		{
			Msg( "Map cycle file '%s' was not found.\n", szRecommendedName );
			V_strcpy_safe( szLastResult, "__notfound" );
		}
	}

	void CMultiplayRules::LoapMapCycleFileIntoVector( const char *pszMapCycleFile, CUtlVector<char *> &mapList )
	{
		CUtlBuffer buf;
		if ( !filesystem->ReadFile( pszMapCycleFile, "GAME", buf ) )
			return;
		buf.PutChar( 0 );
		V_SplitString( (char*)buf.Base(), "\n", mapList );

		for ( int i = 0; i < mapList.Count(); i++ )
		{
			bool bIgnore = false;

			// Strip out the spaces in the name
			StripChar( mapList[i] , '\r');
			StripChar( mapList[i] , ' ');

			if ( !Q_strncmp( mapList[i], "//", 2 ) || mapList[i][0] == '\0' )
			{
				bIgnore = true;
			}
			else if ( !engine->IsMapValid( mapList[i] ) )
			{
				bIgnore = true;

				// If the engine doesn't consider it a valid map remove it from the lists
				Warning( "Invalid map '%s' included in map cycle file. Ignored.\n", mapList[i] );
			}

			if ( bIgnore )
			{
				delete [] mapList[i];
				mapList.Remove( i );
				--i;
			}
		}
	}

	void CMultiplayRules::FreeMapCycleFileVector( CUtlVector<char *> &mapList )
	{
		// Clear out existing map list. Not using Purge() or PurgeAndDeleteAll() because they won't delete [] each element.
		for ( int i = 0; i < mapList.Count(); i++ )
		{
			delete [] mapList[i];
		}

		mapList.RemoveAll();
	}

	bool CMultiplayRules::IsMapInMapCycle( const char *pszName )
	{
		for ( int i = 0; i < m_MapList.Count(); i++ )
		{
			if ( V_stricmp( pszName, m_MapList[i] ) == 0 )
			{
				return true;
			}
		}	

		return false;
	}

	void CMultiplayRules::ChangeLevel( void )
	{
		char szNextMap[MAX_MAP_NAME];

		if ( nextlevel.GetString() && *nextlevel.GetString() && engine->IsMapValid( nextlevel.GetString() ) )
		{
			Q_strncpy( szNextMap, nextlevel.GetString(), sizeof( szNextMap ) );
		}
		else
		{
			GetNextLevelName( szNextMap, sizeof(szNextMap) );
			IncrementMapCycleIndex();
		}

		ChangeLevelToMap( szNextMap );
	}

	void CMultiplayRules::LoadMapCycleFile( void )
	{
		char mapcfile[256];
		DetermineMapCycleFilename( mapcfile, sizeof(mapcfile), false );

		FreeMapCycleFileVector( m_MapList );

		// Repopulate map list from mapcycle file
		LoapMapCycleFileIntoVector( mapcfile, m_MapList );

		// Load server's mapcycle into network string table for client-side voting
		if ( g_pStringTableServerMapCycle )
		{
			CUtlString sFileList;
			for ( int i = 0; i < m_MapList.Count(); i++ )
			{
				sFileList += m_MapList[i];
				sFileList += '\n';
			}

			g_pStringTableServerMapCycle->AddString( CBaseEntity::IsServer(), "ServerMapCycle", sFileList.Length() + 1, sFileList.String() );
		}

#if defined ( TF_DLL ) || defined ( TF_CLIENT_DLL )
		if ( g_pStringTableServerPopFiles )
		{
			// Search for all pop files that are prefixed with the current map name
			CUtlString sFileList;

			char szBaseName[_MAX_PATH];
			V_snprintf( szBaseName, sizeof( szBaseName ), "scripts/population/%s*.pop", STRING(gpGlobals->mapname) );

			FileFindHandle_t popHandle;
			const char *pPopFileName = filesystem->FindFirst( szBaseName, &popHandle );

			while ( pPopFileName && pPopFileName[ 0 ] != '\0' )
			{
				// Skip it if it's a directory or is the folder info
				if ( filesystem->FindIsDirectory( popHandle ) )
				{
					pPopFileName = filesystem->FindNext( popHandle );
					continue;
				}

				const char *pchPopPostfix = StringAfterPrefix( pPopFileName, STRING(gpGlobals->mapname) );
				if ( pchPopPostfix )
				{
					char szShortName[_MAX_PATH];
					V_strncpy( szShortName, ( ( pchPopPostfix[ 0 ] == '_' ) ? ( pchPopPostfix + 1 ) : "normal" ), sizeof( szShortName ) ); // skip the '_'
					V_StripExtension( szShortName, szShortName, sizeof( szShortName ) );

					sFileList += szShortName;
					sFileList += '\n';
				}

				pPopFileName = filesystem->FindNext( popHandle );
			}

			filesystem->FindClose( popHandle );

			if ( sFileList.Length() > 0 )
			{
				g_pStringTableServerPopFiles->AddString( CBaseEntity::IsServer(), "ServerPopFiles", sFileList.Length() + 1, sFileList.String() );
			}
		}

		if ( g_pStringTableServerMapCycleMvM )
		{
			ConVarRef tf_mvm_missioncyclefile( "tf_mvm_missioncyclefile" );
			KeyValues *pKV = new KeyValues( tf_mvm_missioncyclefile.GetString() );
			if ( pKV->LoadFromFile( g_pFullFileSystem, tf_mvm_missioncyclefile.GetString(), "MOD" ) )
			{
				CUtlVector<CUtlString> mapList;

				// Parse the maps and send a list to each client for vote options
				int iMaxCat = pKV->GetInt( "categories", 0 );
				for ( int iCat = 1; iCat <= iMaxCat; iCat++ )
				{
					KeyValues *pCategory = pKV->FindKey( UTIL_VarArgs( "%d", iCat ), false );
					if ( pCategory )
					{
						int iMapCount = pCategory->GetInt( "count", 0 );
						for ( int iMap = 1; iMap <= iMapCount; ++iMap )
						{
							KeyValues *pMission = pCategory->FindKey( UTIL_VarArgs( "%d", iMap ), false );
							if ( pMission )
							{
								const char *pszMap = pMission->GetString( "map", "" );
								int iIdx = mapList.Find( pszMap );
								if ( !mapList.IsValidIndex( iIdx ) )
								{
									mapList.AddToTail( pszMap );
								}
							}
						}
					}
				}

				if ( mapList.Count() )
				{
					CUtlString sFileList;
					for ( int i = 0; i < mapList.Count(); i++ )
					{
						sFileList += mapList[i];
						sFileList += '\n';
					}

					g_pStringTableServerMapCycleMvM->AddString( CBaseEntity::IsServer(), "ServerMapCycleMvM", sFileList.Length() + 1, sFileList.String() );
				}

				pKV->deleteThis();
			}
		}
#endif

		// If the current map selection is in the list, set m_nMapCycleindex to the map that follows it.
		for ( int i = 0; i < m_MapList.Count(); i++ )
		{
			if ( V_strcmp( STRING( gpGlobals->mapname ), m_MapList[i] ) == 0 )
			{
				m_nMapCycleindex = i;
				IncrementMapCycleIndex();
				break;
			}
		}		
	}

	void CMultiplayRules::ChangeLevelToMap( const char *pszMap )
	{
		g_fGameOver = true;
		m_flTimeLastMapChangeOrPlayerWasConnected = 0.0f;
		Msg( "CHANGE LEVEL: %s\n", pszMap );
		engine->ChangeLevel( pszMap, NULL );
	}


#endif		


	//-----------------------------------------------------------------------------
	// Purpose: Shared script resource of voice menu commands and hud strings
	//-----------------------------------------------------------------------------
	void CMultiplayRules::LoadVoiceCommandScript( void )
	{
		KeyValues *pKV = new KeyValues( "VoiceCommands" );

		if ( pKV->LoadFromFile( filesystem, "scripts/voicecommands.txt", "GAME" ) )
		{
			for ( KeyValues *menu = pKV->GetFirstSubKey(); menu != NULL; menu = menu->GetNextKey() )
			{
				int iMenuIndex = m_VoiceCommandMenus.AddToTail();

				int iNumItems = 0;

				// for each subkey of this menu, add a menu item
				for ( KeyValues *menuitem = menu->GetFirstSubKey(); menuitem != NULL; menuitem = menuitem->GetNextKey() )
				{
					iNumItems++;

					if ( iNumItems > 9 )
					{
						Warning( "Trying to load more than 9 menu items in voicecommands.txt, extras ignored" );
						continue;
					}

					VoiceCommandMenuItem_t item;

#ifndef CLIENT_DLL
					int iConcept = GetMPConceptIndexFromString( menuitem->GetString( "concept", "" ) );
					if ( iConcept == MP_CONCEPT_NONE )
					{
						Warning( "Voicecommand script attempting to use unknown concept. Need to define new concepts in code. ( %s )\n", menuitem->GetString( "concept", "" ) );
					}
					item.m_iConcept = iConcept;

					item.m_bShowSubtitle = ( menuitem->GetInt( "show_subtitle", 0 ) > 0 );
					item.m_bDistanceBasedSubtitle = ( menuitem->GetInt( "distance_check_subtitle", 0 ) > 0 );

					Q_strncpy( item.m_szGestureActivity, menuitem->GetString( "activity", "" ), sizeof( item.m_szGestureActivity ) ); 
#else
					Q_strncpy( item.m_szSubtitle, menuitem->GetString( "subtitle", "" ), MAX_VOICE_COMMAND_SUBTITLE );
					Q_strncpy( item.m_szMenuLabel, menuitem->GetString( "menu_label", "" ), MAX_VOICE_COMMAND_SUBTITLE );

#endif
					m_VoiceCommandMenus.Element( iMenuIndex ).AddToTail( item );
				}
			}
		}

		pKV->deleteThis();
	}

#ifndef CLIENT_DLL

	void CMultiplayRules::SkipNextMapInCycle()
	{
		char szSkippedMap[MAX_MAP_NAME];
		char szNextMap[MAX_MAP_NAME];

		GetNextLevelName( szSkippedMap, sizeof( szSkippedMap ) );
		IncrementMapCycleIndex();
		GetNextLevelName( szNextMap, sizeof( szNextMap ) );

		Msg( "Skipping: %s\tNext map: %s\n", szSkippedMap, szNextMap );

		if ( nextlevel.GetString() && *nextlevel.GetString() && engine->IsMapValid( nextlevel.GetString() ) )
		{
			Msg( "Warning! \"nextlevel\" is set to \"%s\" and will override the next map to be played.\n", nextlevel.GetString() );
		}
	}

	void CMultiplayRules::IncrementMapCycleIndex()
	{
		// Reset index if we've passed the end of the map list
		if ( ++m_nMapCycleindex >= m_MapList.Count() )
		{
			m_nMapCycleindex = 0;
		}
	}

	bool CMultiplayRules::ClientCommand( CBaseEntity *pEdict, const CCommand &args )
	{
		CBasePlayer *pPlayer = ToBasePlayer( pEdict );

		const char *pcmd = args[0];
		if ( FStrEq( pcmd, "voicemenu" ) )
		{
			if ( args.ArgC() < 3 )
				return true;

			CBaseMultiplayerPlayer *pMultiPlayerPlayer = dynamic_cast< CBaseMultiplayerPlayer * >( pPlayer );

			if ( pMultiPlayerPlayer )
			{
				int iMenu = atoi( args[1] );
				int iItem = atoi( args[2] );

				VoiceCommand( pMultiPlayerPlayer, iMenu, iItem );
			}

			return true;
		}

		return BaseClass::ClientCommand( pEdict, args );
	}

	void CMultiplayRules::ClientCommandKeyValues( edict_t *pEntity, KeyValues *pKeyValues )
	{
		CBaseMultiplayerPlayer *pPlayer = dynamic_cast< CBaseMultiplayerPlayer * >( CBaseEntity::Instance( pEntity ) );

		if ( !pPlayer )
			return;

		char const *pszCommand = pKeyValues->GetName();
		if ( pszCommand && pszCommand[0] )
		{
			if ( FStrEq( pszCommand, "AchievementEarned" ) )
			{
				if ( pPlayer->ShouldAnnounceAchievement() )
				{
					int nAchievementID = pKeyValues->GetInt( "achievementID" );

					IGameEvent * event = gameeventmanager->CreateEvent( "achievement_earned" );
					if ( event )
					{
						event->SetInt( "player", pPlayer->entindex() );
						event->SetInt( "achievement", nAchievementID );
						gameeventmanager->FireEvent( event );
					}

					pPlayer->OnAchievementEarned( nAchievementID );
				}
			}
			else if ( FStrEq( pszCommand, "SendServerMapCycle" ) )
			{
				LoadMapCycleFile();	
			}
		}
	}

	VoiceCommandMenuItem_t *CMultiplayRules::VoiceCommand( CBaseMultiplayerPlayer *pPlayer, int iMenu, int iItem )
	{
		// have the player speak the concept that is in a particular menu slot
		if ( !pPlayer )
			return NULL;

		if ( iMenu < 0 || iMenu >= m_VoiceCommandMenus.Count() )
			return NULL;

		if ( iItem < 0 || iItem >= m_VoiceCommandMenus.Element( iMenu ).Count() )
			return NULL;

		VoiceCommandMenuItem_t *pItem = &m_VoiceCommandMenus.Element( iMenu ).Element( iItem );

		Assert( pItem );

		char szResponse[AI_Response::MAX_RESPONSE_NAME];

		if ( pPlayer->CanSpeakVoiceCommand() )
		{
			CMultiplayer_Expresser *pExpresser = pPlayer->GetMultiplayerExpresser();
			Assert( pExpresser );
			pExpresser->AllowMultipleScenes();

			if ( pPlayer->SpeakConceptIfAllowed( pItem->m_iConcept, NULL, szResponse, AI_Response::MAX_RESPONSE_NAME ) )
			{
				// show a subtitle if we need to
				if ( pItem->m_bShowSubtitle )
				{
					CRecipientFilter filter;

					if ( pItem->m_bDistanceBasedSubtitle )
					{
						filter.AddRecipientsByPAS( pPlayer->WorldSpaceCenter() );

						// further reduce the range to a certain radius
						int i;
						for ( i = filter.GetRecipientCount()-1; i >= 0; i-- )
						{
							int index = filter.GetRecipientIndex(i);

							CBasePlayer *pListener = UTIL_PlayerByIndex( index );

							if ( pListener && pListener != pPlayer )
							{
								float flDist = ( pListener->WorldSpaceCenter() - pPlayer->WorldSpaceCenter() ).Length2D();

								if ( flDist > VOICE_COMMAND_MAX_SUBTITLE_DIST )
									filter.RemoveRecipientByPlayerIndex( index );
							}
						}
					}
					else
					{
						filter.AddAllPlayers();
					}

					// if we aren't a disguised spy
					if ( !pPlayer->ShouldShowVoiceSubtitleToEnemy() )
					{
						// remove players on other teams
						filter.RemoveRecipientsNotOnTeam( pPlayer->GetTeam() );
					}

					// Register this event in the mod-specific usermessages .cpp file if you hit this assert
					Assert( usermessages->LookupUserMessage( "VoiceSubtitle" ) != -1 );

					// Send a subtitle to anyone in the PAS
					UserMessageBegin( filter, "VoiceSubtitle" );
						WRITE_BYTE( pPlayer->entindex() );
						WRITE_BYTE( iMenu );
						WRITE_BYTE( iItem );
					MessageEnd();
				}

				pPlayer->NoteSpokeVoiceCommand( szResponse );

#ifdef NEXT_BOT
				// let bots react to player's voice commands
				CUtlVector< INextBot * > botVector;
				TheNextBots().CollectAllBots( &botVector );

				for( int i=0; i<botVector.Count(); ++i )
				{
					botVector[i]->OnActorEmoted( pPlayer, pItem->m_iConcept );
				}
#endif
			}
			else
			{
				pItem = NULL;
			}

			pExpresser->DisallowMultipleScenes();
			return pItem;
		}

		return NULL;
	}

	bool CMultiplayRules::IsLoadingBugBaitReport()
	{
		return ( !engine->IsDedicatedServer()&& CommandLine()->CheckParm( "-bugbait" ) && sv_cheats->GetBool() );
	}

	void CMultiplayRules::HaveAllPlayersSpeakConceptIfAllowed( int iConcept, int iTeam /* = TEAM_UNASSIGNED */, const char *modifiers /* = NULL */ )
	{
		CBaseMultiplayerPlayer *pPlayer;
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			pPlayer = ToBaseMultiplayerPlayer( UTIL_PlayerByIndex( i ) );

			if ( !pPlayer )
				continue;

			if ( iTeam != TEAM_UNASSIGNED )
			{
				if ( pPlayer->GetTeamNumber() != iTeam )
					continue;
			}

			pPlayer->SpeakConceptIfAllowed( iConcept, modifiers );
		}
	}

	void CMultiplayRules::ClientSettingsChanged( CBasePlayer *pPlayer )
	{
		// NVNT see if this user is still or has began using a haptic device
		const char *pszHH = engine->GetClientConVarValue( pPlayer->entindex(), "hap_HasDevice" );
		if( pszHH )
		{
			int iHH = atoi( pszHH );
			pPlayer->SetHaptics( iHH != 0 );
		}

	}
	void CMultiplayRules::GetTaggedConVarList( KeyValues *pCvarTagList )
	{
		BaseClass::GetTaggedConVarList( pCvarTagList );

		// sv_gravity
		KeyValues *pGravity = new KeyValues( "sv_gravity" );
		pGravity->SetString( "convar", "sv_gravity" );
		pGravity->SetString( "tag", "gravity" );

		pCvarTagList->AddSubKey( pGravity );

		// sv_alltalk
		KeyValues *pAllTalk = new KeyValues( "sv_alltalk" );
		pAllTalk->SetString( "convar", "sv_alltalk" );
		pAllTalk->SetString( "tag", "alltalk" );

		pCvarTagList->AddSubKey( pAllTalk );
	}

#else

	const char *CMultiplayRules::GetVoiceCommandSubtitle( int iMenu, int iItem )
	{
		Assert( iMenu >= 0 && iMenu < m_VoiceCommandMenus.Count() );
		if ( iMenu < 0 || iMenu >= m_VoiceCommandMenus.Count() )
			return "";

		Assert( iItem >= 0 && iItem < m_VoiceCommandMenus.Element( iMenu ).Count() );
		if ( iItem < 0 || iItem >= m_VoiceCommandMenus.Element( iMenu ).Count() )
			return "";

		VoiceCommandMenuItem_t *pItem = &m_VoiceCommandMenus.Element( iMenu ).Element( iItem );

		Assert( pItem );

		return pItem->m_szSubtitle;
	}

	// Returns false if no such menu is declared or if it's an empty menu
	bool CMultiplayRules::GetVoiceMenuLabels( int iMenu, KeyValues *pKV )
	{
		Assert( iMenu >= 0 && iMenu < m_VoiceCommandMenus.Count() );
		if ( iMenu < 0 || iMenu >= m_VoiceCommandMenus.Count() )
			return false;

		int iNumItems = m_VoiceCommandMenus.Element( iMenu ).Count();

		for ( int i=0; i<iNumItems; i++ )
		{
			VoiceCommandMenuItem_t *pItem = &m_VoiceCommandMenus.Element( iMenu ).Element( i );

			KeyValues *pLabelKV = new KeyValues( pItem->m_szMenuLabel );

			pKV->AddSubKey( pLabelKV );
		}

		return iNumItems > 0;
	}

#endif

// HL1

extern DLL_GLOBAL CGameRulesHL1	*g_pGameRulesHL1;
extern DLL_GLOBAL BOOL	g_fGameOverHL1;
extern int gmsgDeathMsg;	// client dll messages
extern int gmsgScoreInfo;
extern int gmsgMOTD;
extern int gmsgServerName;

extern int g_teamplay;

#define ITEM_RESPAWN_TIME	30
#define WEAPON_RESPAWN_TIME	20
#define AMMO_RESPAWN_TIME	20

float g_flIntermissionStartTime = 0;

CVoiceGameMgr	g_VoiceGameMgrHL1;

class CMultiplayGameMgrHelper : public IVoiceGameMgrHelper
{
public:
	virtual bool		CanPlayerHearPlayer(CBasePlayerHL1 *pListener, CBasePlayerHL1 *pTalker)
	{
		if ( g_teamplay )
		{
			if ( g_pGameRulesHL1->PlayerRelationship( pListener, pTalker ) != GR_TEAMMATE )
			{
				return false;
			}
		}

		return true;
	}
};
static CMultiplayGameMgrHelper g_GameMgrHelper;

//*********************************************************
// Rules for the half-life multiplayer game.
//*********************************************************

CHalfLifeMultiplay :: CHalfLifeMultiplay()
{
	g_VoiceGameMgrHL1.Init(&g_GameMgrHelper, gpGlobalsHL1->maxClients);

	RefreshSkillData();
	m_flIntermissionEndTime = 0;
	g_flIntermissionStartTime = 0;
	
	// 11/8/98
	// Modified by YWB:  Server .cfg file is now a cvar, so that 
	//  server ops can run multiple game servers, with different server .cfg files,
	//  from a single installed directory.
	// Mapcyclefile is already a cvar.

	// 3/31/99
	// Added lservercfg file cvar, since listen and dedicated servers should not
	// share a single config file. (sjb)
	if ( IS_DEDICATED_SERVER() )
	{
		// dedicated server
		char *servercfgfile = (char *)CVAR_GET_STRING( "servercfgfile" );

		if ( servercfgfile && servercfgfile[0] )
		{
			char szCommand[256];
			
			ALERT( at_console, "Executing dedicated server config file\n" );
			sprintf( szCommand, "exec %s\n", servercfgfile );
			SERVER_COMMAND( szCommand );
		}
	}
	else
	{
		// listen server
		char *lservercfgfile = (char *)CVAR_GET_STRING( "lservercfgfile" );

		if ( lservercfgfile && lservercfgfile[0] )
		{
			char szCommand[256];
			
			ALERT( at_console, "Executing listen server config file\n" );
			sprintf( szCommand, "exec %s\n", lservercfgfile );
			SERVER_COMMAND( szCommand );
		}
	}
}

BOOL CHalfLifeMultiplay::ClientCommandHL1( CBasePlayerHL1 *pPlayer, const char *pcmd )
{
	if(g_VoiceGameMgrHL1.ClientCommandHL1(pPlayer, pcmd))
		return TRUE;

	return CGameRulesHL1::ClientCommandHL1(pPlayer, pcmd);
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::RefreshSkillData( void )
{
// load all default values
	CGameRulesHL1::RefreshSkillData();

// override some values for multiplay.

	// suitcharger
	gSkillData.suitchargerCapacity = 30;

	// Crowbar whack
	gSkillData.plrDmgCrowbar = 25;

	// Glock Round
	gSkillData.plrDmg9MM = 12;

	// 357 Round
	gSkillData.plrDmg357 = 40;

	// MP5 Round
	gSkillData.plrDmgMP5 = 12;

	// M203 grenade
	gSkillData.plrDmgM203Grenade = 100;

	// Shotgun buckshot
	gSkillData.plrDmgBuckshot = 20;// fewer pellets in deathmatch

	// Crossbow
	gSkillData.plrDmgCrossbowClient = 20;

	// RPG
	gSkillData.plrDmgRPG = 120;

	// Egon
	gSkillData.plrDmgEgonWide = 20;
	gSkillData.plrDmgEgonNarrow = 10;

	// Hand Grendade
	gSkillData.plrDmgHandGrenade = 100;

	// Satchel Charge
	gSkillData.plrDmgSatchel = 120;

	// Tripmine
	gSkillData.plrDmgTripmine = 150;

	// hornet
	gSkillData.plrDmgHornet = 10;
}

// longest the intermission can last, in seconds
#define MAX_INTERMISSION_TIME		120

extern cvar_t timeleft, fragsleft;

extern cvar_t mp_chattime_hl1;

//=========================================================
//=========================================================
void CHalfLifeMultiplay :: Think ( void )
{
	g_VoiceGameMgrHL1.Update(gpGlobalsHL1->frametime);

	///// Check game rules /////
	static int last_frags;
	static int last_time;

	int frags_remaining = 0;
	int time_remaining = 0;

	if ( g_fGameOverHL1 )   // someone else quit the game already
	{
		// bounds check
		int time = (int)CVAR_GET_FLOAT( "mp_chattime_hl1" );
		if ( time < 1 )
			CVAR_SET_STRING( "mp_chattime_hl1", "1" );
		else if ( time > MAX_INTERMISSION_TIME )
			CVAR_SET_STRING( "mp_chattime_hl1", UTIL_dtos1( MAX_INTERMISSION_TIME ) );

		m_flIntermissionEndTime = g_flIntermissionStartTime + mp_chattime_hl1.value;

		// check to see if we should change levels now
		if ( m_flIntermissionEndTime < gpGlobals->time )
		{
			if ( m_iEndIntermissionButtonHit  // check that someone has pressed a key, or the max intermission time is over
				|| ( ( g_flIntermissionStartTime + MAX_INTERMISSION_TIME ) < gpGlobals->time) ) 
				ChangeLevel(); // intermission is over
		}

		return;
	}

	float flTimeLimit = timelimit.value * 60;
	float flFragLimit = fraglimit_hl1.value;

	time_remaining = (int)(flTimeLimit ? ( flTimeLimit - gpGlobalsHL1->time ) : 0);
	
	if ( flTimeLimit != 0 && gpGlobalsHL1->time >= flTimeLimit )
	{
		GoToIntermission();
		return;
	}

	if ( flFragLimit )
	{
		int bestfrags = 9999;
		int remain;

		// check if any player is over the frag limit
		for ( int i = 1; i <= gpGlobalsHL1->maxClients; i++ )
		{
			CBaseEntityHL1 *pPlayer = UTIL_PlayerByIndexHL1( i );

			if ( pPlayer && pPlayer->pev->frags >= flFragLimit )
			{
				GoToIntermission();
				return;
			}


			if ( pPlayer )
			{
				remain = flFragLimit - pPlayer->pev->frags;
				if ( remain < bestfrags )
				{
					bestfrags = remain;
				}
			}

		}
		frags_remaining = bestfrags;
	}

	// Updates when frags change
	if ( frags_remaining != last_frags )
	{
		g_engfuncsHL1.pfnCvar_DirectSet( &fragsleft, UTIL_VarArgs( "%i", frags_remaining ) );
	}

	// Updates once per second
	if ( timeleft.value != last_time )
	{
		g_engfuncsHL1.pfnCvar_DirectSet( &timeleft, UTIL_VarArgs( "%i", time_remaining ) );
	}

	last_frags = frags_remaining;
	last_time  = time_remaining;
}


//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsMultiplayer( void )
{
	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsDeathmatch( void )
{
	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsCoOp( void )
{
	return gpGlobalsHL1->coop;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::FShouldSwitchWeapon( CBasePlayerHL1 *pPlayer, CBasePlayerItemHL1 *pWeapon )
{
	if ( !pWeapon->CanDeploy() )
	{
		// that weapon can't deploy anyway.
		return FALSE;
	}

	if ( !pPlayer->m_pActiveItem )
	{
		// player doesn't have an active item!
		return TRUE;
	}

	if ( !pPlayer->m_pActiveItem->CanHolster() )
	{
		// can't put away the active item.
		return FALSE;
	}

	if ( pWeapon->iWeight() > pPlayer->m_pActiveItem->iWeight() )
	{
		return TRUE;
	}

	return FALSE;
}

BOOL CHalfLifeMultiplay :: GetNextBestWeapon( CBasePlayerHL1 *pPlayer, CBasePlayerItemHL1 *pCurrentWeapon )
{

	CBasePlayerItemHL1 *pCheck;
	CBasePlayerItemHL1 *pBest;// this will be used in the event that we don't find a weapon in the same category.
	int iBestWeight;
	int i;

	iBestWeight = -1;// no weapon lower than -1 can be autoswitched to
	pBest = NULL;

	if ( !pCurrentWeapon->CanHolster() )
	{
		// can't put this gun away right now, so can't switch.
		return FALSE;
	}

	for ( i = 0 ; i < MAX_ITEM_TYPES ; i++ )
	{
		pCheck = pPlayer->m_rgpPlayerItems[ i ];

		while ( pCheck )
		{
			if ( pCheck->iWeight() > -1 && pCheck->iWeight() == pCurrentWeapon->iWeight() && pCheck != pCurrentWeapon )
			{
				// this weapon is from the same category. 
				if ( pCheck->CanDeploy() )
				{
					if ( pPlayer->SwitchWeapon( pCheck ) )
					{
						return TRUE;
					}
				}
			}
			else if ( pCheck->iWeight() > iBestWeight && pCheck != pCurrentWeapon )// don't reselect the weapon we're trying to get rid of
			{
				//ALERT ( at_console, "Considering %s\n", STRING( pCheck->pev->classname ) );
				// we keep updating the 'best' weapon just in case we can't find a weapon of the same weight
				// that the player was using. This will end up leaving the player with his heaviest-weighted 
				// weapon. 
				if ( pCheck->CanDeploy() )
				{
					// if this weapon is useable, flag it as the best
					iBestWeight = pCheck->iWeight();
					pBest = pCheck;
				}
			}

			pCheck = pCheck->m_pNext;
		}
	}

	// if we make it here, we've checked all the weapons and found no useable 
	// weapon in the same catagory as the current weapon. 
	
	// if pBest is null, we didn't find ANYTHING. Shouldn't be possible- should always 
	// at least get the crowbar, but ya never know.
	if ( !pBest )
	{
		return FALSE;
	}

	pPlayer->SwitchWeapon( pBest );

	return TRUE;
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay :: ClientConnected( edict_t_hl1 *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[ 128 ] )
{
	g_VoiceGameMgr.ClientConnected(pEntity);
	return TRUE;
}

extern int gmsgSayText;
extern int gmsgGameMode;

void CHalfLifeMultiplay :: UpdateGameMode( CBasePlayerHL1 *pPlayer )
{
	MESSAGE_BEGIN( MSG_ONE, gmsgGameMode, NULL, pPlayer->edict() );
		WRITE_BYTE( 0 );  // game mode none
	MESSAGE_END();
}

void CHalfLifeMultiplay :: InitHUD( CBasePlayerHL1 *pl )
{
	// notify other clients of player joining the game
	UTIL_ClientPrintAll( HUD_PRINTNOTIFY, UTIL_VarArgs( "%s has joined the game\n", 
		( pl->pev->netname && STRING_HL1(pl->pev->netname)[0] != 0 ) ? STRING_HL1(pl->pev->netname) : "unconnected" ) );

	// team match?
	if ( g_teamplay )
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%s>\" entered the game\n",  
			STRING_HL1( pl->pev->netname ), 
			GETPLAYERUSERID( pl->edict() ),
			GETPLAYERAUTHID( pl->edict() ),
			g_engfuncsHL1.pfnInfoKeyValue( g_engfuncsHL1.pfnGetInfoKeyBuffer( pl->edict() ), "model" ) );
	}
	else
	{
		UTIL_LogPrintf( "\"%s<%i><%s><%i>\" entered the game\n",  
			STRING_HL1( pl->pev->netname ), 
			GETPLAYERUSERID( pl->edict() ),
			GETPLAYERAUTHID( pl->edict() ),
			GETPLAYERUSERID( pl->edict() ) );
	}

	UpdateGameMode( pl );

	// sending just one score makes the hud scoreboard active;  otherwise
	// it is just disabled for single play
	MESSAGE_BEGIN( MSG_ONE, gmsgScoreInfo, NULL, pl->edict() );
		WRITE_BYTE( ENTINDEX(pl->edict()) );
		WRITE_SHORT( 0 );
		WRITE_SHORT( 0 );
		WRITE_SHORT( 0 );
		WRITE_SHORT( 0 );
	MESSAGE_END();

	SendMOTDToClient( pl->edict() );

	// loop through all active players and send their score info to the new client
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		// FIXME:  Probably don't need to cast this just to read m_iDeaths
		CBasePlayerHL1 *plr = (CBasePlayerHL1 *)UTIL_PlayerByIndex( i );

		if ( plr )
		{
			MESSAGE_BEGIN( MSG_ONE, gmsgScoreInfo, NULL, pl->edict() );
				WRITE_BYTE( i );	// client number
				WRITE_SHORT( plr->pev->frags );
				WRITE_SHORT( plr->m_iDeaths );
				WRITE_SHORT( 0 );
				WRITE_SHORT( GetTeamIndex( plr->m_szTeamName ) + 1 );
			MESSAGE_END();
		}
	}

	if ( g_fGameOverHL1 )
	{
		MESSAGE_BEGIN( MSG_ONE, SVC_INTERMISSION, NULL, pl->edict() );
		MESSAGE_END();
	}
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay :: ClientDisconnected( edict_t_hl1 *pClient )
{
	if ( pClient )
	{
		CBasePlayerHL1 *pPlayer = (CBasePlayerHL1 *)CBaseEntityHL1::Instance( pClient );

		if ( pPlayer )
		{
			FireTargets( "game_playerleave", pPlayer, pPlayer, USE_TOGGLE, 0 );

			// team match?
			if ( g_teamplay )
			{
				UTIL_LogPrintf( "\"%s<%i><%s><%s>\" disconnected\n",  
					STRING_HL1( pPlayer->pev->netname ), 
					GETPLAYERUSERID( pPlayer->edict() ),
					GETPLAYERAUTHID( pPlayer->edict() ),
					g_engfuncsHL1.pfnInfoKeyValue( g_engfuncsHL1.pfnGetInfoKeyBuffer( pPlayer->edict() ), "model" ) );
			}
			else
			{
				UTIL_LogPrintf( "\"%s<%i><%s><%i>\" disconnected\n",  
					STRING_HL1( pPlayer->pev->netname ), 
					GETPLAYERUSERID( pPlayer->edict() ),
					GETPLAYERAUTHID( pPlayer->edict() ),
					GETPLAYERUSERID( pPlayer->edict() ) );
			}

			pPlayer->RemoveAllItems( TRUE );// destroy all of the players weapons and items
		}
	}
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay :: FlPlayerFallDamage( CBasePlayerHL1 *pPlayer )
{
	int iFallDamage = (int)falldamage_hl1.value;

	switch ( iFallDamage )
	{
	case 1://progressive
		pPlayer->m_flFallVelocity -= PLAYER_MAX_SAFE_FALL_SPEED;
		return pPlayer->m_flFallVelocity * DAMAGE_FOR_FALL_SPEED;
		break;
	default:
	case 0:// fixed
		return 10;
		break;
	}
} 

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::FPlayerCanTakeDamage( CBasePlayerHL1 *pPlayer, CBaseEntityHL1 *pAttacker )
{
	return TRUE;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay :: PlayerThink( CBasePlayerHL1 *pPlayer )
{
	if ( g_fGameOverHL1 )
	{
		// check for button presses
		if ( pPlayer->m_afButtonPressed & ( IN_DUCK | IN_ATTACK | IN_ATTACK2 | IN_USE | IN_JUMP ) )
			m_iEndIntermissionButtonHit = TRUE;

		// clear attack/use commands from player
		pPlayer->m_afButtonPressed = 0;
		pPlayer->pev->button = 0;
		pPlayer->m_afButtonReleased = 0;
	}
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay :: PlayerSpawn( CBasePlayerHL1 *pPlayer )
{
	BOOL		addDefault;
	CBaseEntityHL1	*pWeaponEntity = NULL;

	pPlayer->pev->weapons |= (1<<WEAPON_SUIT);
	
	addDefault = TRUE;

	while ( pWeaponEntity = UTIL_FindEntityByClassname( pWeaponEntity, "game_player_equip" ))
	{
		pWeaponEntity->Touch( pPlayer );
		addDefault = FALSE;
	}

	if ( addDefault )
	{
		pPlayer->GiveNamedItem( "weapon_crowbar" );
		pPlayer->GiveNamedItem( "weapon_9mmhandgun" );
		pPlayer->GiveAmmo( 68, "9mm", _9MM_MAX_CARRY );// 4 full reloads
	}
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay :: FPlayerCanRespawn( CBasePlayerHL1 *pPlayer )
{
	return TRUE;
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay :: FlPlayerSpawnTime( CBasePlayerHL1 *pPlayer )
{
	return gpGlobalsHL1->time;//now!
}

BOOL CHalfLifeMultiplay :: AllowAutoTargetCrosshair( void )
{
	return ( aimcrosshair_hl1.value != 0 );
}

//=========================================================
// IPointsForKill - how many points awarded to anyone
// that kills this player?
//=========================================================
int CHalfLifeMultiplay :: IPointsForKill( CBasePlayerHL1 *pAttacker, CBasePlayerHL1 *pKilled )
{
	return 1;
}


//=========================================================
// PlayerKilled - someone/something killed this player
//=========================================================
void CHalfLifeMultiplay :: PlayerKilled( CBasePlayerHL1 *pVictim, entvars_t *pKiller, entvars_t *pInflictor )
{
	DeathNotice( pVictim, pKiller, pInflictor );

	pVictim->m_iDeaths += 1;


	FireTargets( "game_playerdie", pVictim, pVictim, USE_TOGGLE, 0 );
	CBasePlayerHL1 *peKiller = NULL;
	CBaseEntityHL1 *ktmp = CBaseEntityHL1::Instance( pKiller );
	if ( ktmp && (ktmp->Classify() == CLASS_PLAYER) )
		peKiller = (CBasePlayerHL1*)ktmp;

	if ( pVictim->pev == pKiller )  
	{  // killed self
		pKiller->frags -= 1;
	}
	else if ( ktmp && ktmp->IsPlayer() )
	{
		// if a player dies in a deathmatch game and the killer is a client, award the killer some points
		pKiller->frags += IPointsForKill( peKiller, pVictim );
		
		FireTargets( "game_playerkill", ktmp, ktmp, USE_TOGGLE, 0 );
	}
	else
	{  // killed by the world
		pKiller->frags -= 1;
	}

	// update the scores
	// killed scores
	MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
		WRITE_BYTE( ENTINDEX(pVictim->edict()) );
		WRITE_SHORT( pVictim->pev->frags );
		WRITE_SHORT( pVictim->m_iDeaths );
		WRITE_SHORT( 0 );
		WRITE_SHORT( GetTeamIndex( pVictim->m_szTeamName ) + 1 );
	MESSAGE_END();

	// killers score, if it's a player
	CBaseEntityHL1 *ep = CBaseEntityHL1::Instance( pKiller );
	if ( ep && ep->Classify() == CLASS_PLAYER )
	{
		CBasePlayerHL1 *PK = (CBasePlayerHL1*)ep;

		MESSAGE_BEGIN( MSG_ALL, gmsgScoreInfo );
			WRITE_BYTE( ENTINDEX(PK->edict()) );
			WRITE_SHORT( PK->pev->frags );
			WRITE_SHORT( PK->m_iDeaths );
			WRITE_SHORT( 0 );
			WRITE_SHORT( GetTeamIndex( PK->m_szTeamName) + 1 );
		MESSAGE_END();

		// let the killer paint another decal as soon as he'd like.
		PK->m_flNextDecalTime = gpGlobals->time;
	}
#ifndef HLDEMO_BUILD
	if ( pVictim->HasNamedPlayerItem("weapon_satchel") )
	{
		DeactivateSatchels( pVictim );
	}
#endif
}

//=========================================================
// Deathnotice. 
//=========================================================
void CHalfLifeMultiplay::DeathNotice( CBasePlayerHL1 *pVictim, entvars_t *pKiller, entvars_t *pevInflictor )
{
	// Work out what killed the player, and send a message to all clients about it
	CBaseEntityHL1 *Killer = CBaseEntityHL1::Instance( pKiller );

	const char *killer_weapon_name = "world";		// by default, the player is killed by the world
	int killer_index = 0;
	
	// Hack to fix name change
	char *tau = "tau_cannon";
	char *gluon = "gluon gun";

	if ( pKiller->flags & FL_CLIENT )
	{
		killer_index = ENTINDEX(ENT(pKiller));
		
		if ( pevInflictor )
		{
			if ( pevInflictor == pKiller )
			{
				// If the inflictor is the killer,  then it must be their current weapon doing the damage
				CBasePlayerHL1 *pPlayer = (CBasePlayerHL1*)CBaseEntityHL1::Instance( pKiller );
				
				if ( pPlayer->m_pActiveItem )
				{
					killer_weapon_name = pPlayer->m_pActiveItem->pszName();
				}
			}
			else
			{
				killer_weapon_name = STRING_HL1( pevInflictor->classname );  // it's just that easy
			}
		}
	}
	else
	{
		killer_weapon_name = STRING_HL1( pevInflictor->classname );
	}

	// strip the monster_* or weapon_* from the inflictor's classname
	if ( strncmp( killer_weapon_name, "weapon_", 7 ) == 0 )
		killer_weapon_name += 7;
	else if ( strncmp( killer_weapon_name, "monster_", 8 ) == 0 )
		killer_weapon_name += 8;
	else if ( strncmp( killer_weapon_name, "func_", 5 ) == 0 )
		killer_weapon_name += 5;

	MESSAGE_BEGIN( MSG_ALL, gmsgDeathMsg );
		WRITE_BYTE( killer_index );						// the killer
		WRITE_BYTE( ENTINDEX(pVictim->edict()) );		// the victim
		WRITE_STRING( killer_weapon_name );		// what they were killed by (should this be a string?)
	MESSAGE_END();

	// replace the code names with the 'real' names
	if ( !strcmp( killer_weapon_name, "egon" ) )
		killer_weapon_name = gluon;
	else if ( !strcmp( killer_weapon_name, "gauss" ) )
		killer_weapon_name = tau;

	if ( pVictim->pev == pKiller )  
	{
		// killed self

		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" committed suicide with \"%s\"\n",  
				STRING_HL1( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				g_engfuncsHL1.pfnInfoKeyValue( g_engfuncsHL1.pfnGetInfoKeyBuffer( pVictim->edict() ), "model" ),
				killer_weapon_name );		
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" committed suicide with \"%s\"\n",  
				STRING_HL1( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				GETPLAYERUSERID( pVictim->edict() ),
				killer_weapon_name );		
		}
	}
	else if ( pKiller->flags & FL_CLIENT )
	{
		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" killed \"%s<%i><%s><%s>\" with \"%s\"\n",  
				STRING_HL1( pKiller->netname ),
				GETPLAYERUSERID( ENT(pKiller) ),
				GETPLAYERAUTHID( ENT(pKiller) ),
				g_engfuncsHL1.pfnInfoKeyValue( g_engfuncsHL1.pfnGetInfoKeyBuffer( ENT(pKiller) ), "model" ),
				STRING_HL1( pVictim->pev->netname ),
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				g_engfuncsHL1.pfnInfoKeyValue( g_engfuncsHL1.pfnGetInfoKeyBuffer( pVictim->edict() ), "model" ),
				killer_weapon_name );
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" killed \"%s<%i><%s><%i>\" with \"%s\"\n",  
				STRING_HL1( pKiller->netname ),
				GETPLAYERUSERID( ENT(pKiller) ),
				GETPLAYERAUTHID( ENT(pKiller) ),
				GETPLAYERUSERID( ENT(pKiller) ),
				STRING_HL1( pVictim->pev->netname ),
				GETPLAYERUSERID( pVictim->edict() ),
				GETPLAYERAUTHID( pVictim->edict() ),
				GETPLAYERUSERID( pVictim->edict() ),
				killer_weapon_name );
		}
	}
	else
	{ 
		// killed by the world

		// team match?
		if ( g_teamplay )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" committed suicide with \"%s\" (world)\n",
				STRING_HL1( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ), 
				GETPLAYERAUTHID( pVictim->edict() ),
				g_engfuncsHL1.pfnInfoKeyValue( g_engfuncsHL1.pfnGetInfoKeyBuffer( pVictim->edict() ), "model" ),
				killer_weapon_name );		
		}
		else
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%i>\" committed suicide with \"%s\" (world)\n",
				STRING_HL1( pVictim->pev->netname ), 
				GETPLAYERUSERID( pVictim->edict() ), 
				GETPLAYERAUTHID( pVictim->edict() ),
				GETPLAYERUSERID( pVictim->edict() ),
				killer_weapon_name );		
		}
	}

	MESSAGE_BEGIN( MSG_SPEC, SVC_DIRECTOR );
		WRITE_BYTE ( 9 );	// command length in bytes
		WRITE_BYTE ( DRC_CMD_EVENT );	// player killed
		WRITE_SHORT( ENTINDEX(pVictim->edict()) );	// index number of primary entity
		if (pevInflictor)
			WRITE_SHORT( ENTINDEX(ENT(pevInflictor)) );	// index number of secondary entity
		else
			WRITE_SHORT( ENTINDEX(ENT(pKiller)) );	// index number of secondary entity
		WRITE_LONG( 7 | DRC_FLAG_DRAMATIC);   // eventflags (priority and flags)
	MESSAGE_END();

//  Print a standard message
	// TODO: make this go direct to console
	return; // just remove for now
/*
	char	szText[ 128 ];

	if ( pKiller->flags & FL_MONSTER )
	{
		// killed by a monster
		strcpy ( szText, STRING_HL1( pVictim->pev->netname ) );
		strcat ( szText, " was killed by a monster.\n" );
		return;
	}

	if ( pKiller == pVictim->pev )
	{
		strcpy ( szText, STRING_HL1( pVictim->pev->netname ) );
		strcat ( szText, " commited suicide.\n" );
	}
	else if ( pKiller->flags & FL_CLIENT )
	{
		strcpy ( szText, STRING_HL1( pKiller->netname ) );

		strcat( szText, " : " );
		strcat( szText, killer_weapon_name );
		strcat( szText, " : " );

		strcat ( szText, STRING_HL1( pVictim->pev->netname ) );
		strcat ( szText, "\n" );
	}
	else if ( FClassnameIs ( pKiller, "worldspawn" ) )
	{
		strcpy ( szText, STRING_HL1( pVictim->pev->netname ) );
		strcat ( szText, " fell or drowned or something.\n" );
	}
	else if ( pKiller->solid == SOLID_BSP )
	{
		strcpy ( szText, STRING_HL1( pVictim->pev->netname ) );
		strcat ( szText, " was mooshed.\n" );
	}
	else
	{
		strcpy ( szText, STRING_HL1( pVictim->pev->netname ) );
		strcat ( szText, " died mysteriously.\n" );
	}

	UTIL_ClientPrintAll( szText );
*/
}

//=========================================================
// PlayerGotWeapon - player has grabbed a weapon that was
// sitting in the world
//=========================================================
void CHalfLifeMultiplay :: PlayerGotWeapon( CBasePlayerHL1 *pPlayer, CBasePlayerItemHL1 *pWeapon )
{
}

//=========================================================
// FlWeaponRespawnTime - what is the time in the future
// at which this weapon may spawn?
//=========================================================
float CHalfLifeMultiplay :: FlWeaponRespawnTime( CBasePlayerItemHL1 *pWeapon )
{
	if ( weaponstay_hl1.value > 0 )
	{
		// make sure it's only certain weapons
		if ( !(pWeapon->iFlags() & ITEM_FLAG_LIMITINWORLD) )
		{
			return gpGlobalsHL1->time + 0;		// weapon respawns almost instantly
		}
	}

	return gpGlobalsHL1->time + WEAPON_RESPAWN_TIME;
}

// when we are within this close to running out of entities,  items 
// marked with the ITEM_FLAG_LIMITINWORLD will delay their respawn
#define ENTITY_INTOLERANCE	100

//=========================================================
// FlWeaponRespawnTime - Returns 0 if the weapon can respawn 
// now,  otherwise it returns the time at which it can try
// to spawn again.
//=========================================================
float CHalfLifeMultiplay :: FlWeaponTryRespawn( CBasePlayerItemHL1 *pWeapon )
{
	if ( pWeapon && pWeapon->m_iId && (pWeapon->iFlags() & ITEM_FLAG_LIMITINWORLD) )
	{
		if ( NUMBER_OF_ENTITIES() < (gpGlobalsHL1->maxEntities - ENTITY_INTOLERANCE) )
			return 0;

		// we're past the entity tolerance level,  so delay the respawn
		return FlWeaponRespawnTime( pWeapon );
	}

	return 0;
}

//=========================================================
// VecWeaponRespawnSpot - where should this weapon spawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
VectorHL1 CHalfLifeMultiplay :: VecWeaponRespawnSpotHL1( CBasePlayerItemHL1 *pWeapon )
{
	return pWeapon->pev->origin;
}

//=========================================================
// WeaponShouldRespawn - any conditions inhibiting the
// respawning of this weapon?
//=========================================================
int CHalfLifeMultiplay :: WeaponShouldRespawn( CBasePlayerItemHL1 *pWeapon )
{
	if ( pWeapon->pev->spawnflags & SF_NORESPAWN )
	{
		return GR_WEAPON_RESPAWN_NO;
	}

	return GR_WEAPON_RESPAWN_YES;
}

//=========================================================
// CanHaveWeapon - returns FALSE if the player is not allowed
// to pick up this weapon
//=========================================================
BOOL CHalfLifeMultiplay::CanHavePlayerItem( CBasePlayerHL1 *pPlayer, CBasePlayerItemHL1 *pItem )
{
	if ( weaponstay_hl1.value > 0 )
	{
		if ( pItem->iFlags() & ITEM_FLAG_LIMITINWORLD )
			return CGameRulesHL1::CanHavePlayerItem( pPlayer, pItem );

		// check if the player already has this weapon
		for ( int i = 0 ; i < MAX_ITEM_TYPES ; i++ )
		{
			CBasePlayerItemHL1 *it = pPlayer->m_rgpPlayerItems[i];

			while ( it != NULL )
			{
				if ( it->m_iId == pItem->m_iId )
				{
					return FALSE;
				}

				it = it->m_pNext;
			}
		}
	}

	return CGameRulesHL1::CanHavePlayerItem( pPlayer, pItem );
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::CanHaveItem( CBasePlayerHL1 *pPlayer, CItemHL1 *pItem )
{
	return TRUE;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::PlayerGotItem( CBasePlayerHL1 *pPlayer, CItemHL1 *pItem )
{
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::ItemShouldRespawn( CItemHL1 *pItem )
{
	if ( pItem->pev->spawnflags & SF_NORESPAWN )
	{
		return GR_ITEM_RESPAWN_NO;
	}

	return GR_ITEM_RESPAWN_YES;
}


//=========================================================
// At what time in the future may this Item respawn?
//=========================================================
float CHalfLifeMultiplay::FlItemRespawnTime( CItemHL1 *pItem )
{
	return gpGlobalsHL1->time + ITEM_RESPAWN_TIME;
}

//=========================================================
// Where should this item respawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CHalfLifeMultiplay::VecItemRespawnSpot( CItemHL1 *pItem )
{
	return pItem->pev->origin;
}

//=========================================================
//=========================================================
void CHalfLifeMultiplay::PlayerGotAmmo( CBasePlayerHL1 *pPlayer, char *szName, int iCount )
{
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay::IsAllowedToSpawn( CBaseEntityHL1 *pEntity )
{
//	if ( pEntity->pev->flags & FL_MONSTER )
//		return FALSE;

	return TRUE;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::AmmoShouldRespawn( CBasePlayerAmmoHL1 *pAmmo )
{
	if ( pAmmo->pev->spawnflags & SF_NORESPAWN )
	{
		return GR_AMMO_RESPAWN_NO;
	}

	return GR_AMMO_RESPAWN_YES;
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay::FlAmmoRespawnTime( CBasePlayerAmmoHL1 *pAmmo )
{
	return gpGlobalsHL1->time + AMMO_RESPAWN_TIME;
}

//=========================================================
//=========================================================
Vector CHalfLifeMultiplay::VecAmmoRespawnSpot( CBasePlayerAmmoHL1 *pAmmo )
{
	return pAmmo->pev->origin;
}

//=========================================================
//=========================================================
float CHalfLifeMultiplay::FlHealthChargerRechargeTime( void )
{
	return 60;
}


float CHalfLifeMultiplay::FlHEVChargerRechargeTime( void )
{
	return 30;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::DeadPlayerWeapons( CBasePlayerHL1 *pPlayer )
{
	return GR_PLR_DROP_GUN_ACTIVE;
}

//=========================================================
//=========================================================
int CHalfLifeMultiplay::DeadPlayerAmmo( CBasePlayerHL1 *pPlayer )
{
	return GR_PLR_DROP_AMMO_ACTIVE;
}

edict_t *CHalfLifeMultiplay::GetPlayerSpawnSpot( CBasePlayerHL1 *pPlayer )
{
	edict_t *pentSpawnSpot = CGameRulesHL1::GetPlayerSpawnSpot( pPlayer );	
	if ( IsMultiplayer() && pentSpawnSpot->v.target )
	{
		FireTargets( STRING_HL1(pentSpawnSpot->v.target), pPlayer, pPlayer, USE_TOGGLE, 0 );
	}

	return pentSpawnSpot;
}


//=========================================================
//=========================================================
int CHalfLifeMultiplay::PlayerRelationship( CBaseEntityHL1 *pPlayer, CBaseEntityHL1 *pTarget )
{
	// half life deathmatch has only enemies
	return GR_NOTTEAMMATE;
}

BOOL CHalfLifeMultiplay :: PlayFootstepSounds( CBasePlayerHL1 *pl, float fvol )
{
	if ( g_footsteps && g_footsteps->value == 0 )
		return FALSE;

	if ( pl->IsOnLadder() || pl->pev->velocity.Length2D() > 220 )
		return TRUE;  // only make step sounds in multiplayer if the player is moving fast enough

	return FALSE;
}

BOOL CHalfLifeMultiplay :: FAllowFlashlight( void ) 
{ 
	return flashlight_hl1.value != 0; 
}

//=========================================================
//=========================================================
BOOL CHalfLifeMultiplay :: FAllowMonsters( void )
{
	return ( allowmonsters.value != 0 );
}

//=========================================================
//======== CHalfLifeMultiplay private functions ===========
#define INTERMISSION_TIME		6

void CHalfLifeMultiplay :: GoToIntermission( void )
{
	if ( g_fGameOverHL1 )
		return;  // intermission has already been triggered, so ignore.

	MESSAGE_BEGIN(MSG_ALL, SVC_INTERMISSION);
	MESSAGE_END();

	// bounds check
	int time = (int)CVAR_GET_FLOAT( "mp_chattime" );
	if ( time < 1 )
		CVAR_SET_STRING( "mp_chattime_hl1", "1" );
	else if ( time > MAX_INTERMISSION_TIME )
		CVAR_SET_STRING( "mp_chattime_hl1", UTIL_dtos1( MAX_INTERMISSION_TIME ) );

	m_flIntermissionEndTime = gpGlobalsHL1->time + ( (int)mp_chattime_hl1.value );
	g_flIntermissionStartTime = gpGlobalsHL1->time;

	g_fGameOverHL1 = TRUE;
	m_iEndIntermissionButtonHit = FALSE;
}

#define MAX_RULE_BUFFER 1024

typedef struct mapcycle_item_s
{
	struct mapcycle_item_s *next;

	char mapname[ 32 ];
	int  minplayers, maxplayers;
	char rulebuffer[ MAX_RULE_BUFFER ];
} mapcycle_item_t;

typedef struct mapcycle_s
{
	struct mapcycle_item_s *items;
	struct mapcycle_item_s *next_item;
} mapcycle_t;

/*
==============
DestroyMapCycle

Clean up memory used by mapcycle when switching it
==============
*/
void DestroyMapCycle( mapcycle_t *cycle )
{
	mapcycle_item_t *p, *n, *start;
	p = cycle->items;
	if ( p )
	{
		start = p;
		p = p->next;
		while ( p != start )
		{
			n = p->next;
			delete p;
			p = n;
		}
		
		delete cycle->items;
	}
	cycle->items = NULL;
	cycle->next_item = NULL;
}

static char com_token[ 1500 ];

/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char *COM_Parse (char *data)
{
	int             c;
	int             len;
	
	len = 0;
	com_token[0] = 0;
	
	if (!data)
		return NULL;
		
// skip whitespace
skipwhite:
	while ( (c = *data) <= ' ')
	{
		if (c == 0)
			return NULL;                    // end of file;
		data++;
	}
	
// skip // comments
	if (c=='/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}
	

// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		}
	}

// parse single characters
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c == ',' )
	{
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data+1;
	}

// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
	if (c=='{' || c=='}'|| c==')'|| c=='(' || c=='\'' || c == ',' )
			break;
	} while (c>32);
	
	com_token[len] = 0;
	return data;
}

/*
==============
COM_TokenWaiting

Returns 1 if additional data is waiting to be processed on this line
==============
*/
int COM_TokenWaiting( char *buffer )
{
	char *p;

	p = buffer;
	while ( *p && *p!='\n')
	{
		if ( !isspace( *p ) || isalnum( *p ) )
			return 1;

		p++;
	}

	return 0;
}



/*
==============
ReloadMapCycleFile


Parses mapcycle.txt file into mapcycle_t structure
==============
*/
int ReloadMapCycleFile( char *filename, mapcycle_t *cycle )
{
	char szBuffer[ MAX_RULE_BUFFER ];
	char szMap[ 32 ];
	int length;
	char *pFileList;
	char *aFileList = pFileList = (char*)LOAD_FILE_FOR_ME( filename, &length );
	int hasbuffer;
	mapcycle_item_s *item, *newlist = NULL, *next;

	if ( pFileList && length )
	{
		// the first map name in the file becomes the default
		while ( 1 )
		{
			hasbuffer = 0;
			memset( szBuffer, 0, MAX_RULE_BUFFER );

			pFileList = COM_Parse( pFileList );
			if ( strlen( com_token ) <= 0 )
				break;

			strcpy( szMap, com_token );

			// Any more tokens on this line?
			if ( COM_TokenWaiting( pFileList ) )
			{
				pFileList = COM_Parse( pFileList );
				if ( strlen( com_token ) > 0 )
				{
					hasbuffer = 1;
					strcpy( szBuffer, com_token );
				}
			}

			// Check map
			if ( IS_MAP_VALID( szMap ) )
			{
				// Create entry
				char *s;

				item = new mapcycle_item_s;

				strcpy( item->mapname, szMap );

				item->minplayers = 0;
				item->maxplayers = 0;

				memset( item->rulebuffer, 0, MAX_RULE_BUFFER );

				if ( hasbuffer )
				{
					s = g_engfuncsHL1.pfnInfoKeyValue( szBuffer, "minplayers" );
					if ( s && s[0] )
					{
						item->minplayers = atoi( s );
						item->minplayers = max( item->minplayers, 0 );
						item->minplayers = min( item->minplayers, gpGlobals->maxClients );
					}
					s = g_engfuncsHL1.pfnInfoKeyValue( szBuffer, "maxplayers" );
					if ( s && s[0] )
					{
						item->maxplayers = atoi( s );
						item->maxplayers = max( item->maxplayers, 0 );
						item->maxplayers = min( item->maxplayers, gpGlobals->maxClients );
					}

					// Remove keys
					//
					g_engfuncsHL1.pfnInfo_RemoveKey( szBuffer, "minplayers" );
					g_engfuncsHL1.pfnInfo_RemoveKey( szBuffer, "maxplayers" );

					strcpy( item->rulebuffer, szBuffer );
				}

				item->next = cycle->items;
				cycle->items = item;
			}
			else
			{
				ALERT( at_console, "Skipping %s from mapcycle, not a valid map\n", szMap );
			}

		}

		FREE_FILE( aFileList );
	}

	// Fixup circular list pointer
	item = cycle->items;

	// Reverse it to get original order
	while ( item )
	{
		next = item->next;
		item->next = newlist;
		newlist = item;
		item = next;
	}
	cycle->items = newlist;
	item = cycle->items;

	// Didn't parse anything
	if ( !item )
	{
		return 0;
	}

	while ( item->next )
	{
		item = item->next;
	}
	item->next = cycle->items;
	
	cycle->next_item = item->next;

	return 1;
}

/*
==============
CountPlayers

Determine the current # of active players on the server for map cycling logic
==============
*/
int CountPlayers( void )
{
	int	num = 0;

	for ( int i = 1; i <= gpGlobalsHL1->maxClients; i++ )
	{
		CBaseEntity *pEnt = UTIL_PlayerByIndex( i );

		if ( pEnt )
		{
			num = num + 1;
		}
	}

	return num;
}

/*
==============
ExtractCommandString

Parse commands/key value pairs to issue right after map xxx command is issued on server
 level transition
==============
*/
void ExtractCommandString( char *s, char *szCommand )
{
	// Now make rules happen
	char	pkey[512];
	char	value[512];	// use two buffers so compares
								// work without stomping on each other
	char	*o;
	
	if ( *s == '\\' )
		s++;

	while (1)
	{
		o = pkey;
		while ( *s != '\\' )
		{
			if ( !*s )
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;

		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		strcat( szCommand, pkey );
		if ( strlen( value ) > 0 )
		{
			strcat( szCommand, " " );
			strcat( szCommand, value );
		}
		strcat( szCommand, "\n" );

		if (!*s)
			return;
		s++;
	}
}

/*
==============
ChangeLevel

Server is changing to a new level, check mapcycle.txt for map name and setup info
==============
*/
void CHalfLifeMultiplay :: ChangeLevel( void )
{
	static char szPreviousMapCycleFile[ 256 ];
	static mapcycle_t mapcycle;

	char szNextMap[32];
	char szFirstMapInList[32];
	char szCommands[ 1500 ];
	char szRules[ 1500 ];
	int minplayers = 0, maxplayers = 0;
	strcpy( szFirstMapInList, "hldm1" );  // the absolute default level is hldm1

	int	curplayers;
	BOOL do_cycle = TRUE;

	// find the map to change to
	char *mapcfile = (char*)CVAR_GET_STRING( "mapcyclefile" );
	ASSERT( mapcfile != NULL );

	szCommands[ 0 ] = '\0';
	szRules[ 0 ] = '\0';

	curplayers = CountPlayers();

	// Has the map cycle filename changed?
	if ( stricmp( mapcfile, szPreviousMapCycleFile ) )
	{
		strcpy( szPreviousMapCycleFile, mapcfile );

		DestroyMapCycle( &mapcycle );

		if ( !ReloadMapCycleFile( mapcfile, &mapcycle ) || ( !mapcycle.items ) )
		{
			ALERT( at_console, "Unable to load map cycle file %s\n", mapcfile );
			do_cycle = FALSE;
		}
	}

	if ( do_cycle && mapcycle.items )
	{
		BOOL keeplooking = FALSE;
		BOOL found = FALSE;
		mapcycle_item_s *item;

		// Assume current map
		strcpy( szNextMap, STRING(gpGlobalsHL1->mapname) );
		strcpy( szFirstMapInList, STRING(gpGlobalsHL1->mapname) );

		// Traverse list
		for ( item = mapcycle.next_item; item->next != mapcycle.next_item; item = item->next )
		{
			keeplooking = FALSE;

			ASSERT( item != NULL );

			if ( item->minplayers != 0 )
			{
				if ( curplayers >= item->minplayers )
				{
					found = TRUE;
					minplayers = item->minplayers;
				}
				else
				{
					keeplooking = TRUE;
				}
			}

			if ( item->maxplayers != 0 )
			{
				if ( curplayers <= item->maxplayers )
				{
					found = TRUE;
					maxplayers = item->maxplayers;
				}
				else
				{
					keeplooking = TRUE;
				}
			}

			if ( keeplooking )
				continue;

			found = TRUE;
			break;
		}

		if ( !found )
		{
			item = mapcycle.next_item;
		}			
		
		// Increment next item pointer
		mapcycle.next_item = item->next;

		// Perform logic on current item
		strcpy( szNextMap, item->mapname );

		ExtractCommandString( item->rulebuffer, szCommands );
		strcpy( szRules, item->rulebuffer );
	}

	if ( !IS_MAP_VALID(szNextMap) )
	{
		strcpy( szNextMap, szFirstMapInList );
	}

	g_fGameOverHL1 = TRUE;

	ALERT( at_console, "CHANGE LEVEL: %s\n", szNextMap );
	if ( minplayers || maxplayers )
	{
		ALERT( at_console, "PLAYER COUNT:  min %i max %i current %i\n", minplayers, maxplayers, curplayers );
	}
	if ( strlen( szRules ) > 0 )
	{
		ALERT( at_console, "RULES:  %s\n", szRules );
	}
	
	CHANGE_LEVEL( szNextMap, NULL );
	if ( strlen( szCommands ) > 0 )
	{
		SERVER_COMMAND( szCommands );
	}
}

#define MAX_MOTD_CHUNK	  60
#define MAX_MOTD_LENGTH   1536 // (MAX_MOTD_CHUNK * 4)

void CHalfLifeMultiplay :: SendMOTDToClient( edict_t *client )
{
	// read from the MOTD.txt file
	int length, char_count = 0;
	char *pFileList;
	char *aFileList = pFileList = (char*)LOAD_FILE_FOR_ME( (char *)CVAR_GET_STRING( "motdfile" ), &length );

	// send the server name
	MESSAGE_BEGIN( MSG_ONE, gmsgServerName, NULL, client );
		WRITE_STRING( CVAR_GET_STRING("hostname") );
	MESSAGE_END();

	// Send the message of the day
	// read it chunk-by-chunk,  and send it in parts

	while ( pFileList && *pFileList && char_count < MAX_MOTD_LENGTH )
	{
		char chunk[MAX_MOTD_CHUNK+1];
		
		if ( strlen( pFileList ) < MAX_MOTD_CHUNK )
		{
			strcpy( chunk, pFileList );
		}
		else
		{
			strncpy( chunk, pFileList, MAX_MOTD_CHUNK );
			chunk[MAX_MOTD_CHUNK] = 0;		// strncpy doesn't always append the null terminator
		}

		char_count += strlen( chunk );
		if ( char_count < MAX_MOTD_LENGTH )
			pFileList = aFileList + char_count; 
		else
			*pFileList = 0;

		MESSAGE_BEGIN( MSG_ONE, gmsgMOTD, NULL, client );
			WRITE_BYTE( *pFileList ? FALSE : TRUE );	// FALSE means there is still more message to come
			WRITE_STRING( chunk );
		MESSAGE_END();
	}

	FREE_FILE( aFileList );
}
