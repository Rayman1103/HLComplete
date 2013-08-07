//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef TEAMPLAY_GAMERULES_H
#define TEAMPLAY_GAMERULES_H
#pragma once

#include "gamerules.h"
#include "multiplay_gamerules.h"

#ifdef CLIENT_DLL

	#define CTeamplayRules C_TeamplayRules

#else

	#include "takedamageinfo.h"

#endif


//
// teamplay_gamerules.h
//


#define MAX_TEAMNAME_LENGTH	16
#define MAX_TEAMS			32

#define TEAMPLAY_TEAMLISTLENGTH		MAX_TEAMS*MAX_TEAMNAME_LENGTH


class CTeamplayRules : public CMultiplayRules
{
public:
	DECLARE_CLASS( CTeamplayRules, CMultiplayRules );

	// Return the value of this player towards capturing a point
	virtual int	 GetCaptureValueForPlayer( CBasePlayer *pPlayer ) { return 1; }
	virtual bool TeamMayCapturePoint( int iTeam, int iPointIndex ) { return true; }
	virtual bool PlayerMayCapturePoint( CBasePlayer *pPlayer, int iPointIndex, char *pszReason = NULL, int iMaxReasonLength = 0 ) { return true; }
	virtual bool PlayerMayBlockPoint( CBasePlayer *pPlayer, int iPointIndex, char *pszReason = NULL, int iMaxReasonLength = 0 ) { return false; }

	// Return false if players aren't allowed to cap points at this time (i.e. in WaitingForPlayers)
	virtual bool PointsMayBeCaptured( void ) { return true; }
	virtual void SetLastCapPointChanged( int iIndex ) { return; }

#ifdef CLIENT_DLL

#else

	CTeamplayRules();
	virtual ~CTeamplayRules() {};

	virtual void Precache( void );

	virtual bool ClientCommand( CBaseEntity *pEdict, const CCommand &args );
	virtual void ClientSettingsChanged( CBasePlayer *pPlayer );
	virtual bool IsTeamplay( void );
	virtual bool FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker );
	virtual int PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget );
	virtual bool PlayerCanHearChat( CBasePlayer *pListener, CBasePlayer *pSpeaker );
	virtual const char *GetTeamID( CBaseEntity *pEntity );
	virtual bool ShouldAutoAim( CBasePlayer *pPlayer, edict_t *target );
	virtual int IPointsForKill( CBasePlayer *pAttacker, CBasePlayer *pKilled );
	virtual void InitHUD( CBasePlayer *pl );
	virtual void DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info );
	virtual const char *GetGameDescription( void ) { return "Teamplay"; }  // this is the game name that gets seen in the server browser
	virtual void PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info );
	virtual void Think ( void );
	virtual int GetTeamIndex( const char *pTeamName );
	virtual const char *GetIndexedTeamName( int teamIndex );
	virtual bool IsValidTeam( const char *pTeamName );
	virtual const char *SetDefaultPlayerTeam( CBasePlayer *pPlayer );
	virtual void ChangePlayerTeam( CBasePlayer *pPlayer, const char *pTeamName, bool bKill, bool bGib );
	virtual void ClientDisconnected( edict_t *pClient );
	virtual bool TimerMayExpire( void ) { return true; }

	// A game has been won by the specified team
	virtual void SetWinningTeam( int team, int iWinReason, bool bForceMapReset = true, bool bSwitchTeams = false, bool bDontAddScore = false ) { return; }
	virtual void SetStalemate( int iReason, bool bForceMapReset = true, bool bSwitchTeams = false ) { return; }

	// Used to determine if all players should switch teams
	virtual void SetSwitchTeams( bool bSwitch ){ m_bSwitchTeams = bSwitch; }
	virtual bool ShouldSwitchTeams( void ){ return m_bSwitchTeams; }
	virtual void HandleSwitchTeams( void ){ return; }

	// Used to determine if we should scramble the teams
	virtual void SetScrambleTeams( bool bScramble ){ m_bScrambleTeams = bScramble; }
	virtual bool ShouldScrambleTeams( void ){ return m_bScrambleTeams; }
	virtual void HandleScrambleTeams( void ){ return; }

	virtual bool PointsMayAlwaysBeBlocked(){ return false; }
	
protected:
	bool m_DisableDeathMessages;

private:
	void RecountTeams( void );
	const char *TeamWithFewestPlayers( void );

	bool m_DisableDeathPenalty;
	bool m_teamLimit;				// This means the server set only some teams as valid
	char m_szTeamList[TEAMPLAY_TEAMLISTLENGTH];
	bool m_bSwitchTeams;
	bool m_bScrambleTeams;

#endif
};

inline CTeamplayRules* TeamplayGameRules()
{
	return static_cast<CTeamplayRules*>(g_pGameRules);
}

// HL1

#define MAX_TEAMNAME_LENGTH	16
#define MAX_TEAMS			32

#define TEAMPLAY_TEAMLISTLENGTH		MAX_TEAMS*MAX_TEAMNAME_LENGTH

class CHalfLifeTeamplay : public CHalfLifeMultiplay
{
public:
	CHalfLifeTeamplay();

	virtual BOOL ClientCommand( CBasePlayerHL1 *pPlayer, const char *pcmd );
	virtual void ClientUserInfoChanged( CBasePlayerHL1 *pPlayer, char *infobuffer );
	virtual BOOL IsTeamplay( void );
	virtual BOOL FPlayerCanTakeDamage( CBasePlayerHL1 *pPlayer, CBaseEntityHL1 *pAttacker );
	virtual int PlayerRelationship( CBaseEntityHL1 *pPlayer, CBaseEntityHL1 *pTarget );
	virtual const char *GetTeamID( CBaseEntityHL1 *pEntity );
	virtual BOOL ShouldAutoAim( CBasePlayerHL1 *pPlayer, edict_t_hl1 *target );
	virtual int IPointsForKill( CBasePlayerHL1 *pAttacker, CBasePlayerHL1 *pKilled );
	virtual void InitHUD( CBasePlayerHL1 *pl );
	virtual void DeathNotice( CBasePlayerHL1 *pVictim, entvars_t *pKiller, entvars_t *pevInflictor );
	virtual const char *GetGameDescription( void ) { return "HL Teamplay"; }  // this is the game name that gets seen in the server browser
	virtual void UpdateGameMode( CBasePlayerHL1 *pPlayer );  // the client needs to be informed of the current game mode
	virtual void PlayerKilled( CBasePlayerHL1 *pVictim, entvars_t *pKiller, entvars_t *pInflictor );
	virtual void Think ( void );
	virtual int GetTeamIndex( const char *pTeamName );
	virtual const char *GetIndexedTeamName( int teamIndex );
	virtual BOOL IsValidTeam( const char *pTeamName );
	const char *SetDefaultPlayerTeam( CBasePlayerHL1 *pPlayer );
	virtual void ChangePlayerTeam( CBasePlayerHL1 *pPlayer, const char *pTeamName, BOOL bKill, BOOL bGib );

private:
	void RecountTeams( bool bResendInfo = FALSE );
	const char *TeamWithFewestPlayers( void );

	BOOL m_DisableDeathMessages;
	BOOL m_DisableDeathPenalty;
	BOOL m_teamLimit;				// This means the server set only some teams as valid
	char m_szTeamList[TEAMPLAY_TEAMLISTLENGTH];
};

#endif // TEAMPLAY_GAMERULES_H
