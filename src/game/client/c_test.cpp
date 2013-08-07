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
class C_Test : public C_AI_BaseNPC
{
public:
	DECLARE_CLASS( C_Test, C_AI_BaseNPC );
	DECLARE_CLIENTCLASS();

					C_Test();
	virtual			~C_Test();

private:
	C_Test( const C_Test & ); // not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT(C_Test, DT_NPC_Test, CNPC_Test)
END_RECV_TABLE()

C_Test::C_Test()
{
}


C_Test::~C_Test()
{
}


