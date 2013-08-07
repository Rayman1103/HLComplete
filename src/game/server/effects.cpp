//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements a grab bag of visual effects entities.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "effects.h"
#include "gib.h"
#include "beam_shared.h"
#include "decals.h"
#include "func_break.h"
#include "EntityFlame.h"
#include "entitylist.h"
#include "basecombatweapon.h"
#include "model_types.h"
#include "player.h"
#include "physics.h"
#include "baseparticleentity.h"
#include "ndebugoverlay.h"
#include "IEffects.h"
#include "vstdlib/random.h"
#include "env_wind_shared.h"
#include "filesystem.h"
#include "engine/IEngineSound.h"
#include "fire.h"
#include "te_effect_dispatch.h"
#include "Sprite.h"
#include "precipitation_shared.h"
#include "shot_manipulator.h"

//HL1
#include "extdll.h"
#include "util.h"
#include "monsters.h"
#include "customentity.h"
#include "weapons.h"
#include "shake.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SF_FUNNEL_REVERSE			1 // funnel effect repels particles instead of attracting them.

#define	SF_GIBSHOOTER_REPEATABLE	(1<<0)	// allows a gibshooter to be refired
#define	SF_SHOOTER_FLAMING			(1<<1)	// gib is on fire
#define SF_SHOOTER_STRICT_REMOVE	(1<<2)	// remove this gib even if it is in the player's view

// UNDONE: This should be client-side and not use TempEnts
class CBubbling : public CBaseEntity
{
public:
	DECLARE_CLASS( CBubbling, CBaseEntity );

	virtual  void	Spawn( void );
	virtual void	Precache( void );

	void	FizzThink( void );

	// Input handlers.
	void	InputActivate( inputdata_t &inputdata );
	void	InputDeactivate( inputdata_t &inputdata );
	void	InputToggle( inputdata_t &inputdata );

	void	InputSetCurrent( inputdata_t &inputdata );
	void	InputSetDensity( inputdata_t &inputdata );
	void	InputSetFrequency( inputdata_t &inputdata );

	DECLARE_DATADESC();

private:

	void TurnOn();
	void TurnOff();
	void Toggle();

	int		m_density;
	int		m_frequency;
	int		m_bubbleModel;
	int		m_state;
};

LINK_ENTITY_TO_CLASS( env_bubbles, CBubbling );

BEGIN_DATADESC( CBubbling )

	DEFINE_KEYFIELD( m_flSpeed, FIELD_FLOAT, "current" ),
	DEFINE_KEYFIELD( m_density, FIELD_INTEGER, "density" ),
	DEFINE_KEYFIELD( m_frequency, FIELD_INTEGER, "frequency" ),

	DEFINE_FIELD( m_state, FIELD_INTEGER ),
	// Let spawn restore this!
	//	DEFINE_FIELD( m_bubbleModel, FIELD_INTEGER ),

	// Function Pointers
	DEFINE_FUNCTION( FizzThink ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Activate", InputActivate ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Deactivate", InputDeactivate ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Toggle", InputToggle ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetCurrent", InputSetCurrent ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetDensity", InputSetDensity ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetFrequency", InputSetFrequency ),

END_DATADESC()



#define SF_BUBBLES_STARTOFF		0x0001

void CBubbling::Spawn( void )
{
	Precache( );
	SetModel( STRING( GetModelName() ) );		// Set size

	// Make it invisible to client
	SetRenderColorA( 0 );

	SetSolid( SOLID_NONE );						// Remove model & collisions

	if ( !HasSpawnFlags(SF_BUBBLES_STARTOFF) )
	{
		SetThink( &CBubbling::FizzThink );
		SetNextThink( gpGlobals->curtime + 2.0 );
		m_state = 1;
	}
	else
	{
		m_state = 0;
	}
}

void CBubbling::Precache( void )
{
	m_bubbleModel = PrecacheModel("sprites/bubble.vmt");			// Precache bubble sprite
}


void CBubbling::Toggle()
{
	if (!m_state)
	{
		TurnOn();
	}
	else
	{
		TurnOff();
	}
}


void CBubbling::TurnOn()
{
	m_state = 1;
	SetThink( &CBubbling::FizzThink );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CBubbling::TurnOff()
{
	m_state = 0;
	SetThink( NULL );
	SetNextThink( TICK_NEVER_THINK );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBubbling::InputActivate( inputdata_t &inputdata )
{
	TurnOn();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBubbling::InputDeactivate( inputdata_t &inputdata )
{
	TurnOff();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBubbling::InputToggle( inputdata_t &inputdata )
{
	Toggle();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : &inputdata -
//-----------------------------------------------------------------------------
void CBubbling::InputSetCurrent( inputdata_t &inputdata )
{
	m_flSpeed = (float)inputdata.value.Int();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : &inputdata -
//-----------------------------------------------------------------------------
void CBubbling::InputSetDensity( inputdata_t &inputdata )
{
	m_density = inputdata.value.Int();
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : &inputdata -
//-----------------------------------------------------------------------------
void CBubbling::InputSetFrequency( inputdata_t &inputdata )
{
	m_frequency = inputdata.value.Int();

	// Reset think time
	if ( m_state )
	{
		if ( m_frequency > 19 )
		{
			SetNextThink( gpGlobals->curtime + 0.5f );
		}
		else
		{
			SetNextThink( gpGlobals->curtime + 2.5 - (0.1 * m_frequency) );
		}
	}
}

void CBubbling::FizzThink( void )
{
	Vector center = WorldSpaceCenter();
	CPASFilter filter( center );
	te->Fizz( filter, 0.0, this, m_bubbleModel, m_density, (int)m_flSpeed );

	if ( m_frequency > 19 )
	{
		SetNextThink( gpGlobals->curtime + 0.5f );
	}
	else
	{
		SetNextThink( gpGlobals->curtime + 2.5 - (0.1 * m_frequency) );
	}
}


// ENV_TRACER
// Fakes a tracer
class CEnvTracer : public CPointEntity
{
public:
	DECLARE_CLASS( CEnvTracer, CPointEntity );

	void Spawn( void );
	void TracerThink( void );
	void Activate( void );

	DECLARE_DATADESC();

	Vector m_vecEnd;
	float  m_flDelay;
};

LINK_ENTITY_TO_CLASS( env_tracer, CEnvTracer );

BEGIN_DATADESC( CEnvTracer )

	DEFINE_KEYFIELD( m_flDelay, FIELD_FLOAT, "delay" ),

	DEFINE_FIELD( m_vecEnd, FIELD_POSITION_VECTOR ),

	// Function Pointers
	DEFINE_FUNCTION( TracerThink ),

END_DATADESC()



//-----------------------------------------------------------------------------
// Purpose: Called after keyvalues are parsed.
//-----------------------------------------------------------------------------
void CEnvTracer::Spawn( void )
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );

	if (!m_flDelay)
		m_flDelay = 1;
}


//-----------------------------------------------------------------------------
// Purpose: Called after all the entities have been loaded.
//-----------------------------------------------------------------------------
void CEnvTracer::Activate( void )
{
	BaseClass::Activate();

	CBaseEntity *pEnd = gEntList.FindEntityByName( NULL, m_target );
	if (pEnd != NULL)
	{
		m_vecEnd = pEnd->GetLocalOrigin();
		SetThink( &CEnvTracer::TracerThink );
		SetNextThink( gpGlobals->curtime + m_flDelay );
	}
	else
	{
		Msg( "env_tracer: unknown entity \"%s\"\n", STRING(m_target) );
	}
}

// Think
void CEnvTracer::TracerThink( void )
{
	UTIL_Tracer( GetAbsOrigin(), m_vecEnd );

	SetNextThink( gpGlobals->curtime + m_flDelay );
}


//#################################################################################
//  >> CGibShooter
//#################################################################################
enum GibSimulation_t
{
	GIB_SIMULATE_POINT,
	GIB_SIMULATE_PHYSICS,
	GIB_SIMULATE_RAGDOLL,
};

class CGibShooter : public CBaseEntity
{
public:
	DECLARE_CLASS( CGibShooter, CBaseEntity );

	void Spawn( void );
	void Precache( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	virtual CGib *CreateGib( void );

protected:
	// Purpose:
	CBaseEntity *SpawnGib( const Vector &vecShootDir, float flSpeed );

	DECLARE_DATADESC();
private:
	void InitPointGib( CGib *pGib, const Vector &vecShootDir, float flSpeed );
	void ShootThink( void );

protected:
	int		m_iGibs;
	int		m_iGibCapacity;
	int		m_iGibMaterial;
	int		m_iGibModelIndex;
	float	m_flGibVelocity;
	QAngle	m_angGibRotation;
	float	m_flGibAngVelocity;
	float	m_flVariance;
	float	m_flGibLife;
	int		m_nSimulationType;
	int		m_nMaxGibModelFrame;
	float	m_flDelay;

	bool	m_bNoGibShadows;

	bool	m_bIsSprite;
	string_t m_iszLightingOrigin;

	// ----------------
	//	Inputs
	// ----------------
	void InputShoot( inputdata_t &inputdata );
};

BEGIN_DATADESC( CGibShooter )

	DEFINE_KEYFIELD( m_iGibs, FIELD_INTEGER, "m_iGibs" ),
	DEFINE_KEYFIELD( m_flGibVelocity, FIELD_FLOAT, "m_flVelocity" ),
	DEFINE_KEYFIELD( m_flVariance, FIELD_FLOAT, "m_flVariance" ),
	DEFINE_KEYFIELD( m_flGibLife, FIELD_FLOAT, "m_flGibLife" ),
	DEFINE_KEYFIELD( m_nSimulationType, FIELD_INTEGER, "Simulation" ),
	DEFINE_KEYFIELD( m_flDelay, FIELD_FLOAT, "delay" ),
	DEFINE_KEYFIELD( m_angGibRotation, FIELD_VECTOR, "gibangles" ),
	DEFINE_KEYFIELD( m_flGibAngVelocity, FIELD_FLOAT, "gibanglevelocity"),
	DEFINE_FIELD( m_bIsSprite, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_iGibCapacity, FIELD_INTEGER ),
	DEFINE_FIELD( m_iGibMaterial, FIELD_INTEGER ),
	DEFINE_FIELD( m_iGibModelIndex, FIELD_INTEGER ),
	DEFINE_FIELD( m_nMaxGibModelFrame, FIELD_INTEGER ),

	DEFINE_KEYFIELD( m_iszLightingOrigin, FIELD_STRING, "LightingOrigin" ),
	DEFINE_KEYFIELD( m_bNoGibShadows, FIELD_BOOLEAN, "nogibshadows" ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_VOID,	"Shoot", InputShoot ),

	// Function Pointers
	DEFINE_FUNCTION( ShootThink ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( gibshooter, CGibShooter );


void CGibShooter::Precache ( void )
{
	if ( g_Language.GetInt() == LANGUAGE_GERMAN )
	{
		m_iGibModelIndex = PrecacheModel ("models/germanygibs.mdl");
	}
	else
	{
		m_iGibModelIndex = PrecacheModel ("models/gibs/hgibs.mdl");
	}
}


void CGibShooter::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	SetThink( &CGibShooter::ShootThink );
	SetNextThink( gpGlobals->curtime );
}


//-----------------------------------------------------------------------------
// Purpose: Input handler for shooting gibs.
//-----------------------------------------------------------------------------
void CGibShooter::InputShoot( inputdata_t &inputdata )
{
	SetThink( &CGibShooter::ShootThink );
	SetNextThink( gpGlobals->curtime );
}


void CGibShooter::Spawn( void )
{
	Precache();

	SetSolid( SOLID_NONE );
	AddEffects( EF_NODRAW );

	if ( m_flDelay < 0 )
	{
		m_flDelay = 0.0;
	}

	if ( m_flGibLife == 0 )
	{
		m_flGibLife = 25;
	}

	m_iGibCapacity = m_iGibs;

	m_nMaxGibModelFrame = modelinfo->GetModelFrameCount( modelinfo->GetModel( m_iGibModelIndex ) );
}

CGib *CGibShooter::CreateGib ( void )
{
	ConVarRef violence_hgibs( "violence_hgibs" );
	if ( violence_hgibs.IsValid() && !violence_hgibs.GetInt() )
		return NULL;

	CGib *pGib = CREATE_ENTITY( CGib, "gib" );
	pGib->Spawn( "models/gibs/hgibs.mdl" );
	pGib->SetBloodColor( BLOOD_COLOR_RED );

	if ( m_nMaxGibModelFrame <= 1 )
	{
		DevWarning( 2, "GibShooter Body is <= 1!\n" );
	}

	pGib->m_nBody = random->RandomInt ( 1, m_nMaxGibModelFrame - 1 );// avoid throwing random amounts of the 0th gib. (skull).

	if ( m_iszLightingOrigin != NULL_STRING )
	{
		// Make the gibs use the lighting origin
		pGib->SetLightingOrigin( m_iszLightingOrigin );
	}

	return pGib;
}


void CGibShooter::InitPointGib( CGib *pGib, const Vector &vecShootDir, float flSpeed )
{
	if ( pGib )
	{
		pGib->SetLocalOrigin( GetAbsOrigin() );
		pGib->SetAbsVelocity( vecShootDir * flSpeed );

		QAngle angVel( random->RandomFloat ( 100, 200 ), random->RandomFloat ( 100, 300 ), 0 );
		pGib->SetLocalAngularVelocity( angVel );

		float thinkTime = ( pGib->GetNextThink() - gpGlobals->curtime );

		pGib->m_lifeTime = (m_flGibLife * random->RandomFloat( 0.95, 1.05 ));	// +/- 5%

		// HL1 gibs always die after a certain time, other games have to opt-in
#ifndef HL1_DLL
		if( HasSpawnFlags( SF_SHOOTER_STRICT_REMOVE ) )
#endif
		{
			pGib->SetNextThink( gpGlobals->curtime + pGib->m_lifeTime );
			pGib->SetThink ( &CGib::DieThink );
		}

		if ( pGib->m_lifeTime < thinkTime )
		{
			pGib->SetNextThink( gpGlobals->curtime + pGib->m_lifeTime );
			pGib->m_lifeTime = 0;
		}

		if ( m_bIsSprite == true )
		{
			pGib->SetSprite( CSprite::SpriteCreate( STRING( GetModelName() ), pGib->GetAbsOrigin(), false ) );

			CSprite *pSprite = (CSprite*)pGib->GetSprite();

			if ( pSprite )
			{
				pSprite->SetAttachment( pGib, 0 );
				pSprite->SetOwnerEntity( pGib );

				pSprite->SetScale( 1 );
				pSprite->SetTransparency( m_nRenderMode, m_clrRender->r, m_clrRender->g, m_clrRender->b, m_clrRender->a, m_nRenderFX );
				pSprite->AnimateForTime( 5, m_flGibLife + 1 ); //This framerate is totally wrong
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CBaseEntity *CGibShooter::SpawnGib( const Vector &vecShootDir, float flSpeed )
{
	switch (m_nSimulationType)
	{
		case GIB_SIMULATE_RAGDOLL:
		{
			// UNDONE: Assume a mass of 200 for now
			Vector force = vecShootDir * flSpeed * 200;
			return CreateRagGib( STRING( GetModelName() ), GetAbsOrigin(), GetAbsAngles(), force, m_flGibLife );
		}

		case GIB_SIMULATE_PHYSICS:
		{
			CGib *pGib = CreateGib();

			if ( pGib )
			{
				pGib->SetAbsOrigin( GetAbsOrigin() );
				pGib->SetAbsAngles( m_angGibRotation );

				pGib->m_lifeTime = (m_flGibLife * random->RandomFloat( 0.95, 1.05 ));	// +/- 5%

				pGib->SetCollisionGroup( COLLISION_GROUP_DEBRIS );
				IPhysicsObject *pPhysicsObject = pGib->VPhysicsInitNormal( SOLID_VPHYSICS, pGib->GetSolidFlags(), false );
				pGib->SetMoveType( MOVETYPE_VPHYSICS );

				if ( pPhysicsObject )
				{
					// Set gib velocity
					Vector vVel		= vecShootDir * flSpeed;
					pPhysicsObject->AddVelocity(&vVel, NULL);

					AngularImpulse torque;
					torque.x = m_flGibAngVelocity * random->RandomFloat( 0.1f, 1.0f );
					torque.y = m_flGibAngVelocity * random->RandomFloat( 0.1f, 1.0f );
					torque.z = 0.0f;
					torque *= pPhysicsObject->GetMass();

					pPhysicsObject->ApplyTorqueCenter( torque );

#ifndef HL1_DLL
					if( HasSpawnFlags( SF_SHOOTER_STRICT_REMOVE ) )
#endif
					{
						pGib->m_bForceRemove = true;
						pGib->SetNextThink( gpGlobals->curtime + pGib->m_lifeTime );
						pGib->SetThink ( &CGib::DieThink );
					}

				}
				else
				{
					InitPointGib( pGib, vecShootDir, flSpeed );
				}
			}
			return pGib;
		}

		case GIB_SIMULATE_POINT:
		{
			CGib *pGib = CreateGib();

			if ( pGib )
			{
				pGib->SetAbsAngles( m_angGibRotation );

				InitPointGib( pGib, vecShootDir, flSpeed );
				return pGib;
			}
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CGibShooter::ShootThink ( void )
{
	SetNextThink( gpGlobals->curtime + m_flDelay );

	Vector vecShootDir, vForward,vRight,vUp;
	AngleVectors( GetAbsAngles(), &vForward, &vRight, &vUp );
	vecShootDir = vForward;
	vecShootDir = vecShootDir + vRight * random->RandomFloat( -1, 1) * m_flVariance;
	vecShootDir = vecShootDir + vForward * random->RandomFloat( -1, 1) * m_flVariance;
	vecShootDir = vecShootDir + vUp * random->RandomFloat( -1, 1) * m_flVariance;

	VectorNormalize( vecShootDir );

	SpawnGib( vecShootDir, m_flGibVelocity );

	if ( --m_iGibs <= 0 )
	{
		if ( HasSpawnFlags(SF_GIBSHOOTER_REPEATABLE) )
		{
			m_iGibs = m_iGibCapacity;
			SetThink ( NULL );
			SetNextThink( gpGlobals->curtime );
		}
		else
		{
			SetThink ( &CGibShooter::SUB_Remove );
			SetNextThink( gpGlobals->curtime );
		}
	}
}

class CEnvShooter : public CGibShooter
{
public:
	DECLARE_CLASS( CEnvShooter, CGibShooter );

	CEnvShooter() { m_flGibGravityScale = 1.0f; }
	void		Precache( void );
	bool		KeyValue( const char *szKeyName, const char *szValue );

	CGib		*CreateGib( void );

	DECLARE_DATADESC();

public:

	int m_nSkin;
	float m_flGibScale;
	float m_flGibGravityScale;

#if HL2_EPISODIC
	float m_flMassOverride;	// allow designer to force a mass for gibs in some cases
#endif
};

BEGIN_DATADESC( CEnvShooter )

	DEFINE_KEYFIELD( m_nSkin, FIELD_INTEGER, "skin" ),
	DEFINE_KEYFIELD( m_flGibScale, FIELD_FLOAT ,"scale" ),
	DEFINE_KEYFIELD( m_flGibGravityScale, FIELD_FLOAT, "gibgravityscale" ),

#if HL2_EPISODIC
	DEFINE_KEYFIELD( m_flMassOverride, FIELD_FLOAT, "massoverride" ),
#endif

END_DATADESC()


LINK_ENTITY_TO_CLASS( env_shooter, CEnvShooter );

bool CEnvShooter::KeyValue( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "shootmodel"))
	{
		m_bIsSprite = false;
		SetModelName( AllocPooledString(szValue) );

		//Adrian - not pretty...
		if ( Q_stristr( szValue, ".vmt" ) )
			 m_bIsSprite = true;
	}

	else if (FStrEq(szKeyName, "shootsounds"))
	{
		int iNoise = atoi(szValue);
		switch( iNoise )
		{
		case 0:
			m_iGibMaterial = matGlass;
			break;
		case 1:
			m_iGibMaterial = matWood;
			break;
		case 2:
			m_iGibMaterial = matMetal;
			break;
		case 3:
			m_iGibMaterial = matFlesh;
			break;
		case 4:
			m_iGibMaterial = matRocks;
			break;

		default:
		case -1:
			m_iGibMaterial = matNone;
			break;
		}
	}
	else
	{
		return BaseClass::KeyValue( szKeyName, szValue );
	}

	return true;
}


void CEnvShooter::Precache ( void )
{
	m_iGibModelIndex = PrecacheModel( STRING( GetModelName() ) );
}


CGib *CEnvShooter::CreateGib ( void )
{
	CGib *pGib = CREATE_ENTITY( CGib, "gib" );

	if ( m_bIsSprite == true )
	{
		//HACK HACK
		pGib->Spawn( "" );
	}
	else
	{
		pGib->Spawn( STRING( GetModelName() ) );
	}

	int bodyPart = 0;

	if ( m_nMaxGibModelFrame > 1 )
	{
		bodyPart = random->RandomInt( 0, m_nMaxGibModelFrame-1 );
	}

	pGib->m_nBody = bodyPart;
	pGib->SetBloodColor( DONT_BLEED );
	pGib->m_material = m_iGibMaterial;

	pGib->m_nRenderMode = m_nRenderMode;
	pGib->m_clrRender = m_clrRender;
	pGib->m_nRenderFX = m_nRenderFX;
	pGib->m_nSkin = m_nSkin;
	pGib->m_lifeTime = gpGlobals->curtime + m_flGibLife;

	pGib->SetGravity( m_flGibGravityScale );

	// Spawn a flaming gib
	if ( HasSpawnFlags( SF_SHOOTER_FLAMING ) )
	{
		// Tag an entity flame along with us
		CEntityFlame *pFlame = CEntityFlame::Create( pGib, false );
		if ( pFlame != NULL )
		{
			pFlame->SetLifetime( pGib->m_lifeTime );
			pGib->SetFlame( pFlame );
		}
	}

	if ( m_iszLightingOrigin != NULL_STRING )
	{
		// Make the gibs use the lighting origin
		pGib->SetLightingOrigin( m_iszLightingOrigin );
	}

	if( m_bNoGibShadows )
	{
		pGib->AddEffects( EF_NOSHADOW );
	}

#if HL2_EPISODIC
	// if a mass override is set, apply it to the gib
	if (m_flMassOverride != 0)
	{
		IPhysicsObject *pPhys = pGib->VPhysicsGetObject();
		if (pPhys)
		{
			pPhys->SetMass( m_flMassOverride );
		}
	}
#endif

	return pGib;
}


//-----------------------------------------------------------------------------
// An entity that shoots out junk when hit by a rotor wash
//-----------------------------------------------------------------------------
class CRotorWashShooter : public CEnvShooter, public IRotorWashShooter
{
public:
	DECLARE_CLASS( CRotorWashShooter, CEnvShooter );
	DECLARE_DATADESC();

	virtual void Spawn();

public:
	// Inherited from IRotorWashShooter
	virtual CBaseEntity *DoWashPush( float flTimeSincePushStarted, const Vector &vecForce );

private:
	// Amount of time we need to spend under the rotor before we shoot
	float m_flTimeUnderRotor;
	float m_flTimeUnderRotorVariance;

	// Last time we were hit with a wash...
	float m_flLastWashStartTime;
	float m_flNextGibTime;
};


LINK_ENTITY_TO_CLASS( env_rotorshooter, CRotorWashShooter );


//-----------------------------------------------------------------------------
// Save/load
//-----------------------------------------------------------------------------
BEGIN_DATADESC( CRotorWashShooter )

	DEFINE_KEYFIELD( m_flTimeUnderRotor,	FIELD_FLOAT ,"rotortime" ),
	DEFINE_KEYFIELD( m_flTimeUnderRotorVariance,	FIELD_FLOAT ,"rotortimevariance" ),
	DEFINE_FIELD( m_flLastWashStartTime,	FIELD_TIME ),
	DEFINE_FIELD( m_flNextGibTime, FIELD_TIME ),

END_DATADESC()



//-----------------------------------------------------------------------------
// Gets at the interface if the entity supports it
//-----------------------------------------------------------------------------
IRotorWashShooter *GetRotorWashShooter( CBaseEntity *pEntity )
{
	CRotorWashShooter *pShooter = dynamic_cast<CRotorWashShooter*>(pEntity);
	return pShooter;
}


//-----------------------------------------------------------------------------
// Inherited from IRotorWashShooter
//-----------------------------------------------------------------------------
void CRotorWashShooter::Spawn()
{
	BaseClass::Spawn();
	m_flLastWashStartTime = -1;
}



//-----------------------------------------------------------------------------
// Inherited from IRotorWashShooter
//-----------------------------------------------------------------------------
CBaseEntity *CRotorWashShooter::DoWashPush( float flWashStartTime, const Vector &vecForce )
{
	if ( flWashStartTime == m_flLastWashStartTime )
	{
		if ( m_flNextGibTime > gpGlobals->curtime )
			return NULL;
	}

	m_flLastWashStartTime = flWashStartTime;
	m_flNextGibTime	= gpGlobals->curtime + m_flTimeUnderRotor + random->RandomFloat( -1, 1) * m_flTimeUnderRotorVariance;
	if ( m_flNextGibTime <= gpGlobals->curtime )
	{
		m_flNextGibTime = gpGlobals->curtime + 0.01f;
	}

	// Set the velocity to be what the force would cause it to accelerate to
	// after one tick
	Vector vecShootDir = vecForce;
	VectorNormalize( vecShootDir );

	vecShootDir.x += random->RandomFloat( -1, 1 ) * m_flVariance;
	vecShootDir.y += random->RandomFloat( -1, 1 ) * m_flVariance;
	vecShootDir.z += random->RandomFloat( -1, 1 ) * m_flVariance;

	VectorNormalize( vecShootDir );

	CBaseEntity *pGib = SpawnGib( vecShootDir, m_flGibVelocity /*flLength*/ );

	if ( --m_iGibs <= 0 )
	{
		if ( HasSpawnFlags(SF_GIBSHOOTER_REPEATABLE) )
		{
			m_iGibs = m_iGibCapacity;
		}
		else
		{
			SetThink ( &CGibShooter::SUB_Remove );
			SetNextThink( gpGlobals->curtime );
		}
	}

	return pGib;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CTestEffect : public CBaseEntity
{
public:
	DECLARE_CLASS( CTestEffect, CBaseEntity );

	void	Spawn( void );
	void	Precache( void );
	// bool	KeyValue( const char *szKeyName, const char *szValue );
	void Think( void );
	void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	int		m_iLoop;
	int		m_iBeam;
	CBeam	*m_pBeam[24];
	float	m_flBeamTime[24];
	float	m_flStartTime;
};


LINK_ENTITY_TO_CLASS( test_effect, CTestEffect );

void CTestEffect::Spawn( void )
{
	Precache( );
}

void CTestEffect::Precache( void )
{
	PrecacheModel( "sprites/lgtning.vmt" );
}

void CTestEffect::Think( void )
{
	int i;
	float t = (gpGlobals->curtime - m_flStartTime);

	if (m_iBeam < 24)
	{
		CBeam *pbeam = CBeam::BeamCreate( "sprites/lgtning.vmt", 10 );

		trace_t	tr;

		Vector vecSrc = GetAbsOrigin();
		Vector vecDir = Vector( random->RandomFloat( -1.0, 1.0 ), random->RandomFloat( -1.0, 1.0 ),random->RandomFloat( -1.0, 1.0 ) );
		VectorNormalize( vecDir );
		UTIL_TraceLine( vecSrc, vecSrc + vecDir * 128, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);

		pbeam->PointsInit( vecSrc, tr.endpos );
		// pbeam->SetColor( 80, 100, 255 );
		pbeam->SetColor( 255, 180, 100 );
		pbeam->SetWidth( 10.0 );
		pbeam->SetScrollRate( 12 );

		m_flBeamTime[m_iBeam] = gpGlobals->curtime;
		m_pBeam[m_iBeam] = pbeam;
		m_iBeam++;

#if 0
		Vector vecMid = (vecSrc + tr.endpos) * 0.5;
		CBroadcastRecipientFilter filter;
		TE_DynamicLight( filter, 0.0,
			vecMid, 255, 180, 100, 3, 2.0, 0.0 );
#endif
	}

	if (t < 3.0)
	{
		for (i = 0; i < m_iBeam; i++)
		{
			t = (gpGlobals->curtime - m_flBeamTime[i]) / ( 3 + m_flStartTime - m_flBeamTime[i]);
			m_pBeam[i]->SetBrightness( 255 * t );
			// m_pBeam[i]->SetScrollRate( 20 * t );
		}
		SetNextThink( gpGlobals->curtime + 0.1f );
	}
	else
	{
		for (i = 0; i < m_iBeam; i++)
		{
			UTIL_Remove( m_pBeam[i] );
		}
		m_flStartTime = gpGlobals->curtime;
		m_iBeam = 0;
		SetNextThink( TICK_NEVER_THINK );
	}
}


void CTestEffect::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	SetNextThink( gpGlobals->curtime + 0.1f );
	m_flStartTime = gpGlobals->curtime;
}



// Blood effects
class CBlood : public CPointEntity
{
public:
	DECLARE_CLASS( CBlood, CPointEntity );

	void	Spawn( void );
	bool	KeyValue( const char *szKeyName, const char *szValue );

	inline	int		Color( void ) { return m_Color; }
	inline	float 	BloodAmount( void ) { return m_flAmount; }

	inline	void SetColor( int color ) { m_Color = color; }

	// Input handlers
	void InputEmitBlood( inputdata_t &inputdata );

	Vector	Direction( void );
	Vector	BloodPosition( CBaseEntity *pActivator );

	DECLARE_DATADESC();

	Vector m_vecSprayDir;
	float m_flAmount;
	int m_Color;

private:
};

LINK_ENTITY_TO_CLASS( env_blood, CBlood );

BEGIN_DATADESC( CBlood )

	DEFINE_KEYFIELD( m_vecSprayDir, FIELD_VECTOR, "spraydir" ),
	DEFINE_KEYFIELD( m_flAmount, FIELD_FLOAT, "amount" ),
	DEFINE_FIELD( m_Color, FIELD_INTEGER ),

	DEFINE_INPUTFUNC( FIELD_VOID, "EmitBlood", InputEmitBlood ),

END_DATADESC()


#define SF_BLOOD_RANDOM		0x0001
#define SF_BLOOD_STREAM		0x0002
#define SF_BLOOD_PLAYER		0x0004
#define SF_BLOOD_DECAL		0x0008
#define SF_BLOOD_CLOUD		0x0010
#define SF_BLOOD_DROPS		0x0020
#define SF_BLOOD_GORE		0x0040


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBlood::Spawn( void )
{
	// Convert spraydir from angles to a vector
	QAngle angSprayDir = QAngle( m_vecSprayDir.x, m_vecSprayDir.y, m_vecSprayDir.z );
	AngleVectors( angSprayDir, &m_vecSprayDir );

	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
	SetColor( BLOOD_COLOR_RED );
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  : szKeyName -
//			szValue -
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBlood::KeyValue( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "color"))
	{
		int color = atoi(szValue);
		switch ( color )
		{
			case 1:
			{
				SetColor( BLOOD_COLOR_YELLOW );
				break;
			}
		}
	}
	else
	{
		return BaseClass::KeyValue( szKeyName, szValue );
	}

	return true;
}


Vector CBlood::Direction( void )
{
	if ( HasSpawnFlags( SF_BLOOD_RANDOM ) )
		return UTIL_RandomBloodVector();

	return m_vecSprayDir;
}


Vector CBlood::BloodPosition( CBaseEntity *pActivator )
{
	if ( HasSpawnFlags( SF_BLOOD_PLAYER ) )
	{
		CBasePlayer *player;

		if ( pActivator && pActivator->IsPlayer() )
		{
			player = ToBasePlayer( pActivator );
		}
		else
		{
			player = UTIL_GetLocalPlayer();
		}

		if ( player )
		{
			return (player->EyePosition()) + Vector( random->RandomFloat(-10,10), random->RandomFloat(-10,10), random->RandomFloat(-10,10) );
		}
	}

	return GetLocalOrigin();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void UTIL_BloodSpray( const Vector &pos, const Vector &dir, int color, int amount, int flags )
{
	if( color == DONT_BLEED )
		return;

	CEffectData	data;

	data.m_vOrigin = pos;
	data.m_vNormal = dir;
	data.m_flScale = (float)amount;
	data.m_fFlags = flags;
	data.m_nColor = color;

	DispatchEffect( "bloodspray", data );
}

//-----------------------------------------------------------------------------
// Purpose: Input handler for triggering the blood effect.
//-----------------------------------------------------------------------------
void CBlood::InputEmitBlood( inputdata_t &inputdata )
{
	if ( HasSpawnFlags( SF_BLOOD_STREAM ) )
	{
		UTIL_BloodStream( BloodPosition(inputdata.pActivator), Direction(), Color(), BloodAmount() );
	}
	else
	{
		UTIL_BloodDrips( BloodPosition(inputdata.pActivator), Direction(), Color(), BloodAmount() );
	}

	if ( HasSpawnFlags( SF_BLOOD_DECAL ) )
	{
		Vector forward = Direction();
		Vector start = BloodPosition( inputdata.pActivator );
		trace_t tr;

		UTIL_TraceLine( start, start + forward * BloodAmount() * 2, MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction != 1.0 )
		{
			UTIL_BloodDecalTrace( &tr, Color() );
		}
	}

	//
	// New-fangled blood effects.
	//
	if ( HasSpawnFlags( SF_BLOOD_CLOUD | SF_BLOOD_DROPS | SF_BLOOD_GORE ) )
	{
		int nFlags = 0;
		if (HasSpawnFlags(SF_BLOOD_CLOUD))
		{
			nFlags |= FX_BLOODSPRAY_CLOUD;
		}

		if (HasSpawnFlags(SF_BLOOD_DROPS))
		{
			nFlags |= FX_BLOODSPRAY_DROPS;
		}

		if (HasSpawnFlags(SF_BLOOD_GORE))
		{
			nFlags |= FX_BLOODSPRAY_GORE;
		}

		UTIL_BloodSpray(GetAbsOrigin(), Direction(), Color(), BloodAmount(), nFlags);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Console command for emitting the blood spray effect from an NPC.
//-----------------------------------------------------------------------------
void CC_BloodSpray( const CCommand &args )
{
	CBaseEntity *pEnt = NULL;
	while ( ( pEnt = gEntList.FindEntityGeneric( pEnt, args[1] ) ) != NULL )
	{
		Vector forward;
		pEnt->GetVectors(&forward, NULL, NULL);
		UTIL_BloodSpray( (forward * 4 ) + ( pEnt->EyePosition() + pEnt->WorldSpaceCenter() ) * 0.5f, forward, BLOOD_COLOR_RED, 4, FX_BLOODSPRAY_ALL );
	}
}

static ConCommand bloodspray( "bloodspray", CC_BloodSpray, "blood", FCVAR_CHEAT );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CEnvFunnel : public CBaseEntity
{
	DECLARE_DATADESC();
public:
	DECLARE_CLASS( CEnvFunnel, CBaseEntity );

	void	Spawn( void );
	void	Precache( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	int		m_iSprite;	// Don't save, precache
};

LINK_ENTITY_TO_CLASS( env_funnel, CEnvFunnel );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CEnvFunnel )

//	DEFINE_FIELD( m_iSprite,	FIELD_INTEGER ),

END_DATADESC()



void CEnvFunnel::Precache ( void )
{
	m_iSprite = PrecacheModel ( "sprites/flare6.vmt" );
}

void CEnvFunnel::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBroadcastRecipientFilter filter;
	te->LargeFunnel( filter, 0.0,
		&GetAbsOrigin(), m_iSprite, HasSpawnFlags( SF_FUNNEL_REVERSE ) ? 1 : 0 );

	SetThink( &CEnvFunnel::SUB_Remove );
	SetNextThink( gpGlobals->curtime );
}

void CEnvFunnel::Spawn( void )
{
	Precache();
	SetSolid( SOLID_NONE );
	AddEffects( EF_NODRAW );
}

//=========================================================
// Beverage Dispenser
// overloaded m_iHealth, is now how many cans remain in the machine.
//=========================================================
class CEnvBeverage : public CBaseEntity
{
public:
	DECLARE_CLASS( CEnvBeverage, CBaseEntity );

	void	Spawn( void );
	void	Precache( void );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	bool	KeyValue( const char *szKeyName, const char *szValue );

	// Input handlers.
	void	InputActivate( inputdata_t &inputdata );

	DECLARE_DATADESC();

public:
	bool	m_CanInDispenser;
	int		m_nBeverageType;
};

void CEnvBeverage::Precache ( void )
{
	PrecacheModel( "models/can.mdl" );
}

BEGIN_DATADESC( CEnvBeverage )
	DEFINE_FIELD( m_CanInDispenser, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_nBeverageType, FIELD_INTEGER ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Activate", InputActivate ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( env_beverage, CEnvBeverage );


bool CEnvBeverage::KeyValue( const char *szKeyName, const char *szValue )
{
	if (FStrEq(szKeyName, "beveragetype"))
	{
		m_nBeverageType = atoi(szValue);
	}
	else
	{
		return BaseClass::KeyValue( szKeyName, szValue );
	}

	return true;
}

void CEnvBeverage::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( m_CanInDispenser || m_iHealth <= 0 )
	{
		// no more cans while one is waiting in the dispenser, or if I'm out of cans.
		return;
	}

	CBaseAnimating *pCan = (CBaseAnimating *)CBaseEntity::Create( "item_sodacan", GetLocalOrigin(), GetLocalAngles(), this );

	if ( m_nBeverageType == 6 )
	{
		// random
		pCan->m_nSkin = random->RandomInt( 0, 5 );
	}
	else
	{
		pCan->m_nSkin = m_nBeverageType;
	}

	m_CanInDispenser = true;
	m_iHealth -= 1;

	//SetThink (SUB_Remove);
	//SetNextThink( gpGlobals->curtime );
}

void CEnvBeverage::InputActivate( inputdata_t &inputdata )
{
	Use( inputdata.pActivator, inputdata.pCaller, USE_ON, 0 );
}

void CEnvBeverage::Spawn( void )
{
	Precache();
	SetSolid( SOLID_NONE );
	AddEffects( EF_NODRAW );
	m_CanInDispenser = false;

	if ( m_iHealth == 0 )
	{
		m_iHealth = 10;
	}
}

//=========================================================
// Soda can
//=========================================================
class CItemSoda : public CBaseAnimating
{
public:
	DECLARE_CLASS( CItemSoda, CBaseAnimating );

	void	Spawn( void );
	void	Precache( void );
	void	CanThink ( void );
	void	CanTouch ( CBaseEntity *pOther );

	DECLARE_DATADESC();
};


BEGIN_DATADESC( CItemSoda )

	// Function Pointers
	DEFINE_FUNCTION( CanThink ),
	DEFINE_FUNCTION( CanTouch ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( item_sodacan, CItemSoda );

void CItemSoda::Precache ( void )
{
	PrecacheModel( "models/can.mdl" );

	PrecacheScriptSound( "ItemSoda.Bounce" );
}

void CItemSoda::Spawn( void )
{
	Precache();
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_FLYGRAVITY );

	SetModel ( "models/can.mdl" );
	UTIL_SetSize ( this, Vector ( 0, 0, 0 ), Vector ( 0, 0, 0 ) );

	SetThink (&CItemSoda::CanThink);
	SetNextThink( gpGlobals->curtime + 0.5f );
}

void CItemSoda::CanThink ( void )
{
	EmitSound( "ItemSoda.Bounce" );

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_TRIGGER );
	UTIL_SetSize ( this, Vector ( -8, -8, 0 ), Vector ( 8, 8, 8 ) );

	SetThink ( NULL );
	SetTouch ( &CItemSoda::CanTouch );
}

void CItemSoda::CanTouch ( CBaseEntity *pOther )
{
	if ( !pOther->IsPlayer() )
	{
		return;
	}

	// spoit sound here

	pOther->TakeHealth( 1, DMG_GENERIC );// a bit of health.

	if ( GetOwnerEntity() )
	{
		// tell the machine the can was taken
		CEnvBeverage *bev = (CEnvBeverage *)GetOwnerEntity();
		bev->m_CanInDispenser = false;
	}

	AddSolidFlags( FSOLID_NOT_SOLID );
	SetMoveType( MOVETYPE_NONE );
	AddEffects( EF_NODRAW );
	SetTouch ( NULL );
	SetThink ( &CItemSoda::SUB_Remove );
	SetNextThink( gpGlobals->curtime );
}

#ifndef _XBOX
//=========================================================
// func_precipitation - temporary snow solution for first HL2
// technology demo
//=========================================================

class CPrecipitation : public CBaseEntity
{
public:
	DECLARE_CLASS( CPrecipitation, CBaseEntity );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CPrecipitation();
	void	Spawn( void );

	CNetworkVar( PrecipitationType_t, m_nPrecipType );
};

LINK_ENTITY_TO_CLASS( func_precipitation, CPrecipitation );

BEGIN_DATADESC( CPrecipitation )
	DEFINE_KEYFIELD( m_nPrecipType, FIELD_INTEGER, "preciptype" ),
END_DATADESC()

// Just send the normal entity crap
IMPLEMENT_SERVERCLASS_ST( CPrecipitation, DT_Precipitation)
	SendPropInt( SENDINFO( m_nPrecipType ), Q_log2( NUM_PRECIPITATION_TYPES ) + 1, SPROP_UNSIGNED )
END_SEND_TABLE()


CPrecipitation::CPrecipitation()
{
	m_nPrecipType = PRECIPITATION_TYPE_RAIN; // default to rain.
}

void CPrecipitation::Spawn( void )
{
	PrecacheMaterial( "effects/fleck_ash1" );
	PrecacheMaterial( "effects/fleck_ash2" );
	PrecacheMaterial( "effects/fleck_ash3" );
	PrecacheMaterial( "effects/ember_swirling001" );

	Precache();
	SetSolid( SOLID_NONE );							// Remove model & collisions
	SetMoveType( MOVETYPE_NONE );
	SetModel( STRING( GetModelName() ) );		// Set size

	// Default to rain.
	if ( m_nPrecipType < 0 || m_nPrecipType > NUM_PRECIPITATION_TYPES )
		m_nPrecipType = PRECIPITATION_TYPE_RAIN;

	m_nRenderMode = kRenderEnvironmental;
}
#endif

//-----------------------------------------------------------------------------
// EnvWind - global wind info
//-----------------------------------------------------------------------------
class CEnvWind : public CBaseEntity
{
public:
	DECLARE_CLASS( CEnvWind, CBaseEntity );

	void	Spawn( void );
	void	Precache( void );
	void	WindThink( void );
	int		UpdateTransmitState( void );

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

private:
#ifdef POSIX
	CEnvWindShared m_EnvWindShared; // FIXME - fails to compile as networked var due to operator= problem
#else
	CNetworkVarEmbedded( CEnvWindShared, m_EnvWindShared );
#endif
};

LINK_ENTITY_TO_CLASS( env_wind, CEnvWind );

BEGIN_DATADESC( CEnvWind )

	DEFINE_KEYFIELD( m_EnvWindShared.m_iMinWind, FIELD_INTEGER, "minwind" ),
	DEFINE_KEYFIELD( m_EnvWindShared.m_iMaxWind, FIELD_INTEGER, "maxwind" ),
	DEFINE_KEYFIELD( m_EnvWindShared.m_iMinGust, FIELD_INTEGER, "mingust" ),
	DEFINE_KEYFIELD( m_EnvWindShared.m_iMaxGust, FIELD_INTEGER, "maxgust" ),
	DEFINE_KEYFIELD( m_EnvWindShared.m_flMinGustDelay, FIELD_FLOAT, "mingustdelay" ),
	DEFINE_KEYFIELD( m_EnvWindShared.m_flMaxGustDelay, FIELD_FLOAT, "maxgustdelay" ),
	DEFINE_KEYFIELD( m_EnvWindShared.m_iGustDirChange, FIELD_INTEGER, "gustdirchange" ),
	DEFINE_KEYFIELD( m_EnvWindShared.m_flGustDuration, FIELD_FLOAT, "gustduration" ),
//	DEFINE_KEYFIELD( m_EnvWindShared.m_iszGustSound, FIELD_STRING, "gustsound" ),

// Just here to quiet down classcheck
	// DEFINE_FIELD( m_EnvWindShared, CEnvWindShared ),

	DEFINE_FIELD( m_EnvWindShared.m_iWindDir, FIELD_INTEGER ),
	DEFINE_FIELD( m_EnvWindShared.m_flWindSpeed, FIELD_FLOAT ),

	DEFINE_OUTPUT( m_EnvWindShared.m_OnGustStart, "OnGustStart" ),
	DEFINE_OUTPUT( m_EnvWindShared.m_OnGustEnd,	"OnGustEnd" ),

	// Function Pointers
	DEFINE_FUNCTION( WindThink ),

END_DATADESC()


BEGIN_SEND_TABLE_NOBASE(CEnvWindShared, DT_EnvWindShared)
	// These are parameters that are used to generate the entire motion
	SendPropInt		(SENDINFO(m_iMinWind),		10, SPROP_UNSIGNED ),
	SendPropInt		(SENDINFO(m_iMaxWind),		10, SPROP_UNSIGNED ),
	SendPropInt		(SENDINFO(m_iMinGust),		10, SPROP_UNSIGNED ),
	SendPropInt		(SENDINFO(m_iMaxGust),		10, SPROP_UNSIGNED ),
	SendPropFloat	(SENDINFO(m_flMinGustDelay), 0, SPROP_NOSCALE),		// NOTE: Have to do this, so it's *exactly* the same on client
	SendPropFloat	(SENDINFO(m_flMaxGustDelay), 0, SPROP_NOSCALE),
	SendPropInt		(SENDINFO(m_iGustDirChange), 9, SPROP_UNSIGNED ),
	SendPropInt		(SENDINFO(m_iWindSeed),		32, SPROP_UNSIGNED ),

	// These are related to initial state
	SendPropInt		(SENDINFO(m_iInitialWindDir),9, SPROP_UNSIGNED ),
	SendPropFloat	(SENDINFO(m_flInitialWindSpeed),0, SPROP_NOSCALE ),
	SendPropFloat	(SENDINFO(m_flStartTime),	 0, SPROP_NOSCALE ),

	SendPropFloat	(SENDINFO(m_flGustDuration), 0, SPROP_NOSCALE),
	// Sound related
//	SendPropInt		(SENDINFO(m_iszGustSound),	10, SPROP_UNSIGNED ),
END_SEND_TABLE()

// This table encodes the CBaseEntity data.
IMPLEMENT_SERVERCLASS_ST_NOBASE(CEnvWind, DT_EnvWind)
	SendPropDataTable(SENDINFO_DT(m_EnvWindShared), &REFERENCE_SEND_TABLE(DT_EnvWindShared)),
END_SEND_TABLE()

void CEnvWind::Precache ( void )
{
//	if (m_iszGustSound)
//	{
//		PrecacheScriptSound( STRING( m_iszGustSound ) );
//	}
}

void CEnvWind::Spawn( void )
{
	Precache();
	SetSolid( SOLID_NONE );
	AddEffects( EF_NODRAW );

	m_EnvWindShared.Init( entindex(), 0, gpGlobals->frametime, GetLocalAngles().y, 0 );

	SetThink( &CEnvWind::WindThink );
	SetNextThink( gpGlobals->curtime );
}

int CEnvWind::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

void CEnvWind::WindThink( void )
{
	SetNextThink( m_EnvWindShared.WindThink( gpGlobals->curtime ) );
}



//==================================================
// CEmbers
//==================================================

#define	bitsSF_EMBERS_START_ON	0x00000001
#define	bitsSF_EMBERS_TOGGLE	0x00000002

// UNDONE: This is a brush effect-in-volume entity, move client side.
class CEmbers : public CBaseEntity
{
public:
	DECLARE_CLASS( CEmbers, CBaseEntity );

	void	Spawn( void );
	void	Precache( void );

	void	EmberUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	CNetworkVar( int, m_nDensity );
	CNetworkVar( int, m_nLifetime );
	CNetworkVar( int, m_nSpeed );

	CNetworkVar( bool, m_bEmit );

	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();
};

LINK_ENTITY_TO_CLASS( env_embers, CEmbers );

//Data description
BEGIN_DATADESC( CEmbers )

	DEFINE_KEYFIELD( m_nDensity,	FIELD_INTEGER, "density" ),
	DEFINE_KEYFIELD( m_nLifetime,	FIELD_INTEGER, "lifetime" ),
	DEFINE_KEYFIELD( m_nSpeed,		FIELD_INTEGER, "speed" ),

	DEFINE_FIELD( m_bEmit,	FIELD_BOOLEAN ),

	//Function pointers
	DEFINE_FUNCTION( EmberUse ),

END_DATADESC()


//Data table
IMPLEMENT_SERVERCLASS_ST( CEmbers, DT_Embers )
	SendPropInt(	SENDINFO( m_nDensity ),		32,	SPROP_UNSIGNED ),
	SendPropInt(	SENDINFO( m_nLifetime ),	32,	SPROP_UNSIGNED ),
	SendPropInt(	SENDINFO( m_nSpeed ),		32,	SPROP_UNSIGNED ),
	SendPropInt(	SENDINFO( m_bEmit ),		2,	SPROP_UNSIGNED ),
END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEmbers::Spawn( void )
{
	Precache();
	SetModel( STRING( GetModelName() ) );

	SetSolid( SOLID_NONE );
	SetRenderColorA( 0 );
	m_nRenderMode	= kRenderTransTexture;

	SetUse( &CEmbers::EmberUse );

	//Start off if we're targetted (unless flagged)
	m_bEmit = ( HasSpawnFlags( bitsSF_EMBERS_START_ON ) || ( !GetEntityName() ) );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEmbers::Precache( void )
{
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : *pActivator -
//			*pCaller -
//			useType -
//			value -
//-----------------------------------------------------------------------------
void CEmbers::EmberUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	//If we're not toggable, only allow one use
	if ( !HasSpawnFlags( bitsSF_EMBERS_TOGGLE ) )
	{
		SetUse( NULL );
	}

	//Handle it
	switch ( useType )
	{
	case USE_OFF:
		m_bEmit = false;
		break;

	case USE_ON:
		m_bEmit = true;
		break;

	case USE_SET:
		m_bEmit = !!(int)value;
		break;

	default:
	case USE_TOGGLE:
		m_bEmit = !m_bEmit;
		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CPhysicsWire : public CBaseEntity
{
public:
	DECLARE_CLASS( CPhysicsWire, CBaseEntity );

	void	Spawn( void );
	void	Precache( void );

	DECLARE_DATADESC();

protected:

	bool SetupPhysics( void );

	int		m_nDensity;
};

LINK_ENTITY_TO_CLASS( env_physwire, CPhysicsWire );

BEGIN_DATADESC( CPhysicsWire )

	DEFINE_KEYFIELD( m_nDensity,	FIELD_INTEGER, "Density" ),
//	DEFINE_KEYFIELD( m_frequency, FIELD_INTEGER, "frequency" ),

//	DEFINE_FIELD( m_flFoo, FIELD_FLOAT ),

	// Function Pointers
//	DEFINE_FUNCTION( WireThink ),

END_DATADESC()


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPhysicsWire::Spawn( void )
{
	BaseClass::Spawn();

	Precache();

//	if ( SetupPhysics() == false )
//		return;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CPhysicsWire::Precache( void )
{
	BaseClass::Precache();
}

class CPhysBallSocket;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CPhysicsWire::SetupPhysics( void )
{
/*
	CPointEntity	*anchorEnt, *freeEnt;
	CPhysBallSocket *socket;

	char	anchorName[256];
	char	freeName[256];

	int		iAnchorName, iFreeName;

	anchorEnt = (CPointEntity *) CreateEntityByName( "info_target" );

	if ( anchorEnt == NULL )
		return false;

	//Create and connect all segments
	for ( int i = 0; i < m_nDensity; i++ )
	{
		// Create other end of our link
		freeEnt = (CPointEntity *) CreateEntityByName( "info_target" );

		// Create a ballsocket and attach the two
		//socket = (CPhysBallSocket *) CreateEntityByName( "phys_ballsocket" );

		Q_snprintf( anchorName,sizeof(anchorName), "__PWIREANCHOR%d", i );
		Q_snprintf( freeName,sizeof(freeName), "__PWIREFREE%d", i+1 );

		iAnchorName = MAKE_STRING( anchorName );
		iFreeName	= MAKE_STRING( freeName );

		//Fake the names
		//socket->m_nameAttach1 = anchorEnt->m_iGlobalname	= iAnchorName;
		//socket->m_nameAttach2 = freeEnt->m_iGlobalname	= iFreeName

		//socket->Activate();

		//The free ent is now the anchor for the next link
		anchorEnt = freeEnt;
	}
*/

	return true;
}

//
// Muzzle flash
//

class CEnvMuzzleFlash : public CPointEntity
{
	DECLARE_CLASS( CEnvMuzzleFlash, CPointEntity );

public:
	virtual void Spawn();

	// Input handlers
	void	InputFire( inputdata_t &inputdata );

	DECLARE_DATADESC();

	float	m_flScale;
	string_t m_iszParentAttachment;
};

BEGIN_DATADESC( CEnvMuzzleFlash )

	DEFINE_KEYFIELD( m_flScale, FIELD_FLOAT, "scale" ),
	DEFINE_KEYFIELD( m_iszParentAttachment, FIELD_STRING, "parentattachment" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Fire", InputFire ),

END_DATADESC()


LINK_ENTITY_TO_CLASS( env_muzzleflash, CEnvMuzzleFlash );


//-----------------------------------------------------------------------------
// Spawn!
//-----------------------------------------------------------------------------
void CEnvMuzzleFlash::Spawn()
{
	if ( (m_iszParentAttachment != NULL_STRING) && GetParent() && GetParent()->GetBaseAnimating() )
	{
		CBaseAnimating *pAnim = GetParent()->GetBaseAnimating();
		int nParentAttachment = pAnim->LookupAttachment( STRING(m_iszParentAttachment) );
		if ( nParentAttachment > 0 )
		{
			SetParent( GetParent(), nParentAttachment );
			SetLocalOrigin( vec3_origin );
			SetLocalAngles( vec3_angle );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:
// Input  : &inputdata -
//-----------------------------------------------------------------------------
void CEnvMuzzleFlash::InputFire( inputdata_t &inputdata )
{
	g_pEffects->MuzzleFlash( GetAbsOrigin(), GetAbsAngles(), m_flScale, MUZZLEFLASH_TYPE_DEFAULT );
}


//=========================================================
// Splash!
//=========================================================
#define SF_ENVSPLASH_FINDWATERSURFACE	0x00000001
#define SF_ENVSPLASH_DIMINISH			0x00000002
class CEnvSplash : public CPointEntity
{
	DECLARE_CLASS( CEnvSplash, CPointEntity );

public:
	// Input handlers
	void	InputSplash( inputdata_t &inputdata );

protected:

	float	m_flScale;

	DECLARE_DATADESC();
};

BEGIN_DATADESC( CEnvSplash )
	DEFINE_KEYFIELD( m_flScale, FIELD_FLOAT, "scale" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Splash", InputSplash ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( env_splash, CEnvSplash );

//-----------------------------------------------------------------------------
// Purpose:
// Input  : &inputdata -
//-----------------------------------------------------------------------------
#define SPLASH_MAX_DEPTH	120.0f
void CEnvSplash::InputSplash( inputdata_t &inputdata )
{
	CEffectData	data;

	data.m_fFlags = 0;

	float scale = m_flScale;

	if( HasSpawnFlags( SF_ENVSPLASH_FINDWATERSURFACE ) )
	{
		if( UTIL_PointContents(GetAbsOrigin()) & MASK_WATER )
		{
			// No splash if I'm supposed to find the surface of the water, but I'm underwater.
			return;
		}

		// Trace down and find the water's surface. This is designed for making
		// splashes on the surface of water that can change water level.
		trace_t tr;
		UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() - Vector( 0, 0, 4096 ), (MASK_WATER|MASK_SOLID_BRUSHONLY), this, COLLISION_GROUP_NONE, &tr );
		data.m_vOrigin = tr.endpos;

		if ( tr.contents & CONTENTS_SLIME )
		{
			data.m_fFlags |= FX_WATER_IN_SLIME;
		}
	}
	else
	{
		data.m_vOrigin = GetAbsOrigin();
	}

	if( HasSpawnFlags( SF_ENVSPLASH_DIMINISH ) )
	{
		// Get smaller if I'm in deeper water.
		float depth = 0.0f;

		trace_t tr;
		UTIL_TraceLine( data.m_vOrigin, data.m_vOrigin - Vector( 0, 0, 4096 ), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );

		depth = fabs( tr.startpos.z - tr.endpos.z );

		float factor = 1.0f - (depth / SPLASH_MAX_DEPTH);

		if( factor < 0.1 )
		{
			// Don't bother making one this small.
			return;
		}

		scale *= factor;
	}

	data.m_vNormal = Vector( 0, 0, 1 );
	data.m_flScale = scale;

	DispatchEffect( "watersplash", data );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class CEnvGunfire : public CPointEntity
{
public:
	DECLARE_CLASS( CEnvGunfire, CPointEntity );

	CEnvGunfire()
	{
		// !!!HACKHACK
		// These fields came along kind of late, so they get
		// initialized in the constructor for now. (sjb)
		m_flBias = 1.0f;
		m_bCollide = false;
	}

	void Precache();
	void Spawn();
	void Activate();
	void StartShooting();
	void StopShooting();
	void ShootThink();
	void UpdateTarget();

	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );

	int	m_iMinBurstSize;
	int m_iMaxBurstSize;

	float m_flMinBurstDelay;
	float m_flMaxBurstDelay;

	float m_flRateOfFire;

	string_t	m_iszShootSound;
	string_t	m_iszTracerType;

	bool m_bDisabled;

	int	m_iShotsRemaining;

	int		m_iSpread;
	Vector	m_vecSpread;
	Vector	m_vecTargetPosition;
	float	m_flTargetDist;

	float	m_flBias;
	bool	m_bCollide;

	EHANDLE m_hTarget;


	DECLARE_DATADESC();
};

BEGIN_DATADESC( CEnvGunfire )
	DEFINE_KEYFIELD( m_iMinBurstSize, FIELD_INTEGER, "minburstsize" ),
	DEFINE_KEYFIELD( m_iMaxBurstSize, FIELD_INTEGER, "maxburstsize" ),
	DEFINE_KEYFIELD( m_flMinBurstDelay, FIELD_TIME, "minburstdelay" ),
	DEFINE_KEYFIELD( m_flMaxBurstDelay, FIELD_TIME, "maxburstdelay" ),
	DEFINE_KEYFIELD( m_flRateOfFire, FIELD_FLOAT, "rateoffire" ),
	DEFINE_KEYFIELD( m_iszShootSound, FIELD_STRING, "shootsound" ),
	DEFINE_KEYFIELD( m_iszTracerType, FIELD_STRING, "tracertype" ),
	DEFINE_KEYFIELD( m_bDisabled, FIELD_BOOLEAN, "startdisabled" ),
	DEFINE_KEYFIELD( m_iSpread, FIELD_INTEGER, "spread" ),
	DEFINE_KEYFIELD( m_flBias, FIELD_FLOAT, "bias" ),
	DEFINE_KEYFIELD( m_bCollide, FIELD_BOOLEAN, "collisions" ),

	DEFINE_FIELD( m_iShotsRemaining, FIELD_INTEGER ),
	DEFINE_FIELD( m_vecSpread, FIELD_VECTOR ),
	DEFINE_FIELD( m_vecTargetPosition, FIELD_VECTOR ),
	DEFINE_FIELD( m_flTargetDist, FIELD_FLOAT ),

	DEFINE_FIELD( m_hTarget, FIELD_EHANDLE ),

	DEFINE_THINKFUNC( ShootThink ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),
END_DATADESC()
LINK_ENTITY_TO_CLASS( env_gunfire, CEnvGunfire );

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvGunfire::Precache()
{
	PrecacheScriptSound( STRING( m_iszShootSound ) );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvGunfire::Spawn()
{
	Precache();

	m_iShotsRemaining = 0;
	m_flRateOfFire = 1.0f / m_flRateOfFire;

	switch( m_iSpread )
	{
	case 1:
		m_vecSpread = VECTOR_CONE_1DEGREES;
		break;
	case 5:
		m_vecSpread = VECTOR_CONE_5DEGREES;
		break;
	case 10:
		m_vecSpread = VECTOR_CONE_10DEGREES;
		break;
	case 15:
		m_vecSpread = VECTOR_CONE_15DEGREES;
		break;

	default:
		m_vecSpread = vec3_origin;
		break;
	}

	if( !m_bDisabled )
	{
		StartShooting();
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvGunfire::Activate( void )
{
	// Find my target
	if (m_target != NULL_STRING)
	{
		m_hTarget = gEntList.FindEntityByName( NULL, m_target );
	}

	BaseClass::Activate();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvGunfire::StartShooting()
{
	m_iShotsRemaining = random->RandomInt( m_iMinBurstSize, m_iMaxBurstSize );

	SetThink( &CEnvGunfire::ShootThink );
	SetNextThink( gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvGunfire::UpdateTarget()
{
	if( m_hTarget )
	{
		if( m_hTarget->WorldSpaceCenter() != m_vecTargetPosition )
		{
			// Target has moved.
			// Locate my target and cache the position and distance.
			m_vecTargetPosition = m_hTarget->WorldSpaceCenter();
			m_flTargetDist = (GetAbsOrigin() - m_vecTargetPosition).Length();
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvGunfire::StopShooting()
{
	SetThink( NULL );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvGunfire::ShootThink()
{
	if( !m_hTarget )
	{
		StopShooting();
	}

	SetNextThink( gpGlobals->curtime + m_flRateOfFire );

	UpdateTarget();

	Vector vecDir = m_vecTargetPosition - GetAbsOrigin();
	VectorNormalize( vecDir );

	CShotManipulator manipulator( vecDir );

	vecDir = manipulator.ApplySpread( m_vecSpread, m_flBias );

	Vector vecEnd;

	if( m_bCollide )
	{
		trace_t tr;

		UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + vecDir * 8192, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

		if( tr.fraction != 1.0 )
		{
			DoImpactEffect( tr, DMG_BULLET );
		}

		vecEnd = tr.endpos;
	}
	else
	{
		vecEnd = GetAbsOrigin() + vecDir * m_flTargetDist;
	}

	if( m_iszTracerType != NULL_STRING )
	{
		UTIL_Tracer( GetAbsOrigin(), vecEnd, 0, TRACER_DONT_USE_ATTACHMENT, 5000, true, STRING(m_iszTracerType) );
	}
	else
	{
		UTIL_Tracer( GetAbsOrigin(), vecEnd, 0, TRACER_DONT_USE_ATTACHMENT, 5000, true );
	}

	EmitSound( STRING(m_iszShootSound) );

	m_iShotsRemaining--;

	if( m_iShotsRemaining == 0 )
	{
		StartShooting();
		SetNextThink( gpGlobals->curtime + random->RandomFloat( m_flMinBurstDelay, m_flMaxBurstDelay ) );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvGunfire::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
	StartShooting();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvGunfire::InputDisable( inputdata_t &inputdata )
{
	m_bDisabled = true;
	SetThink( NULL );
}

//-----------------------------------------------------------------------------
// Quadratic spline beam effect
//-----------------------------------------------------------------------------
BEGIN_DATADESC( CEnvQuadraticBeam )
	DEFINE_FIELD( m_targetPosition, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_controlPosition, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_scrollRate, FIELD_FLOAT ),
	DEFINE_FIELD( m_flWidth, FIELD_FLOAT ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( env_quadraticbeam, CEnvQuadraticBeam );

IMPLEMENT_SERVERCLASS_ST( CEnvQuadraticBeam, DT_QuadraticBeam )
	SendPropVector(SENDINFO(m_targetPosition), -1, SPROP_COORD),
	SendPropVector(SENDINFO(m_controlPosition), -1, SPROP_COORD),
	SendPropFloat(SENDINFO(m_scrollRate), 8, 0, -4, 4),
	SendPropFloat(SENDINFO(m_flWidth), -1, SPROP_NOSCALE),
END_SEND_TABLE()

void CEnvQuadraticBeam::Spawn()
{
	BaseClass::Spawn();
	m_nRenderMode = kRenderTransAdd;
	SetRenderColor( 255, 255, 255 );
}

CEnvQuadraticBeam *CreateQuadraticBeam( const char *pSpriteName, const Vector &start, const Vector &control, const Vector &end, float width, CBaseEntity *pOwner )
{
	CEnvQuadraticBeam *pBeam = (CEnvQuadraticBeam *)CBaseEntity::Create( "env_quadraticbeam", start, vec3_angle, pOwner );
	UTIL_SetModel( pBeam, pSpriteName );
	pBeam->SetSpline( control, end );
	pBeam->SetScrollRate( 0.0 );
	pBeam->SetWidth(width);
	return pBeam;
}

void EffectsPrecache( void *pUser )
{
	CBaseEntity::PrecacheScriptSound( "Underwater.BulletImpact" );

	CBaseEntity::PrecacheScriptSound( "FX_RicochetSound.Ricochet" );

	CBaseEntity::PrecacheScriptSound( "Physics.WaterSplash" );
	CBaseEntity::PrecacheScriptSound( "BaseExplosionEffect.Sound" );
	CBaseEntity::PrecacheScriptSound( "Splash.SplashSound" );

	if ( gpGlobals->maxClients > 1 )
	{
		CBaseEntity::PrecacheScriptSound( "HudChat.Message" );
	}
}

PRECACHE_REGISTER_FN( EffectsPrecache );


class CEnvViewPunch : public CPointEntity
{
public:

	DECLARE_CLASS( CEnvViewPunch, CPointEntity );

	virtual void Spawn();

	// Input handlers
	void InputViewPunch( inputdata_t &inputdata );

private:

	float m_flRadius;
	QAngle m_angViewPunch;

	void DoViewPunch();

	DECLARE_DATADESC();
};

LINK_ENTITY_TO_CLASS( env_viewpunch, CEnvViewPunch );

BEGIN_DATADESC( CEnvViewPunch )

	DEFINE_KEYFIELD( m_angViewPunch, FIELD_VECTOR, "punchangle" ),
	DEFINE_KEYFIELD( m_flRadius, FIELD_FLOAT, "radius" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "ViewPunch", InputViewPunch ),

END_DATADESC()

#define SF_PUNCH_EVERYONE	0x0001		// Don't check radius
#define SF_PUNCH_IN_AIR		0x0002		// Punch players in air


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvViewPunch::Spawn( void )
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );

	if ( GetSpawnFlags() & SF_PUNCH_EVERYONE )
	{
		m_flRadius = 0;
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvViewPunch::DoViewPunch()
{
	bool bAir = (GetSpawnFlags() & SF_PUNCH_IN_AIR) ? true : false;
	UTIL_ViewPunch( GetAbsOrigin(), m_angViewPunch, m_flRadius, bAir );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CEnvViewPunch::InputViewPunch( inputdata_t &inputdata )
{
	DoViewPunch();
}

// HL1

#define	SF_GIBSHOOTER_REPEATABLE	1 // allows a gibshooter to be refired

#define SF_FUNNEL_REVERSE			1 // funnel effect repels particles instead of attracting them.


// Lightning target, just alias landmark
LINK_ENTITY_TO_CLASS( info_target, CPointEntity );


class CBubbling : public CBaseEntityHL1
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	
	void	EXPORT FizzThink( void );
	void	Use( CBaseEntityHL1 *pActivator, CBaseEntityHL1 *pCaller, USE_TYPE useType, float value );

	virtual int		SaveHL1( CSaveHL1 &save );
	virtual int		RestoreHL1( CRestoreHL1 &restore );
	virtual int		ObjectCaps( void ) { return CBaseEntityHL1::ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }
	static	TYPEDESCRIPTION m_SaveData[];

	int		m_density;
	int		m_frequency;
	int		m_bubbleModel;
	int		m_state;
};

LINK_ENTITY_TO_CLASS( env_bubbles, CBubbling );

TYPEDESCRIPTION	CBubbling::m_SaveData[] = 
{
	DEFINE_FIELD( CBubbling, m_density, FIELD_INTEGER ),
	DEFINE_FIELD( CBubbling, m_frequency, FIELD_INTEGER ),
	DEFINE_FIELD( CBubbling, m_state, FIELD_INTEGER ),
	// Let spawn restore this!
	//	DEFINE_FIELD( CBubbling, m_bubbleModel, FIELD_INTEGER ),
};

IMPLEMENT_SAVERESTORE( CBubbling, CBaseEntityHL1 );


#define SF_BUBBLES_STARTOFF		0x0001

void CBubbling::Spawn( void )
{
	Precache( );
	SET_MODEL( ENT(pev), STRING_HL1(pev->model) );		// Set size

	pev->solid = SOLID_NOT;							// Remove model & collisions
	pev->renderamt = 0;								// The engine won't draw this model if this is set to 0 and blending is on
	pev->rendermode = kRenderTransTexture;
	int speed = pev->speed > 0 ? pev->speed : -pev->speed;

	// HACKHACK!!! - Speed in rendercolor
	pev->rendercolor.x = speed >> 8;
	pev->rendercolor.y = speed & 255;
	pev->rendercolor.z = (pev->speed < 0) ? 1 : 0;


	if ( !(pev->spawnflags & SF_BUBBLES_STARTOFF) )
	{
		SetThinkHL1( FizzThink );
		pev->nextthink = gpGlobalsHL1->time + 2.0;
		m_state = 1;
	}
	else 
		m_state = 0;
}

void CBubbling::Precache( void )
{
	m_bubbleModel = PRECACHE_MODEL("sprites/bubble.spr");			// Precache bubble sprite
}


void CBubbling::Use( CBaseEntityHL1 *pActivator, CBaseEntityHL1 *pCaller, USE_TYPE useType, float value )
{
	if ( ShouldToggle( useType, m_state ) )
		m_state = !m_state;

	if ( m_state )
	{
		SetThinkHL1( FizzThink );
		pev->nextthink = gpGlobalsHL1->time + 0.1;
	}
	else
	{
		SetThinkHL1( NULL );
		pev->nextthink = 0;
	}
}


void CBubbling::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "density"))
	{
		m_density = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "frequency"))
	{
		m_frequency = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "current"))
	{
		pev->speed = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseEntityHL1::KeyValue( pkvd );
}


void CBubbling::FizzThink( void )
{
	MESSAGE_BEGIN( MSG_PAS, SVC_TEMPENTITY, VecBModelOrigin(pev) );
		WRITE_BYTE( TE_FIZZ );
		WRITE_SHORT( (short)ENTINDEX( edict() ) );
		WRITE_SHORT( (short)m_bubbleModel );
		WRITE_BYTE( m_density );
	MESSAGE_END();

	if ( m_frequency > 19 )
		pev->nextthink = gpGlobalsHL1->time + 0.5;
	else
		pev->nextthink = gpGlobalsHL1->time + 2.5 - (0.1 * m_frequency);
}

// --------------------------------------------------
// 
// Beams
//
// --------------------------------------------------

LINK_ENTITY_TO_CLASS( beam, CBeamHL1 );

void CBeamHL1::Spawn( void )
{
	pev->solid = SOLID_NOT;							// Remove model & collisions
	Precache( );
}

void CBeamHL1::Precache( void )
{
	if ( pev->owner )
		SetStartEntity( ENTINDEX( pev->owner ) );
	if ( pev->aiment )
		SetEndEntity( ENTINDEX( pev->aiment ) );
}

void CBeamHL1::SetStartEntity( int entityIndex ) 
{ 
	pev->sequence = (entityIndex & 0x0FFF) | ((pev->sequence&0xF000)<<12); 
	pev->owner = g_engfuncsHL1.pfnPEntityOfEntIndex( entityIndex );
}

void CBeamHL1::SetEndEntity( int entityIndex ) 
{ 
	pev->skin = (entityIndex & 0x0FFF) | ((pev->skin&0xF000)<<12); 
	pev->aiment = g_engfuncsHL1.pfnPEntityOfEntIndex( entityIndex );
}


// These don't take attachments into account
const VectorHL1 &CBeamHL1::GetStartPos( void )
{
	if ( GetType() == BEAM_ENTS )
	{
		edict_t_hl1 *pent =  g_engfuncsHL1.pfnPEntityOfEntIndex( GetStartEntity() );
		return pent->v.origin;
	}
	return pev->origin;
}


const VectorHL1 &CBeamHL1::GetEndPos( void )
{
	int type = GetType();
	if ( type == BEAM_POINTS || type == BEAM_HOSE )
	{
		return pev->angles;
	}

	edict_t_hl1 *pent =  g_engfuncsHL1.pfnPEntityOfEntIndex( GetEndEntity() );
	if ( pent )
		return pent->v.origin;
	return pev->angles;
}


CBeamHL1 *CBeamHL1::BeamCreate( const char *pSpriteName, int width )
{
	// Create a new entity with CBeam private data
	CBeamHL1 *pBeam = GetClassPtr( (CBeamHL1 *)NULL );
	pBeam->pev->classname = MAKE_STRING_HL1("beam");

	pBeam->BeamInit( pSpriteName, width );

	return pBeam;
}


void CBeamHL1::BeamInit( const char *pSpriteName, int width )
{
	pev->flags |= FL_CUSTOMENTITY;
	SetColor( 255, 255, 255 );
	SetBrightness( 255 );
	SetNoise( 0 );
	SetFrame( 0 );
	SetScrollRate( 0 );
	pev->model = MAKE_STRING_HL1( pSpriteName );
	SetTexture( PRECACHE_MODEL( (char *)pSpriteName ) );
	SetWidth( width );
	pev->skin = 0;
	pev->sequence = 0;
	pev->rendermode = 0;
}


void CBeamHL1::PointsInit( const VectorHL1 &start, const VectorHL1 &end )
{
	SetType( BEAM_POINTS );
	SetStartPos( start );
	SetEndPos( end );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}


void CBeamHL1::HoseInit( const VectorHL1 &start, const VectorHL1 &direction )
{
	SetType( BEAM_HOSE );
	SetStartPos( start );
	SetEndPos( direction );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}


void CBeamHL1::PointEntInit( const VectorHL1 &start, int endIndex )
{
	SetType( BEAM_ENTPOINT );
	SetStartPos( start );
	SetEndEntity( endIndex );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}

void CBeamHL1::EntsInit( int startIndex, int endIndex )
{
	SetType( BEAM_ENTS );
	SetStartEntity( startIndex );
	SetEndEntity( endIndex );
	SetStartAttachment( 0 );
	SetEndAttachment( 0 );
	RelinkBeam();
}


void CBeamHL1::RelinkBeam( void )
{
	const VectorHL1 &startPos = GetStartPos(), &endPos = GetEndPos();

	pev->mins.x = min( startPos.x, endPos.x );
	pev->mins.y = min( startPos.y, endPos.y );
	pev->mins.z = min( startPos.z, endPos.z );
	pev->maxs.x = max( startPos.x, endPos.x );
	pev->maxs.y = max( startPos.y, endPos.y );
	pev->maxs.z = max( startPos.z, endPos.z );
	pev->mins = pev->mins - pev->origin;
	pev->maxs = pev->maxs - pev->origin;

	UTIL_SetSizeHL1( pev, pev->mins, pev->maxs );
	UTIL_SetOrigin( pev, pev->origin );
}

#if 0
void CBeamHL1::SetObjectCollisionBox( void )
{
	const VectorHL1 &startPos = GetStartPos(), &endPos = GetEndPos();

	pev->absmin.x = min( startPos.x, endPos.x );
	pev->absmin.y = min( startPos.y, endPos.y );
	pev->absmin.z = min( startPos.z, endPos.z );
	pev->absmax.x = max( startPos.x, endPos.x );
	pev->absmax.y = max( startPos.y, endPos.y );
	pev->absmax.z = max( startPos.z, endPos.z );
}
#endif


void CBeamHL1::TriggerTouch( CBaseEntityHL1 *pOther )
{
	if ( pOther->pev->flags & (FL_CLIENT | FL_MONSTER) )
	{
		if ( pev->owner )
		{
			CBaseEntityHL1 *pOwner = CBaseEntityHL1::Instance(pev->owner);
			pOwner->Use( pOther, this, USE_TOGGLE, 0 );
		}
		ALERT( at_console, "Firing targets!!!\n" );
	}
}


CBaseEntityHL1 *CBeamHL1::RandomTargetname( const char *szName )
{
	int total = 0;

	CBaseEntityHL1 *pEntity = NULL;
	CBaseEntityHL1 *pNewEntity = NULL;
	while ((pNewEntity = UTIL_FindEntityByTargetname( pNewEntity, szName )) != NULL)
	{
		total++;
		if (RANDOM_LONG(0,total-1) < 1)
			pEntity = pNewEntity;
	}
	return pEntity;
}


void CBeamHL1::DoSparks( const VectorHL1 &start, const VectorHL1 &end )
{
	if ( pev->spawnflags & (SF_BEAM_SPARKSTART|SF_BEAM_SPARKEND) )
	{
		if ( pev->spawnflags & SF_BEAM_SPARKSTART )
		{
			UTIL_Sparks( start );
		}
		if ( pev->spawnflags & SF_BEAM_SPARKEND )
		{
			UTIL_Sparks( end );
		}
	}
}


class CLightning : public CBeamHL1
{
public:
	void	Spawn( void );
	void	Precache( void );
	void	KeyValue( KeyValueData *pkvd );
	void	Activate( void );

	void	EXPORT StrikeThink( void );
	void	EXPORT DamageThink( void );
	void	RandomArea( void );
	void	RandomPoint( VectorHL1 &vecSrc );
	void	Zap( const VectorHL1 &vecSrc, const VectorHL1 &vecDest );
	void	EXPORT StrikeUse( CBaseEntityHL1 *pActivator, CBaseEntityHL1 *pCaller, USE_TYPE useType, float value );
	void	EXPORT ToggleUse( CBaseEntityHL1 *pActivator, CBaseEntityHL1 *pCaller, USE_TYPE useType, float value );
	
	inline BOOL ServerSide( void )
	{
		if ( m_life == 0 && !(pev->spawnflags & SF_BEAM_RING) )
			return TRUE;
		return FALSE;
	}

	virtual int		SaveHL1( CSaveHL1 &save );
	virtual int		RestoreHL1( CRestoreHL1 &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	void	BeamUpdateVars( void );

	int		m_active;
	int		m_iszStartEntity;
	int		m_iszEndEntity;
	float	m_life;
	int		m_boltWidth;
	int		m_noiseAmplitude;
	int		m_brightness;
	int		m_speed;
	float	m_restrike;
	int		m_spriteTexture;
	int		m_iszSpriteName;
	int		m_frameStart;

	float	m_radius;
};

LINK_ENTITY_TO_CLASS( env_lightning, CLightning );
LINK_ENTITY_TO_CLASS( env_beam, CLightning );

// UNDONE: Jay -- This is only a test
#if _DEBUG
class CTripBeam : public CLightning
{
	void Spawn( void );
};
LINK_ENTITY_TO_CLASS( trip_beam, CTripBeam );

void CTripBeam::Spawn( void )
{
	CLightning::Spawn();
	SetTouch( TriggerTouch );
	pev->solid = SOLID_TRIGGER;
	RelinkBeam();
}
#endif



TYPEDESCRIPTION	CLightning::m_SaveData[] = 
{
	DEFINE_FIELD( CLightning, m_active, FIELD_INTEGER ),
	DEFINE_FIELD( CLightning, m_iszStartEntity, FIELD_STRING ),
	DEFINE_FIELD( CLightning, m_iszEndEntity, FIELD_STRING ),
	DEFINE_FIELD( CLightning, m_life, FIELD_FLOAT ),
	DEFINE_FIELD( CLightning, m_boltWidth, FIELD_INTEGER ),
	DEFINE_FIELD( CLightning, m_noiseAmplitude, FIELD_INTEGER ),
	DEFINE_FIELD( CLightning, m_brightness, FIELD_INTEGER ),
	DEFINE_FIELD( CLightning, m_speed, FIELD_INTEGER ),
	DEFINE_FIELD( CLightning, m_restrike, FIELD_FLOAT ),
	DEFINE_FIELD( CLightning, m_spriteTexture, FIELD_INTEGER ),
	DEFINE_FIELD( CLightning, m_iszSpriteName, FIELD_STRING ),
	DEFINE_FIELD( CLightning, m_frameStart, FIELD_INTEGER ),
	DEFINE_FIELD( CLightning, m_radius, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CLightning, CBeamHL1 );


void CLightning::Spawn( void )
{
	if ( FStringNull( m_iszSpriteName ) )
	{
		SetThinkHL1( SUB_Remove );
		return;
	}
	pev->solid = SOLID_NOT;							// Remove model & collisions
	Precache( );

	pev->dmgtime = gpGlobalsHL1->time;

	if ( ServerSide() )
	{
		SetThinkHL1( NULL );
		if ( pev->dmg > 0 )
		{
			SetThinkHL1( DamageThink );
			pev->nextthink = gpGlobalsHL1->time + 0.1;
		}
		if ( pev->targetname )
		{
			if ( !(pev->spawnflags & SF_BEAM_STARTON) )
			{
				pev->effects = EF_NODRAW;
				m_active = 0;
				pev->nextthink = 0;
			}
			else
				m_active = 1;
		
			SetUseHL1( ToggleUse );
		}
	}
	else
	{
		m_active = 0;
		if ( !FStringNull(pev->targetname) )
		{
			SetUseHL1( StrikeUse );
		}
		if ( FStringNull(pev->targetname) || FBitSet(pev->spawnflags, SF_BEAM_STARTON) )
		{
			SetThinkHL1( StrikeThink );
			pev->nextthink = gpGlobalsHL1->time + 1.0;
		}
	}
}

void CLightning::Precache( void )
{
	m_spriteTexture = PRECACHE_MODEL( (char *)STRING_HL1(m_iszSpriteName) );
	CBeamHL1::Precache();
}


void CLightning::Activate( void )
{
	if ( ServerSide() )
		BeamUpdateVars();
}


void CLightning::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "LightningStart"))
	{
		m_iszStartEntity = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "LightningEnd"))
	{
		m_iszEndEntity = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "life"))
	{
		m_life = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "BoltWidth"))
	{
		m_boltWidth = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "NoiseAmplitude"))
	{
		m_noiseAmplitude = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "TextureScroll"))
	{
		m_speed = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "StrikeTime"))
	{
		m_restrike = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "texture"))
	{
		m_iszSpriteName = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "framestart"))
	{
		m_frameStart = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "Radius"))
	{
		m_radius = atof( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "damage"))
	{
		pev->dmg = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBeamHL1::KeyValue( pkvd );
}


void CLightning::ToggleUse( CBaseEntityHL1 *pActivator, CBaseEntityHL1 *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_active ) )
		return;
	if ( m_active )
	{
		m_active = 0;
		pev->effects |= EF_NODRAW;
		pev->nextthink = 0;
	}
	else
	{
		m_active = 1;
		pev->effects &= ~EF_NODRAW;
		DoSparks( GetStartPos(), GetEndPos() );
		if ( pev->dmg > 0 )
		{
			pev->nextthink = gpGlobalsHL1->time;
			pev->dmgtime = gpGlobalsHL1->time;
		}
	}
}


void CLightning::StrikeUse( CBaseEntityHL1 *pActivator, CBaseEntityHL1 *pCaller, USE_TYPE useType, float value )
{
	if ( !ShouldToggle( useType, m_active ) )
		return;

	if ( m_active )
	{
		m_active = 0;
		SetThinkHL1( NULL );
	}
	else
	{
		SetThinkHL1( StrikeThink );
		pev->nextthink = gpGlobalsHL1->time + 0.1;
	}

	if ( !FBitSet( pev->spawnflags, SF_BEAM_TOGGLE ) )
		SetUseHL1( NULL );
}


int IsPointEntity( CBaseEntityHL1 *pEnt )
{
	if ( !pEnt->pev->modelindex )
		return 1;
	if ( FClassnameIs( pEnt->pev, "info_target" ) || FClassnameIs( pEnt->pev, "info_landmark" ) || FClassnameIs( pEnt->pev, "path_corner" ) )
		return 1;

	return 0;
}


void CLightning::StrikeThink( void )
{
	if ( m_life != 0 )
	{
		if ( pev->spawnflags & SF_BEAM_RANDOM )
			pev->nextthink = gpGlobalsHL1->time + m_life + RANDOM_FLOAT( 0, m_restrike );
		else
			pev->nextthink = gpGlobalsHL1->time + m_life + m_restrike;
	}
	m_active = 1;

	if (FStringNull(m_iszEndEntity))
	{
		if (FStringNull(m_iszStartEntity))
		{
			RandomArea( );
		}
		else
		{
			CBaseEntityHL1 *pStart = RandomTargetname( STRING_HL1(m_iszStartEntity) );
			if (pStart != NULL)
				RandomPoint( pStart->pev->origin );
			else
				ALERT( at_console, "env_beam: unknown entity \"%s\"\n", STRING_HL1(m_iszStartEntity) );
		}
		return;
	}

	CBaseEntityHL1 *pStart = RandomTargetname( STRING_HL1(m_iszStartEntity) );
	CBaseEntityHL1 *pEnd = RandomTargetname( STRING_HL1(m_iszEndEntity) );

	if ( pStart != NULL && pEnd != NULL )
	{
		if ( IsPointEntity( pStart ) || IsPointEntity( pEnd ) )
		{
			if ( pev->spawnflags & SF_BEAM_RING)
			{
				// don't work
				return;
			}
		}

		MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
			if ( IsPointEntity( pStart ) || IsPointEntity( pEnd ) )
			{
				if ( !IsPointEntity( pEnd ) )	// One point entity must be in pEnd
				{
					CBaseEntityHL1 *pTemp;
					pTemp = pStart;
					pStart = pEnd;
					pEnd = pTemp;
				}
				if ( !IsPointEntity( pStart ) )	// One sided
				{
					WRITE_BYTE( TE_BEAMENTPOINT );
					WRITE_SHORT( pStart->entindex() );
					WRITE_COORD( pEnd->pev->origin.x);
					WRITE_COORD( pEnd->pev->origin.y);
					WRITE_COORD( pEnd->pev->origin.z);
				}
				else
				{
					WRITE_BYTE( TE_BEAMPOINTS);
					WRITE_COORD( pStart->pev->origin.x);
					WRITE_COORD( pStart->pev->origin.y);
					WRITE_COORD( pStart->pev->origin.z);
					WRITE_COORD( pEnd->pev->origin.x);
					WRITE_COORD( pEnd->pev->origin.y);
					WRITE_COORD( pEnd->pev->origin.z);
				}


			}
			else
			{
				if ( pev->spawnflags & SF_BEAM_RING)
					WRITE_BYTE( TE_BEAMRING );
				else
					WRITE_BYTE( TE_BEAMENTS );
				WRITE_SHORT( pStart->entindex() );
				WRITE_SHORT( pEnd->entindex() );
			}

			WRITE_SHORT( m_spriteTexture );
			WRITE_BYTE( m_frameStart ); // framestart
			WRITE_BYTE( (int)pev->framerate); // framerate
			WRITE_BYTE( (int)(m_life*10.0) ); // life
			WRITE_BYTE( m_boltWidth );  // width
			WRITE_BYTE( m_noiseAmplitude );   // noise
			WRITE_BYTE( (int)pev->rendercolor.x );   // r, g, b
			WRITE_BYTE( (int)pev->rendercolor.y );   // r, g, b
			WRITE_BYTE( (int)pev->rendercolor.z );   // r, g, b
			WRITE_BYTE( pev->renderamt );	// brightness
			WRITE_BYTE( m_speed );		// speed
		MESSAGE_END();
		DoSparks( pStart->pev->origin, pEnd->pev->origin );
		if ( pev->dmg > 0 )
		{
			TraceResult tr;
			UTIL_TraceLine( pStart->pev->origin, pEnd->pev->origin, dont_ignore_monsters, NULL, &tr );
			BeamDamageInstant( &tr, pev->dmg );
		}
	}
}


void CBeamHL1::BeamDamage( TraceResult *ptr )
{
	RelinkBeam();
	if ( ptr->flFraction != 1.0 && ptr->pHit != NULL )
	{
		CBaseEntityHL1 *pHit = CBaseEntityHL1::Instance(ptr->pHit);
		if ( pHit )
		{
			ClearMultiDamage();
			pHit->TraceAttack( pev, pev->dmg * (gpGlobalsHL1->time - pev->dmgtime), (ptr->vecEndPos - pev->origin).Normalize(), ptr, DMG_ENERGYBEAM );
			ApplyMultiDamage( pev, pev );
			if ( pev->spawnflags & SF_BEAM_DECALS )
			{
				if ( pHit->IsBSPModel() )
					UTIL_DecalTrace( ptr, DECAL_BIGSHOT1 + RANDOM_LONG(0,4) );
			}
		}
	}
	pev->dmgtime = gpGlobals->time;
}


void CLightning::DamageThink( void )
{
	pev->nextthink = gpGlobalsHL1->time + 0.1;
	TraceResult tr;
	UTIL_TraceLine( GetStartPos(), GetEndPos(), dont_ignore_monsters, NULL, &tr );
	BeamDamage( &tr );
}



void CLightning::Zap( const VectorHL1 &vecSrc, const VectorHL1 &vecDest )
{
#if 1
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE( TE_BEAMPOINTS);
		WRITE_COORD(vecSrc.x);
		WRITE_COORD(vecSrc.y);
		WRITE_COORD(vecSrc.z);
		WRITE_COORD(vecDest.x);
		WRITE_COORD(vecDest.y);
		WRITE_COORD(vecDest.z);
		WRITE_SHORT( m_spriteTexture );
		WRITE_BYTE( m_frameStart ); // framestart
		WRITE_BYTE( (int)pev->framerate); // framerate
		WRITE_BYTE( (int)(m_life*10.0) ); // life
		WRITE_BYTE( m_boltWidth );  // width
		WRITE_BYTE( m_noiseAmplitude );   // noise
		WRITE_BYTE( (int)pev->rendercolor.x );   // r, g, b
		WRITE_BYTE( (int)pev->rendercolor.y );   // r, g, b
		WRITE_BYTE( (int)pev->rendercolor.z );   // r, g, b
		WRITE_BYTE( pev->renderamt );	// brightness
		WRITE_BYTE( m_speed );		// speed
	MESSAGE_END();
#else
	MESSAGE_BEGIN( MSG_BROADCAST, SVC_TEMPENTITY );
		WRITE_BYTE(TE_LIGHTNING);
		WRITE_COORD(vecSrc.x);
		WRITE_COORD(vecSrc.y);
		WRITE_COORD(vecSrc.z);
		WRITE_COORD(vecDest.x);
		WRITE_COORD(vecDest.y);
		WRITE_COORD(vecDest.z);
		WRITE_BYTE(10);
		WRITE_BYTE(50);
		WRITE_BYTE(40);
		WRITE_SHORT(m_spriteTexture);
	MESSAGE_END();
#endif
	DoSparks( vecSrc, vecDest );
}

void CLightning::RandomArea( void )
{
	int iLoops = 0;

	for (iLoops = 0; iLoops < 10; iLoops++)
	{
		VectorHL1 vecSrc = pev->origin;

		VectorHL1 vecDir1 = VectorHL1( RANDOM_FLOAT( -1.0, 1.0 ), RANDOM_FLOAT( -1.0, 1.0 ),RANDOM_FLOAT( -1.0, 1.0 ) );
		vecDir1 = vecDir1.Normalize();
		TraceResult		tr1;
		UTIL_TraceLine( vecSrc, vecSrc + vecDir1 * m_radius, ignore_monsters, ENT(pev), &tr1 );

		if (tr1.flFraction == 1.0)
			continue;

		VectorHL1 vecDir2;
		do {
			vecDir2 = VectorHL1( RANDOM_FLOAT( -1.0, 1.0 ), RANDOM_FLOAT( -1.0, 1.0 ),RANDOM_FLOAT( -1.0, 1.0 ) );
		} while (DotProduct(vecDir1, vecDir2 ) > 0);
		vecDir2 = vecDir2.Normalize();
		TraceResult		tr2;
		UTIL_TraceLine( vecSrc, vecSrc + vecDir2 * m_radius, ignore_monsters, ENT(pev), &tr2 );

		if (tr2.flFraction == 1.0)
			continue;

		if ((tr1.vecEndPos - tr2.vecEndPos).Length() < m_radius * 0.1)
			continue;

		UTIL_TraceLine( tr1.vecEndPos, tr2.vecEndPos, ignore_monsters, ENT(pev), &tr2 );

		if (tr2.flFraction != 1.0)
			continue;

		Zap( tr1.vecEndPos, tr2.vecEndPos );

		break;
	}
}


void CLightning::RandomPoint( VectorHL1 &vecSrc )
{
	int iLoops = 0;

	for (iLoops = 0; iLoops < 10; iLoops++)
	{
		VectorHL1 vecDir1 = VectorHL1( RANDOM_FLOAT( -1.0, 1.0 ), RANDOM_FLOAT( -1.0, 1.0 ),RANDOM_FLOAT( -1.0, 1.0 ) );
		vecDir1 = vecDir1.Normalize();
		TraceResult		tr1;
		UTIL_TraceLine( vecSrc, vecSrc + vecDir1 * m_radius, ignore_monsters, ENT(pev), &tr1 );

		if ((tr1.vecEndPos - vecSrc).Length() < m_radius * 0.1)
			continue;

		if (tr1.flFraction == 1.0)
			continue;

		Zap( vecSrc, tr1.vecEndPos );
		break;
	}
}



void CLightning::BeamUpdateVars( void )
{
	int beamType;
	int pointStart, pointEnd;

	edict_t_hl1 *pStart = FIND_ENTITY_BY_TARGETNAME ( NULL, STRING_HL1(m_iszStartEntity) );
	edict_t_hl1 *pEnd = FIND_ENTITY_BY_TARGETNAME ( NULL, STRING_HL1(m_iszEndEntity) );
	pointStart = IsPointEntity( CBaseEntityHL1::Instance(pStart) );
	pointEnd = IsPointEntity( CBaseEntityHL1::Instance(pEnd) );

	pev->skin = 0;
	pev->sequence = 0;
	pev->rendermode = 0;
	pev->flags |= FL_CUSTOMENTITY;
	pev->model = m_iszSpriteName;
	SetTexture( m_spriteTexture );

	beamType = BEAM_ENTS;
	if ( pointStart || pointEnd )
	{
		if ( !pointStart )	// One point entity must be in pStart
		{
			edict_t_hl1 *pTemp;
			// Swap start & end
			pTemp = pStart;
			pStart = pEnd;
			pEnd = pTemp;
			int swap = pointStart;
			pointStart = pointEnd;
			pointEnd = swap;
		}
		if ( !pointEnd )
			beamType = BEAM_ENTPOINT;
		else
			beamType = BEAM_POINTS;
	}

	SetType( beamType );
	if ( beamType == BEAM_POINTS || beamType == BEAM_ENTPOINT || beamType == BEAM_HOSE )
	{
		SetStartPos( pStart->v.origin );
		if ( beamType == BEAM_POINTS || beamType == BEAM_HOSE )
			SetEndPos( pEnd->v.origin );
		else
			SetEndEntity( ENTINDEX(pEnd) );
	}
	else
	{
		SetStartEntity( ENTINDEX(pStart) );
		SetEndEntity( ENTINDEX(pEnd) );
	}

	RelinkBeam();

	SetWidth( m_boltWidth );
	SetNoise( m_noiseAmplitude );
	SetFrame( m_frameStart );
	SetScrollRate( m_speed );
	if ( pev->spawnflags & SF_BEAM_SHADEIN )
		SetFlags( BEAM_FSHADEIN );
	else if ( pev->spawnflags & SF_BEAM_SHADEOUT )
		SetFlags( BEAM_FSHADEOUT );
}


LINK_ENTITY_TO_CLASS( env_laser, CLaserHL1 );

TYPEDESCRIPTION	CLaserHL1::m_SaveData[] = 
{
	DEFINE_FIELD( CLaserHL1, m_pSprite, FIELD_CLASSPTR ),
	DEFINE_FIELD( CLaserHL1, m_iszSpriteName, FIELD_STRING ),
	DEFINE_FIELD( CLaserHL1, m_firePosition, FIELD_POSITION_VECTOR ),
};

IMPLEMENT_SAVERESTORE( CLaserHL1, CBeamHL1 );

void CLaserHL1::Spawn( void )
{
	if ( FStringNull( pev->model ) )
	{
		SetThinkHL1( SUB_Remove );
		return;
	}
	pev->solid = SOLID_NOT;							// Remove model & collisions
	Precache( );

	SetThinkHL1( StrikeThink );
	pev->flags |= FL_CUSTOMENTITY;

	PointsInit( pev->origin, pev->origin );

	if ( !m_pSprite && m_iszSpriteName )
		m_pSprite = CSpriteHL1::SpriteCreate( STRING_HL1(m_iszSpriteName), pev->origin, TRUE );
	else
		m_pSprite = NULL;

	if ( m_pSprite )
		m_pSprite->SetTransparency( kRenderGlow, pev->rendercolor.x, pev->rendercolor.y, pev->rendercolor.z, pev->renderamt, pev->renderfx );

	if ( pev->targetname && !(pev->spawnflags & SF_BEAM_STARTON) )
		TurnOff();
	else
		TurnOn();
}

void CLaserHL1::Precache( void )
{
	pev->modelindex = PRECACHE_MODEL( (char *)STRING_HL1(pev->model) );
	if ( m_iszSpriteName )
		PRECACHE_MODEL( (char *)STRING_HL1(m_iszSpriteName) );
}


void CLaserHL1::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "LaserTarget"))
	{
		pev->message = ALLOC_STRING( pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "width"))
	{
		SetWidth( atof(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "NoiseAmplitude"))
	{
		SetNoise( atoi(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "TextureScroll"))
	{
		SetScrollRate( atoi(pkvd->szValue) );
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "texture"))
	{
		pev->model = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "EndSprite"))
	{
		m_iszSpriteName = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "framestart"))
	{
		pev->frame = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "damage"))
	{
		pev->dmg = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBeamHL1::KeyValue( pkvd );
}


int CLaserHL1::IsOn( void )
{
	if (pev->effects & EF_NODRAW)
		return 0;
	return 1;
}


void CLaserHL1::TurnOff( void )
{
	pev->effects |= EF_NODRAW;
	pev->nextthink = 0;
	if ( m_pSprite )
		m_pSprite->TurnOff();
}


void CLaserHL1::TurnOn( void )
{
	pev->effects &= ~EF_NODRAW;
	if ( m_pSprite )
		m_pSprite->TurnOn();
	pev->dmgtime = gpGlobalsHL1->time;
	pev->nextthink = gpGlobalsHL1->time;
}


void CLaserHL1::Use( CBaseEntityHL1 *pActivator, CBaseEntityHL1 *pCaller, USE_TYPE useType, float value )
{
	int active = IsOn();

	if ( !ShouldToggle( useType, active ) )
		return;
	if ( active )
	{
		TurnOff();
	}
	else
	{
		TurnOn();
	}
}


void CLaserHL1::FireAtPoint( TraceResult &tr )
{
	SetEndPos( tr.vecEndPos );
	if ( m_pSprite )
		UTIL_SetOrigin( m_pSprite->pev, tr.vecEndPos );

	BeamDamage( &tr );
	DoSparks( GetStartPos(), tr.vecEndPos );
}

void CLaserHL1::StrikeThink( void )
{
	CBaseEntityHL1 *pEnd = RandomTargetname( STRING_HL1(pev->message) );

	if ( pEnd )
		m_firePosition = pEnd->pev->origin;

	TraceResult tr;

	UTIL_TraceLine( pev->origin, m_firePosition, dont_ignore_monsters, NULL, &tr );
	FireAtPoint( tr );
	pev->nextthink = gpGlobals->time + 0.1;
}



class CGlow : public CPointEntity
{
public:
	void Spawn( void );
	void Think( void );
	void Animate( float frames );
	virtual int		SaveHL1( CSaveHL1 &save );
	virtual int		RestoreHL1( CRestoreHL1 &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	float		m_lastTime;
	float		m_maxFrame;
};

LINK_ENTITY_TO_CLASS( env_glow, CGlow );

TYPEDESCRIPTION	CGlow::m_SaveData[] = 
{
	DEFINE_FIELD( CGlow, m_lastTime, FIELD_TIME ),
	DEFINE_FIELD( CGlow, m_maxFrame, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CGlow, CPointEntity );

void CGlow::Spawn( void )
{
	pev->solid			= SOLID_NOT;
	pev->movetype		= MOVETYPE_NONE;
	pev->effects		= 0;
	pev->frame			= 0;

	PRECACHE_MODEL( (char *)STRING_HL1(pev->model) );
	SET_MODEL( ENT(pev), STRING_HL1(pev->model) );

	m_maxFrame = (float) MODEL_FRAMES( pev->modelindex ) - 1;
	if ( m_maxFrame > 1.0 && pev->framerate != 0 )
		pev->nextthink	= gpGlobalsHL1->time + 0.1;

	m_lastTime = gpGlobalsHL1->time;
}


void CGlow::Think( void )
{
	Animate( pev->framerate * (gpGlobalsHL1->time - m_lastTime) );

	pev->nextthink		= gpGlobalsHL1->time + 0.1;
	m_lastTime			= gpGlobalsHL1->time;
}


void CGlow::Animate( float frames )
{ 
	if ( m_maxFrame > 0 )
		pev->frame = fmod( pev->frame + frames, m_maxFrame );
}


LINK_ENTITY_TO_CLASS( env_sprite, CSprite );

TYPEDESCRIPTION	CSpriteHL1::m_SaveData[] = 
{
	DEFINE_FIELD( CSpriteHL1, m_lastTime, FIELD_TIME ),
	DEFINE_FIELD( CSpriteHL1, m_maxFrame, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CSpriteHL1, CPointEntity );

void CSpriteHL1::Spawn( void )
{
	pev->solid			= SOLID_NOT;
	pev->movetype		= MOVETYPE_NONE;
	pev->effects		= 0;
	pev->frame			= 0;

	Precache();
	SET_MODEL( ENT(pev), STRING_HL1(pev->model) );

	m_maxFrame = (float) MODEL_FRAMES( pev->modelindex ) - 1;
	if ( pev->targetname && !(pev->spawnflags & SF_SPRITE_STARTON) )
		TurnOff();
	else
		TurnOn();
	
	// Worldcraft only sets y rotation, copy to Z
	if ( pev->angles.y != 0 && pev->angles.z == 0 )
	{
		pev->angles.z = pev->angles.y;
		pev->angles.y = 0;
	}
}


void CSpriteHL1::Precache( void )
{
	PRECACHE_MODEL( (char *)STRING_HL1(pev->model) );

	// Reset attachment after save/restore
	if ( pev->aiment )
		SetAttachment( pev->aiment, pev->body );
	else
	{
		// Clear attachment
		pev->skin = 0;
		pev->body = 0;
	}
}


void CSpriteHL1::SpriteInit( const char *pSpriteName, const VectorHL1 &origin )
{
	pev->model = MAKE_STRING_HL1(pSpriteName);
	pev->origin = origin;
	Spawn();
}

CSpriteHL1 *CSpriteHL1::SpriteCreate( const char *pSpriteName, const VectorHL1 &origin, BOOL animate )
{
	CSpriteHL1 *pSprite = GetClassPtr( (CSpriteHL1 *)NULL );
	pSprite->SpriteInit( pSpriteName, origin );
	pSprite->pev->classname = MAKE_STRING_HL1("env_sprite");
	pSprite->pev->solid = SOLID_NOT;
	pSprite->pev->movetype = MOVETYPE_NOCLIP;
	if ( animate )
		pSprite->TurnOn();

	return pSprite;
}


void CSpriteHL1::AnimateThink( void )
{
	Animate( pev->framerate * (gpGlobalsHL1->time - m_lastTime) );

	pev->nextthink		= gpGlobalsHL1->time + 0.1;
	m_lastTime			= gpGlobalsHL1->time;
}

void CSpriteHL1::AnimateUntilDead( void )
{
	if ( gpGlobalsHL1->time > pev->dmgtime )
		UTIL_Remove(this);
	else
	{
		AnimateThink();
		pev->nextthink = gpGlobalsHL1->time;
	}
}

void CSpriteHL1::Expand( float scaleSpeed, float fadeSpeed )
{
	pev->speed = scaleSpeed;
	pev->health = fadeSpeed;
	SetThinkHL1( ExpandThink );

	pev->nextthink	= gpGlobalsHL1->time;
	m_lastTime		= gpGlobalsHL1->time;
}


void CSpriteHL1::ExpandThink( void )
{
	float frametime = gpGlobalsHL1->time - m_lastTime;
	pev->scale += pev->speed * frametime;
	pev->renderamt -= pev->health * frametime;
	if ( pev->renderamt <= 0 )
	{
		pev->renderamt = 0;
		UTIL_Remove( this );
	}
	else
	{
		pev->nextthink		= gpGlobalsHL1->time + 0.1;
		m_lastTime			= gpGlobalsHL1->time;
	}
}


void CSpriteHL1::Animate( float frames )
{ 
	pev->frame += frames;
	if ( pev->frame > m_maxFrame )
	{
		if ( pev->spawnflags & SF_SPRITE_ONCE )
		{
			TurnOff();
		}
		else
		{
			if ( m_maxFrame > 0 )
				pev->frame = fmod( pev->frame, m_maxFrame );
		}
	}
}


void CSpriteHL1::TurnOff( void )
{
	pev->effects = EF_NODRAW;
	pev->nextthink = 0;
}


void CSpriteHL1::TurnOn( void )
{
	pev->effects = 0;
	if ( (pev->framerate && m_maxFrame > 1.0) || (pev->spawnflags & SF_SPRITE_ONCE) )
	{
		SetThinkHL1( AnimateThink );
		pev->nextthink = gpGlobals->time;
		m_lastTime = gpGlobals->time;
	}
	pev->frame = 0;
}


void CSpriteHL1::Use( CBaseEntityHL1 *pActivator, CBaseEntityHL1 *pCaller, USE_TYPE useType, float value )
{
	int on = pev->effects != EF_NODRAW;
	if ( ShouldToggle( useType, on ) )
	{
		if ( on )
		{
			TurnOff();
		}
		else
		{
			TurnOn();
		}
	}
}
