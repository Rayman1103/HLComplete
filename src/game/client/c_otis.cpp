//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_ai_basenpc.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_Otis : public C_AI_BaseNPC
{
public:
	DECLARE_CLASS( C_Otis, C_AI_BaseNPC );
	DECLARE_CLIENTCLASS();

					C_Otis();
	virtual			~C_Otis();

private:
	C_Otis( const C_Otis & ); // not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT(C_Otis, DT_NPC_Monster_Otis, CNPC_Monster_Otis)
END_RECV_TABLE()

C_Otis::C_Otis()
{
}


C_Otis::~C_Otis()
{
}


