/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

/*
==========================
This file contains "stubs" of class member implementations so that we can predict certain
 weapons client side.  From time to time you might find that you need to implement part of the
 these functions.  If so, cut it from here, paste it in hl_weapons.cpp or somewhere else and
 add in the functionality you need.
==========================
*/
#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"player.h"
#include	"weapons.h"
#include	"nodes.h"
#include	"soundent.h"
#include	"skill.h"

// Globals used by game logic
const VectorHL1 g_vecZero = VectorHL1( 0, 0, 0 );
int gmsgWeapPickup = 0;
enginefuncs_t g_engfuncsHL1;
globalvars_t  *gpGlobalsHL1;

ItemInfo CBasePlayerItemHL1::ItemInfoArray[MAX_WEAPONS];

void EMIT_SOUND_DYN(edict_t_hl1 *entity, int channel, const char *sample, float volume, float attenuation, int flags, int pitch) { }

// CBaseEntityHL1 Stubs
int CBaseEntityHL1 :: TakeHealth( float flHealth, int bitsDamageType ) { return 1; }
int CBaseEntityHL1 :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType ) { return 1; }
CBaseEntityHL1 *CBaseEntityHL1::GetNextTarget( void ) { return NULL; }
int CBaseEntityHL1::SaveHL1( CSaveHL1 &save ) { return 1; }
int CBaseEntityHL1::RestoreHL1( CRestoreHL1 &restore ) { return 1; }
void CBaseEntityHL1::SetObjectCollisionBox( void ) { }
int	CBaseEntityHL1 :: Intersects( CBaseEntityHL1 *pOther ) { return 0; }
void CBaseEntityHL1 :: MakeDormant( void ) { }
int CBaseEntityHL1 :: IsDormant( void ) { return 0; }
BOOL CBaseEntityHL1 :: IsInWorld( void ) { return TRUE; }
int CBaseEntityHL1::ShouldToggle( USE_TYPE useType, BOOL currentState ) { return 0; }
int	CBaseEntityHL1 :: DamageDecal( int bitsDamageType ) { return -1; }
CBaseEntityHL1 * CBaseEntityHL1::Create( char *szName, const VectorHL1 &vecOrigin, const VectorHL1 &vecAngles, edict_t_hl1 *pentOwner ) { return NULL; }
void CBaseEntityHL1::SUB_Remove( void ) { }

// CBaseDelay Stubs
void CBaseDelay :: KeyValue( struct KeyValueData_s * ) { }
int CBaseDelay::RestoreHL1( class CRestoreHL1 & ) { return 1; }
int CBaseDelay::SaveHL1( class CSaveHL1 & ) { return 1; }

// CBaseAnimatingHL1 Stubs
int CBaseAnimatingHL1::RestoreHL1( class CRestoreHL1 & ) { return 1; }
int CBaseAnimatingHL1::SaveHL1( class CSaveHL1 & ) { return 1; }

// DEBUG Stubs
edict_t_hl1 *DBG_EntOfVars( const entvars_t *pev ) { return NULL; }
void DBG_AssertFunction(BOOL fExpr,	const char*	szExpr,	const char*	szFile,	int szLine,	const char*	szMessage) { }

// UTIL_* Stubs
void UTIL_PrecacheOtherHL1( const char *szClassname ) { }
void UTIL_BloodDrips( const VectorHL1 &origin, const VectorHL1 &direction, int color, int amount ) { }
void UTIL_DecalTrace( TraceResult *pTrace, int decalNumber ) { }
void UTIL_GunshotDecalTrace( TraceResult *pTrace, int decalNumber ) { }
void UTIL_MakeVectors( const VectorHL1 &vecAngles ) { }
BOOL UTIL_IsValidEntity( edict_t_hl1 *pent ) { return TRUE; }
void UTIL_SetOrigin( entvars_t *, const VectorHL1 &org ) { }
BOOL UTIL_GetNextBestWeapon( CBasePlayerHL1 *pPlayer, CBasePlayerItemHL1 *pCurrentWeapon ) { return TRUE; }
void UTIL_LogPrintf(char *,...) { }
void UTIL_ClientPrintAll( int,char const *,char const *,char const *,char const *,char const *) { }
void ClientPrint( entvars_t *client, int msg_dest, const char *msg_name, const char *param1, const char *param2, const char *param3, const char *param4 ) { }

// CBaseToggle Stubs
int CBaseToggleHL1::RestoreHL1( class CRestoreHL1 & ) { return 1; }
int CBaseToggleHL1::SaveHL1( class CSaveHL1 & ) { return 1; }
void CBaseToggleHL1 :: KeyValue( struct KeyValueData_s * ) { }

// CGrenade Stubs
void CGrenade::BounceSound( void ) { }
void CGrenade::Explode( VectorHL1, VectorHL1 ) { }
void CGrenade::Explode( TraceResult *, int ) { }
void CGrenade::Killed( entvars_t *, int ) { }
void CGrenade::Spawn( void ) { }
CGrenade * CGrenade:: ShootTimed( entvars_t *pevOwner, VectorHL1 vecStart, VectorHL1 vecVelocity, float time ){ return 0; }
CGrenade *CGrenade::ShootContact( entvars_t *pevOwner, VectorHL1 vecStart, VectorHL1 vecVelocity ){ return 0; }
void CGrenade::DetonateUse( CBaseEntityHL1 *pActivator, CBaseEntityHL1 *pCaller, USE_TYPE useType, float value ){ }

void UTIL_Remove( CBaseEntityHL1 *pEntity ){ }
struct skilldata_t  gSkillData;
void UTIL_SetSizeHL1( entvars_t *pev, const VectorHL1 &vecMin, const VectorHL1 &vecMax ){ }
CBaseEntityHL1 *UTIL_FindEntityInSphere( CBaseEntityHL1 *pStartEntity, const VectorHL1 &vecCenter, float flRadius ){ return 0;}

VectorHL1 UTIL_VecToAngles( const VectorHL1 &vec ){ return 0; }
CSprite *CSprite::SpriteCreate( const char *pSpriteName, const VectorHL1 &origin, BOOL animate ) { return 0; }
void CBeam::PointEntInit( const VectorHL1 &start, int endIndex ) { }
CBeam *CBeam::BeamCreate( const char *pSpriteName, int width ) { return NULL; }
void CSprite::Expand( float scaleSpeed, float fadeSpeed ) { }


CBaseEntityHL1* CBaseMonster :: CheckTraceHullAttack( float flDist, int iDamage, int iDmgType ) { return NULL; }
void CBaseMonster :: Eat ( float flFullDuration ) { }
BOOL CBaseMonster :: FShouldEat ( void ) { return TRUE; }
void CBaseMonster :: BarnacleVictimBitten ( entvars_t *pevBarnacle ) { }
void CBaseMonster :: BarnacleVictimReleased ( void ) { }
void CBaseMonster :: Listen ( void ) { }
float CBaseMonster :: FLSoundVolume ( CSoundHL1 *pSound ) { return 0.0; }
BOOL CBaseMonster :: FValidateHintType ( short sHint ) { return FALSE; }
void CBaseMonster :: Look ( int iDistance ) { }
int CBaseMonster :: ISoundMask ( void ) { return 0; }
CSoundHL1* CBaseMonster :: PBestSound ( void ) { return NULL; }
CSoundHL1* CBaseMonster :: PBestScent ( void ) { return NULL; } 
float CBaseAnimatingHL1 :: StudioFrameAdvance ( float flInterval ) { return 0.0; }
void CBaseMonster :: MonsterThink ( void ) { }
void CBaseMonster :: MonsterUse ( CBaseEntityHL1 *pActivator, CBaseEntityHL1 *pCaller, USE_TYPE useType, float value ) { }
int CBaseMonster :: IgnoreConditions ( void ) { return 0; }
void CBaseMonster :: RouteClear ( void ) { }
void CBaseMonster :: RouteNew ( void ) { }
BOOL CBaseMonster :: FRouteClear ( void ) { return FALSE; }
BOOL CBaseMonster :: FRefreshRoute ( void ) { return 0; }
BOOL CBaseMonster::MoveToEnemy( Activity movementAct, float waitTime ) { return FALSE; }
BOOL CBaseMonster::MoveToLocation( Activity movementAct, float waitTime, const VectorHL1 &goal ) { return FALSE; }
BOOL CBaseMonster::MoveToTarget( Activity movementAct, float waitTime ) { return FALSE; }
BOOL CBaseMonster::MoveToNode( Activity movementAct, float waitTime, const VectorHL1 &goal ) { return FALSE; }
int ShouldSimplify( int routeType ) { return TRUE; }
void CBaseMonster :: RouteSimplify( CBaseEntityHL1 *pTargetEnt ) { }
BOOL CBaseMonster :: FBecomeProne ( void ) { return TRUE; }
BOOL CBaseMonster :: CheckRangeAttack1 ( float flDot, float flDist ) { return FALSE; }
BOOL CBaseMonster :: CheckRangeAttack2 ( float flDot, float flDist ) { return FALSE; }
BOOL CBaseMonster :: CheckMeleeAttack1 ( float flDot, float flDist ) { return FALSE; }
BOOL CBaseMonster :: CheckMeleeAttack2 ( float flDot, float flDist ) { return FALSE; }
void CBaseMonster :: CheckAttacks ( CBaseEntityHL1 *pTarget, float flDist ) { }
BOOL CBaseMonster :: FCanCheckAttacks ( void ) { return FALSE; }
int CBaseMonster :: CheckEnemy ( CBaseEntityHL1 *pEnemy ) { return 0; }
void CBaseMonster :: PushEnemy( CBaseEntityHL1 *pEnemy, VectorHL1 &vecLastKnownPos ) { }
BOOL CBaseMonster :: PopEnemy( ) { return FALSE; }
void CBaseMonster :: SetActivity ( Activity NewActivity ) { }
void CBaseMonster :: SetSequenceByName ( char *szSequence ) { }
int CBaseMonster :: CheckLocalMove ( const VectorHL1 &vecStart, const VectorHL1 &vecEnd, CBaseEntityHL1 *pTarget, float *pflDist ) { return 0; }
float CBaseMonster :: OpenDoorAndWait( entvars_t *pevDoor ) { return 0.0; }
void CBaseMonster :: AdvanceRoute ( float distance ) { }
int CBaseMonster :: RouteClassify( int iMoveFlag ) { return 0; }
BOOL CBaseMonster :: BuildRoute ( const VectorHL1 &vecGoal, int iMoveFlag, CBaseEntityHL1 *pTarget ) { return FALSE; }
void CBaseMonster :: InsertWaypoint ( VectorHL1 vecLocation, int afMoveFlags ) { }
BOOL CBaseMonster :: FTriangulate ( const VectorHL1 &vecStart , const VectorHL1 &vecEnd, float flDist, CBaseEntityHL1 *pTargetEnt, VectorHL1 *pApex ) { return FALSE; }
void CBaseMonster :: Move ( float flInterval ) { }
BOOL CBaseMonster:: ShouldAdvanceRoute( float flWaypointDist ) { return FALSE; }
void CBaseMonster::MoveExecute( CBaseEntityHL1 *pTargetEnt, const VectorHL1 &vecDir, float flInterval ) { }
void CBaseMonster :: MonsterInit ( void ) { }
void CBaseMonster :: MonsterInitThink ( void ) { }
void CBaseMonster :: StartMonster ( void ) { }
void CBaseMonster :: MovementComplete( void ) { }
int CBaseMonster::TaskIsRunning( void ) { return 0; }
int CBaseMonster::IRelationship ( CBaseEntityHL1 *pTarget ) { return 0; }
BOOL CBaseMonster :: FindCover ( VectorHL1 vecThreat, VectorHL1 vecViewOffset, float flMinDist, float flMaxDist ) { return FALSE; }
BOOL CBaseMonster :: BuildNearestRoute ( VectorHL1 vecThreat, VectorHL1 vecViewOffset, float flMinDist, float flMaxDist ) { return FALSE; }
CBaseEntityHL1 *CBaseMonster :: BestVisibleEnemy ( void ) { return NULL; }
BOOL CBaseMonster :: FInViewCone ( CBaseEntityHL1 *pEntity ) { return FALSE; }
BOOL CBaseMonster :: FInViewCone ( VectorHL1 *pOrigin ) { return FALSE; }
BOOL CBaseEntityHL1 :: FVisible ( CBaseEntityHL1 *pEntity ) { return FALSE; }
BOOL CBaseEntityHL1 :: FVisible ( const VectorHL1 &vecOrigin ) { return FALSE; }
void CBaseMonster :: MakeIdealYaw( VectorHL1 vecTarget ) { }
float	CBaseMonster::FlYawDiff ( void ) { return 0.0; }
float CBaseMonster::ChangeYaw ( int yawSpeed ) { return 0; }
float	CBaseMonster::VecToYaw ( VectorHL1 vecDir ) { return 0.0; }
int CBaseAnimatingHL1 :: LookupActivity ( int Activity ) { return 0; }
int CBaseAnimatingHL1 :: LookupActivityHeaviest ( int Activity ) { return 0; }
void CBaseMonster :: SetEyePosition ( void ) { }
int CBaseAnimatingHL1 :: LookupSequence ( const char *label ) { return 0; }
void CBaseAnimatingHL1 :: ResetSequenceInfo ( ) { }
BOOL CBaseAnimatingHL1 :: GetSequenceFlags( ) { return FALSE; }
void CBaseAnimatingHL1 :: DispatchAnimEvents ( float flInterval ) { }
void CBaseMonster :: HandleAnimEvent( MonsterEvent_t *pEvent ) { }
float CBaseAnimatingHL1 :: SetBoneController ( int iController, float flValue ) { return 0.0; }
void CBaseAnimatingHL1 :: InitBoneControllers ( void ) { }
float CBaseAnimatingHL1 :: SetBlending ( int iBlender, float flValue ) { return 0; }
void CBaseAnimatingHL1 :: GetBonePosition ( int iBone, VectorHL1 &origin, VectorHL1 &angles ) { }
void CBaseAnimatingHL1 :: GetAttachment ( int iAttachment, VectorHL1 &origin, VectorHL1 &angles ) { }
int CBaseAnimatingHL1 :: FindTransition( int iEndingSequence, int iGoalSequence, int *piDir ) { return -1; }
void CBaseAnimatingHL1 :: GetAutomovement( VectorHL1 &origin, VectorHL1 &angles, float flInterval ) { }
void CBaseAnimatingHL1 :: SetBodygroup( int iGroup, int iValue ) { }
int CBaseAnimatingHL1 :: GetBodygroup( int iGroup ) { return 0; }
VectorHL1 CBaseMonster :: GetGunPosition( void ) { return g_vecZero; }
void CBaseEntityHL1::TraceAttack(entvars_t *pevAttacker, float flDamage, VectorHL1 vecDir, TraceResult *ptr, int bitsDamageType) { }
void CBaseEntityHL1::FireBullets(ULONG cShots, VectorHL1 vecSrc, VectorHL1 vecDirShooting, VectorHL1 vecSpread, float flDistance, int iBulletType, int iTracerFreq, int iDamage, entvars_t *pevAttacker ) { }
void CBaseEntityHL1 :: TraceBleed( float flDamage, VectorHL1 vecDir, TraceResult *ptr, int bitsDamageType ) { }
void CBaseMonster :: MakeDamageBloodDecal ( int cCount, float flNoise, TraceResult *ptr, const VectorHL1 &vecDir ) { }
BOOL CBaseMonster :: FGetNodeRoute ( VectorHL1 vecDest ) { return TRUE; }
int CBaseMonster :: FindHintNode ( void ) { return NO_NODE; }
void CBaseMonster::ReportAIState( void ) { }
void CBaseMonster :: KeyValue( KeyValueData *pkvd ) { }
BOOL CBaseMonster :: FCheckAITrigger ( void ) { return FALSE; }
int CBaseMonster :: CanPlaySequence( BOOL fDisregardMonsterState, int interruptLevel ) { return FALSE; }
BOOL CBaseMonster :: FindLateralCover ( const VectorHL1 &vecThreat, const VectorHL1 &vecViewOffset ) { return FALSE; }
VectorHL1 CBaseMonster :: ShootAtEnemy( const VectorHL1 &shootOrigin ) { return g_vecZero; }
BOOL CBaseMonster :: FacingIdeal( void ) { return FALSE; }
BOOL CBaseMonster :: FCanActiveIdle ( void ) { return FALSE; }
void CBaseMonster::PlaySentence( const char *pszSentence, float duration, float volume, float attenuation ) { }
void CBaseMonster::PlayScriptedSentence( const char *pszSentence, float duration, float volume, float attenuation, BOOL bConcurrent, CBaseEntityHL1 *pListener ) { }
void CBaseMonster::SentenceStop( void ) { }
void CBaseMonster::CorpseFallThink( void ) { }
void CBaseMonster :: MonsterInitDead( void ) { }
BOOL CBaseMonster :: BBoxFlat ( void ) { return TRUE; }
BOOL CBaseMonster :: GetEnemy ( void ) { return FALSE; }
void CBaseMonster :: TraceAttack( entvars_t *pevAttacker, float flDamage, VectorHL1 vecDir, TraceResult *ptr, int bitsDamageType) { }
CBaseEntityHL1* CBaseMonster :: DropItem ( char *pszItemName, const VectorHL1 &vecPos, const VectorHL1 &vecAng ) { return NULL; }
BOOL CBaseMonster :: ShouldFadeOnDeath( void ) { return FALSE; }
void CBaseMonster :: RadiusDamage(entvars_t* pevInflictor, entvars_t*	pevAttacker, float flDamage, int iClassIgnore, int bitsDamageType ) { }
void CBaseMonster :: RadiusDamage( VectorHL1 vecSrc, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int iClassIgnore, int bitsDamageType ) { }
void CBaseMonster::FadeMonster( void ) { }
void CBaseMonster :: GibMonster( void ) { }
BOOL CBaseMonster :: HasHumanGibs( void ) { return FALSE; }
BOOL CBaseMonster :: HasAlienGibs( void ) { return FALSE; }
Activity CBaseMonster :: GetDeathActivity ( void ) { return ACT_DIE_HEADSHOT; }
MONSTERSTATE CBaseMonster :: GetIdealState ( void ) { return MONSTERSTATE_ALERT; }
Schedule_t* CBaseMonster :: GetScheduleOfType ( int Type ) { return NULL; }
Schedule_t *CBaseMonster :: GetSchedule ( void ) { return NULL; }
void CBaseMonster :: RunTask ( Task_t_hl1 *pTask ) { }
void CBaseMonster :: StartTask ( Task_t_hl1 *pTask ) { }
Schedule_t *CBaseMonster::ScheduleFromName( const char *pName ) { return NULL;}
void CBaseMonster::BecomeDead( void ) {}
void CBaseMonster :: RunAI ( void ) {}
void CBaseMonster :: Killed( entvars_t *pevAttacker, int iGib ) {}
int CBaseMonster :: TakeHealth (float flHealth, int bitsDamageType) { return 0; }
int CBaseMonster :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType ) { return 0; }
int CBaseMonster::RestoreHL1( class CRestoreHL1 & ) { return 1; }
int CBaseMonster::SaveHL1( class CSaveHL1 & ) { return 1; }

int TrainSpeed(int iSpeed, int iMax) { 	return 0; }
void CBasePlayerHL1 :: DeathSound( void ) { }
int CBasePlayerHL1 :: TakeHealth( float flHealth, int bitsDamageType ) { return 0; }
void CBasePlayerHL1 :: TraceAttack( entvars_t *pevAttacker, float flDamage, VectorHL1 vecDir, TraceResult *ptr, int bitsDamageType) { }
int CBasePlayerHL1 :: TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType ) { return 0; }
void CBasePlayerHL1::PackDeadPlayerItems( void ) { }
void CBasePlayerHL1::RemoveAllItems( BOOL removeSuit ) { }
void CBasePlayerHL1::SetAnimation( PLAYER_ANIM playerAnim ) { }
void CBasePlayerHL1::WaterMove() { }
BOOL CBasePlayerHL1::IsOnLadder( void ) { return FALSE; }
void CBasePlayerHL1::PlayerDeathThink(void) { }
void CBasePlayerHL1::StartDeathCam( void ) { }
void CBasePlayerHL1::StartObserver( VectorHL1 vecPosition, VectorHL1 vecViewAngle ) { }
void CBasePlayerHL1::PlayerUse ( void ) { }
void CBasePlayerHL1::Jump() { }
void CBasePlayerHL1::Duck( ) { }
int  CBasePlayerHL1::Classify ( void ) { return 0; }
void CBasePlayerHL1::PreThink(void) { }
void CBasePlayerHL1::CheckTimeBasedDamage()  { }
void CBasePlayerHL1 :: UpdateGeigerCounter( void ) { }
void CBasePlayerHL1::CheckSuitUpdate() { }
void CBasePlayerHL1::SetSuitUpdate(char *name, int fgroup, int iNoRepeatTime) { }
void CBasePlayerHL1 :: UpdatePlayerSound ( void ) { }
void CBasePlayerHL1::PostThink() { }
void CBasePlayerHL1 :: Precache( void ) { }
int CBasePlayerHL1::SaveHL1( CSaveHL1 &save ) { return 0; }
void CBasePlayerHL1::RenewItems(void) { }
int CBasePlayerHL1::RestoreHL1( CRestoreHL1 &restore ) { return 0; }
void CBasePlayerHL1::SelectNextItem( int iItem ) { }
BOOL CBasePlayerHL1::HasWeapons( void ) { return FALSE; }
void CBasePlayerHL1::SelectPrevItem( int iItem ) { }
CBaseEntityHL1 *FindEntityForward( CBaseEntityHL1 *pMe ) { return NULL; }
BOOL CBasePlayerHL1 :: FlashlightIsOn( void ) { return FALSE; }
void CBasePlayerHL1 :: FlashlightTurnOn( void ) { }
void CBasePlayerHL1 :: FlashlightTurnOff( void ) { }
void CBasePlayerHL1 :: ForceClientDllUpdate( void ) { }
void CBasePlayerHL1::ImpulseCommands( ) { }
void CBasePlayerHL1::CheatImpulseCommands( int iImpulse ) { }
int CBasePlayerHL1::AddPlayerItem( CBasePlayerItemHL1 *pItem ) { return FALSE; }
int CBasePlayerHL1::RemovePlayerItem( CBasePlayerItemHL1 *pItem ) { return FALSE; }
void CBasePlayerHL1::ItemPreFrame() { }
void CBasePlayerHL1::ItemPostFrame() { }
int CBasePlayerHL1::AmmoInventory( int iAmmoIndex ) { return -1; }
int CBasePlayerHL1::GetAmmoIndex(const char *psz) { return -1; }
void CBasePlayerHL1::SendAmmoUpdate(void) { }
void CBasePlayerHL1 :: UpdateClientData( void ) { }
BOOL CBasePlayerHL1 :: FBecomeProne ( void ) { return TRUE; }
void CBasePlayerHL1 :: BarnacleVictimBitten ( entvars_t *pevBarnacle ) { }
void CBasePlayerHL1 :: BarnacleVictimReleased ( void ) { }
int CBasePlayerHL1 :: Illumination( void ) { return 0; }
void CBasePlayerHL1 :: EnableControl(BOOL fControl) { }
VectorHL1 CBasePlayerHL1 :: GetAutoaimVector( float flDelta ) { return g_vecZero; }
VectorHL1 CBasePlayerHL1 :: AutoaimDeflection( VectorHL1 &vecSrc, float flDist, float flDelta  ) { return g_vecZero; }
void CBasePlayerHL1 :: ResetAutoaim( ) { }
void CBasePlayerHL1 :: SetCustomDecalFrames( int nFrames ) { }
int CBasePlayerHL1 :: GetCustomDecalFrames( void ) { return -1; }
void CBasePlayerHL1::DropPlayerItem ( char *pszItemName ) { }
BOOL CBasePlayerHL1::HasPlayerItem( CBasePlayerItemHL1 *pCheckItem ) { return FALSE; }
BOOL CBasePlayerHL1 :: SwitchWeapon( CBasePlayerItemHL1 *pWeapon )  { return FALSE; }
VectorHL1 CBasePlayerHL1 :: GetGunPosition( void ) { return g_vecZero; }
const char *CBasePlayerHL1::TeamID( void ) { return ""; }
int CBasePlayerHL1 :: GiveAmmo( int iCount, char *szName, int iMax ) { return 0; }
void CBasePlayerHL1::AddPoints( int score, BOOL bAllowNegativeScore ) { } 
void CBasePlayerHL1::AddPointsToTeam( int score, BOOL bAllowNegativeScore ) { } 

void ClearMultiDamage(void) { }
void ApplyMultiDamage(entvars_t *pevInflictor, entvars_t *pevAttacker ) { }
void AddMultiDamage( entvars_t *pevInflictor, CBaseEntityHL1 *pEntity, float flDamage, int bitsDamageType) { }
void SpawnBlood(VectorHL1 vecSpot, int bloodColor, float flDamage) { }
int DamageDecal( CBaseEntityHL1 *pEntity, int bitsDamageType ) { return 0; }
void DecalGunshot( TraceResult *pTrace, int iBulletType ) { }
void EjectBrass ( const VectorHL1 &vecOrigin, const VectorHL1 &vecVelocity, float rotation, int model, int soundtype ) { }
void AddAmmoNameToAmmoRegistry( const char *szAmmoname ) { }
int CBasePlayerItemHL1::RestoreHL1( class CRestoreHL1 & ) { return 1; }
int CBasePlayerItemHL1::SaveHL1( class CSaveHL1 & ) { return 1; }
int CBasePlayerWeaponHL1::RestoreHL1( class CRestoreHL1 & ) { return 1; }
int CBasePlayerWeaponHL1::SaveHL1( class CSaveHL1 & ) { return 1; }
void CBasePlayerItemHL1 :: SetObjectCollisionBox( void ) { }
void CBasePlayerItemHL1 :: FallInit( void ) { }
void CBasePlayerItemHL1::FallThink ( void ) { }
void CBasePlayerItemHL1::Materialize( void ) { }
void CBasePlayerItemHL1::AttemptToMaterialize( void ) { }
void CBasePlayerItemHL1 :: CheckRespawn ( void ) { }
CBaseEntityHL1* CBasePlayerItemHL1::Respawn( void ) { return NULL; }
void CBasePlayerItemHL1::DefaultTouch( CBaseEntityHL1 *pOther ) { }
void CBasePlayerItemHL1::DestroyItem( void ) { }
int CBasePlayerItemHL1::AddToPlayer( CBasePlayerHL1 *pPlayer ) { return TRUE; }
void CBasePlayerItemHL1::Drop( void ) { }
void CBasePlayerItemHL1::Kill( void ) { }
void CBasePlayerItemHL1::Holster( int skiplocal ) { }
void CBasePlayerItemHL1::AttachToPlayer ( CBasePlayerHL1 *pPlayer ) { }
int CBasePlayerWeaponHL1::AddDuplicate( CBasePlayerItemHL1 *pOriginal ) { return 0; }
int CBasePlayerWeaponHL1::AddToPlayer( CBasePlayerHL1 *pPlayer ) { return FALSE; }
int CBasePlayerWeaponHL1::UpdateClientData( CBasePlayerHL1 *pPlayer ) { return 0; }
BOOL CBasePlayerWeaponHL1 :: AddPrimaryAmmo( int iCount, char *szName, int iMaxClip, int iMaxCarry ) { return TRUE; }
BOOL CBasePlayerWeaponHL1 :: AddSecondaryAmmo( int iCount, char *szName, int iMax ) { return TRUE; }
BOOL CBasePlayerWeaponHL1 :: IsUseable( void ) { return TRUE; }
int CBasePlayerWeaponHL1::PrimaryAmmoIndex( void ) { return -1; }
int CBasePlayerWeaponHL1::SecondaryAmmoIndex( void ) {	return -1; }
void CBasePlayerAmmoHL1::Spawn( void ) { }
CBaseEntityHL1* CBasePlayerAmmoHL1::Respawn( void ) { return this; }
void CBasePlayerAmmoHL1::Materialize( void ) { }
void CBasePlayerAmmoHL1 :: DefaultTouch( CBaseEntityHL1 *pOther ) { }
int CBasePlayerWeaponHL1::ExtractAmmo( CBasePlayerWeaponHL1 *pWeapon ) { return 0; }
int CBasePlayerWeaponHL1::ExtractClipAmmo( CBasePlayerWeaponHL1 *pWeapon ) { return 0; }	
void CBasePlayerWeaponHL1::RetireWeapon( void ) { }
void CSoundEntHL1::InsertSound ( int iType, const VectorHL1 &vecOrigin, int iVolume, float flDuration ) {}
void RadiusDamage( VectorHL1 vecSrc, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, float flRadius, int iClassIgnore, int bitsDamageType ){}