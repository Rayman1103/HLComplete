//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef VOICE_GAMEMGR_H
#define VOICE_GAMEMGR_H
#pragma once


#include "voice_common.h"


class CGameRules;
class CBasePlayer;

abstract_class IVoiceGameMgrHelper
{
public:
	virtual				~IVoiceGameMgrHelper() {}

	// Called each frame to determine which players are allowed to hear each other.	This overrides
	// whatever squelch settings players have.
	virtual bool		CanPlayerHearPlayer(CBasePlayer *pListener, CBasePlayer *pTalker, bool &bProximity ) = 0;
};


// CVoiceGameMgr manages which clients can hear which other clients.
class CVoiceGameMgr
{
public:
						CVoiceGameMgr();
	virtual				~CVoiceGameMgr();
	
	bool				Init(
		IVoiceGameMgrHelper *m_pHelper,
		int maxClients
		);

	void				SetHelper(IVoiceGameMgrHelper *pHelper);

	// Updates which players can hear which other players.
	// If gameplay mode is DM, then only players within the PVS can hear each other.
	// If gameplay mode is teamplay, then only players on the same team can hear each other.
	// Player masks are always applied.
	void				Update(double frametime);

	// Called when a new client connects (unsquelches its entity for everyone).
	void				ClientConnected(struct edict_t *pEdict);

	// Called on ClientCommand. Checks for the squelch and unsquelch commands.
	// Returns true if it handled the command.
	bool				ClientCommand(CBasePlayer *pPlayer, const CCommand &args );

	bool				CheckProximity( int iDistance );
	void				SetProximityDistance( int iDistance );

	bool				IsPlayerIgnoringPlayer( int iTalker, int iListener );

private:

	// Force it to update the client masks.
	void				UpdateMasks();


private:
	IVoiceGameMgrHelper	*m_pHelper;
	int					m_nMaxPlayers;
	double				m_UpdateInterval;						// How long since the last update.
	int					m_iProximityDistance;
};


// Use this to access CVoiceGameMgr.
CVoiceGameMgr* GetVoiceGameMgr();

// HL1

class CGameRulesHL1;
class CBasePlayerHL1;


class IVoiceGameMgrHelperHL1
{
public:
	virtual				~IVoiceGameMgrHelperHL1() {}

	// Called each frame to determine which players are allowed to hear each other.	This overrides
	// whatever squelch settings players have.
	virtual bool		CanPlayerHearPlayer(CBasePlayerHL1 *pListener, CBasePlayerHL1 *pTalker) = 0;
};


// CVoiceGameMgr manages which clients can hear which other clients.
class CVoiceGameMgrHL1
{
public:
						CVoiceGameMgrHL1();
	virtual				~CVoiceGameMgrHL1();
	
	bool				Init(
		IVoiceGameMgrHelperHL1 *m_pHelper,
		int maxClients
		);

	void				SetHelper(IVoiceGameMgrHelperHL1 *pHelper);

	// Updates which players can hear which other players.
	// If gameplay mode is DM, then only players within the PVS can hear each other.
	// If gameplay mode is teamplay, then only players on the same team can hear each other.
	// Player masks are always applied.
	void				Update(double frametime);

	// Called when a new client connects (unsquelches its entity for everyone).
	void				ClientConnected(struct edict_s *pEdict);

	// Called on ClientCommand. Checks for the squelch and unsquelch commands.
	// Returns true if it handled the command.
	bool				ClientCommand(CBasePlayerHL1 *pPlayer, const char *cmd);

	// Called to determine if the Receiver has muted (blocked) the Sender
	// Returns true if the receiver has blocked the sender
	bool				PlayerHasBlockedPlayer(CBasePlayerHL1 *pReceiver, CBasePlayerHL1 *pSender);


private:

	// Force it to update the client masks.
	void				UpdateMasks();


private:
	int					m_msgPlayerVoiceMask;
	int					m_msgRequestState;

	IVoiceGameMgrHelperHL1	*m_pHelper;
	int					m_nMaxPlayers;
	double				m_UpdateInterval;						// How long since the last update.
};



#endif // VOICE_GAMEMGR_H
