//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
/*
Entity Data Descriptions

Each entity has an array which defines it's data in way that is useful for
entity communication, parsing initial values from the map, and save/restore.

each entity has to have the following line in it's class declaration:

	DECLARE_DATADESC();

this line defines that it has an m_DataDesc[] array, and declares functions through
which various subsystems can iterate the data.

In it's implementation, each entity has to have:

	typedescription_t CBaseEntity::m_DataDesc[] = { ... }
	
in which all it's data is defined (see below), followed by:


which implements the functions necessary for iterating through an entities data desc.

There are several types of data:
	
	FIELD		: this is a variable which gets saved & loaded from disk
	KEY			: this variable can be read in from the map file
	GLOBAL		: a global field is actually local; it is saved/restored, but is actually
				  unique to the entity on each level.
	CUSTOM		: the save/restore parsing functions are described by the user.
	ARRAY		: an array of values
	OUTPUT		: a variable or event that can be connected to other entities (see below)
	INPUTFUNC	: maps a string input to a function pointer. Outputs connected to this input
				  will call the notify function when fired.
	INPUT		: maps a string input to a member variable. Outputs connected to this input
				  will update the input data value when fired.
	INPUTNOTIFY	: maps a string input to a member variable/function pointer combo. Outputs
				  connected to this input will update the data value and call the notify
				  function when fired.

some of these can overlap.  all the data descriptions usable are:

	DEFINE_FIELD(		name,	fieldtype )
	DEFINE_KEYFIELD(	name,	fieldtype,	mapname )
	DEFINE_KEYFIELD_NOTSAVED(	name,	fieldtype,	mapname )
	DEFINE_ARRAY(		name,	fieldtype,	count )
	DEFINE_GLOBAL_FIELD(name,	fieldtype )
	DEFINE_CUSTOM_FIELD(name,	datafuncs,	mapname )
	DEFINE_GLOBAL_KEYFIELD(name,	fieldtype,	mapname )

where:
	type is the name of the class (eg. CBaseEntity)
	name is the name of the variable in the class (eg. m_iHealth)
	fieldtype is the type of data (FIELD_STRING, FIELD_INTEGER, etc)
	mapname is the string by which this variable is associated with map file data
	count is the number of items in the array
	datafuncs is a struct containing function pointers for a custom-defined save/restore/parse

OUTPUTS:

 	DEFINE_OUTPUT(		outputvar,	outputname )

	This maps the string 'outputname' to the COutput-derived member variable outputvar.  In the VMF
	file these outputs can be hooked up to inputs (see above).  Whenever the internal state
	of an entity changes it will often fire off outputs so that map makers can hook up behaviors.
	e.g.  A door entity would have OnDoorOpen, OnDoorClose, OnTouched, etc outputs.
*/


#include "cbase.h"
#include "entitylist.h"
#include "mapentities_shared.h"
#include "isaverestore.h"
#include "eventqueue.h"
#include "entityinput.h"
#include "entityoutput.h"
#include "mempool.h"
#include "tier1/strtools.h"
#include "datacache/imdlcache.h"
#include "env_debughistory.h"

#include "tier0/vprof.h"

//HL1
#include	"extdll.h"
#include	"util.h"
#include	"saverestore.h"
#include	"client.h"
#include	"decals.h"
#include	"gamerules.h"
#include	"game.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ISaveRestoreOps *variantFuncs;	// function pointer set for save/restoring variants

BEGIN_SIMPLE_DATADESC( CEventAction )
	DEFINE_FIELD( m_iTarget, FIELD_STRING ),
	DEFINE_FIELD( m_iTargetInput, FIELD_STRING ),
	DEFINE_FIELD( m_iParameter, FIELD_STRING ),
	DEFINE_FIELD( m_flDelay, FIELD_FLOAT ),
	DEFINE_FIELD( m_nTimesToFire, FIELD_INTEGER ),
	DEFINE_FIELD( m_iIDStamp, FIELD_INTEGER ),

	// This is dealt with by the Restore method
	// DEFINE_FIELD( m_pNext, CEventAction ),
END_DATADESC()


// ID Stamp used to uniquely identify every output
int CEventAction::s_iNextIDStamp = 0;


//-----------------------------------------------------------------------------
// Purpose: Creates an event action and assigns it an unique ID stamp.
// Input  : ActionData - the map file data block descibing the event action.
//-----------------------------------------------------------------------------
CEventAction::CEventAction( const char *ActionData )
{
	m_pNext = NULL;
	m_iIDStamp = ++s_iNextIDStamp;

	m_flDelay = 0;
	m_iTarget = NULL_STRING;
	m_iParameter = NULL_STRING;
	m_iTargetInput = NULL_STRING;
	m_nTimesToFire = EVENT_FIRE_ALWAYS;

	if (ActionData == NULL)
		return;

	char szToken[256];

	//
	// Parse the target name.
	//
	const char *psz = nexttoken(szToken, ActionData, ',');
	if (szToken[0] != '\0')
	{
		m_iTarget = AllocPooledString(szToken);
	}

	//
	// Parse the input name.
	//
	psz = nexttoken(szToken, psz, ',');
	if (szToken[0] != '\0')
	{
		m_iTargetInput = AllocPooledString(szToken);
	}
	else
	{
		m_iTargetInput = AllocPooledString("Use");
	}

	//
	// Parse the parameter override.
	//
	psz = nexttoken(szToken, psz, ',');
	if (szToken[0] != '\0')
	{
		m_iParameter = AllocPooledString(szToken);
	}

	//
	// Parse the delay.
	//
	psz = nexttoken(szToken, psz, ',');
	if (szToken[0] != '\0')
	{
		m_flDelay = atof(szToken);
	}

	//
	// Parse the number of times to fire.
	//
	nexttoken(szToken, psz, ',');
	if (szToken[0] != '\0')
	{
		m_nTimesToFire = atoi(szToken);
		if (m_nTimesToFire == 0)
		{
			m_nTimesToFire = EVENT_FIRE_ALWAYS;
		}
	}
}


// this memory pool stores blocks around the size of CEventAction/inputitem_t structs
// can be used for other blocks; will error if to big a block is tried to be allocated
CUtlMemoryPool g_EntityListPool( MAX(sizeof(CEventAction),sizeof(CMultiInputVar::inputitem_t)), 512, CUtlMemoryPool::GROW_FAST, "g_EntityListPool" );

#include "tier0/memdbgoff.h"

void *CEventAction::operator new( size_t stAllocateBlock )
{
	return g_EntityListPool.Alloc( stAllocateBlock );
}

void *CEventAction::operator new( size_t stAllocateBlock, int nBlockUse, const char *pFileName, int nLine )
{
	return g_EntityListPool.Alloc( stAllocateBlock );
}

void CEventAction::operator delete( void *pMem )
{
	g_EntityListPool.Free( pMem );
}

#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Returns the highest-valued delay in our list of event actions.
//-----------------------------------------------------------------------------
float CBaseEntityOutput::GetMaxDelay(void)
{
	float flMaxDelay = 0;
	CEventAction *ev = m_ActionList;

	while (ev != NULL)
	{
		if (ev->m_flDelay > flMaxDelay)
		{
			flMaxDelay = ev->m_flDelay;
		}
		ev = ev->m_pNext;
	}

	return(flMaxDelay);
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CBaseEntityOutput::~CBaseEntityOutput()
{
	CEventAction *ev = m_ActionList;
	while (ev != NULL)
	{
		CEventAction *pNext = ev->m_pNext;	
		delete ev;
		ev = pNext;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Fires the event, causing a sequence of action to occur in other ents.
// Input  : pActivator - Entity that initiated this sequence of actions.
//			pCaller - Entity that is actually causing the event.
//-----------------------------------------------------------------------------
void CBaseEntityOutput::FireOutput(variant_t Value, CBaseEntity *pActivator, CBaseEntity *pCaller, float fDelay)
{
	//
	// Iterate through all eventactions and fire them off.
	//
	CEventAction *ev = m_ActionList;
	CEventAction *prev = NULL;
	
	while (ev != NULL)
	{
		if (ev->m_iParameter == NULL_STRING)
		{
			//
			// Post the event with the default parameter.
			//
			g_EventQueue.AddEvent( STRING(ev->m_iTarget), STRING(ev->m_iTargetInput), Value, ev->m_flDelay + fDelay, pActivator, pCaller, ev->m_iIDStamp );
		}
		else
		{
			//
			// Post the event with a parameter override.
			//
			variant_t ValueOverride;
			ValueOverride.SetString( ev->m_iParameter );
			g_EventQueue.AddEvent( STRING(ev->m_iTarget), STRING(ev->m_iTargetInput), ValueOverride, ev->m_flDelay, pActivator, pCaller, ev->m_iIDStamp );
		}

		if ( ev->m_flDelay )
		{
			char szBuffer[256];
			Q_snprintf( szBuffer,
						sizeof(szBuffer),
						"(%0.2f) output: (%s,%s) -> (%s,%s,%.1f)(%s)\n",
						engine->GetServerTime(),
						pCaller ? STRING(pCaller->m_iClassname) : "NULL",
						pCaller ? STRING(pCaller->GetEntityName()) : "NULL",
						STRING(ev->m_iTarget),
						STRING(ev->m_iTargetInput),
						ev->m_flDelay,
						STRING(ev->m_iParameter) );

			DevMsg( 2, "%s", szBuffer );
			ADD_DEBUG_HISTORY( HISTORY_ENTITY_IO, szBuffer );
		}
		else
		{
			char szBuffer[256];
			Q_snprintf( szBuffer,
						sizeof(szBuffer),
						"(%0.2f) output: (%s,%s) -> (%s,%s)(%s)\n",
						engine->GetServerTime(),
						pCaller ? STRING(pCaller->m_iClassname) : "NULL",
						pCaller ? STRING(pCaller->GetEntityName()) : "NULL", STRING(ev->m_iTarget),
						STRING(ev->m_iTargetInput),
						STRING(ev->m_iParameter) );

			DevMsg( 2, "%s", szBuffer );
			ADD_DEBUG_HISTORY( HISTORY_ENTITY_IO, szBuffer );
		}

		if ( pCaller && pCaller->m_debugOverlays & OVERLAY_MESSAGE_BIT)
		{
			pCaller->DrawOutputOverlay(ev);
		}

		//
		// Remove the event action from the list if it was set to be fired a finite
		// number of times (and has been).
		//
		bool bRemove = false;
		if (ev->m_nTimesToFire != EVENT_FIRE_ALWAYS)
		{
			ev->m_nTimesToFire--;
			if (ev->m_nTimesToFire == 0)
			{
				char szBuffer[256];
				Q_snprintf( szBuffer, sizeof(szBuffer), "Removing from action list: (%s,%s) -> (%s,%s)\n", pCaller ? STRING(pCaller->m_iClassname) : "NULL", pCaller ? STRING(pCaller->GetEntityName()) : "NULL", STRING(ev->m_iTarget), STRING(ev->m_iTargetInput));
				DevMsg( 2, "%s", szBuffer );
				ADD_DEBUG_HISTORY( HISTORY_ENTITY_IO, szBuffer );
				bRemove = true;
			}
		}

		if (!bRemove)
		{
			prev = ev;
			ev = ev->m_pNext;
		}
		else
		{
			if (prev != NULL)
			{
				prev->m_pNext = ev->m_pNext;
			}
			else
			{
				m_ActionList = ev->m_pNext;
			}

			CEventAction *next = ev->m_pNext;
			delete ev;
			ev = next;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Parameterless firing of an event
// Input  : pActivator - 
//			pCaller - 
//-----------------------------------------------------------------------------
void COutputEvent::FireOutput(CBaseEntity *pActivator, CBaseEntity *pCaller, float fDelay)
{
	variant_t Val;
	Val.Set( FIELD_VOID, NULL );
	CBaseEntityOutput::FireOutput(Val, pActivator, pCaller, fDelay);
}


void CBaseEntityOutput::ParseEventAction( const char *EventData )
{
	AddEventAction( new CEventAction( EventData ) );
}

void CBaseEntityOutput::AddEventAction( CEventAction *pEventAction )
{
	pEventAction->m_pNext = m_ActionList;
	m_ActionList = pEventAction;
}


// save data description for the event queue
BEGIN_SIMPLE_DATADESC( CBaseEntityOutput )

	DEFINE_CUSTOM_FIELD( m_Value, variantFuncs ),

	// This is saved manually by CBaseEntityOutput::Save
	// DEFINE_FIELD( m_ActionList, CEventAction ),
END_DATADESC()


int CBaseEntityOutput::Save( ISave &save )
{
	// save that value out to disk, so we know how many to restore
	if ( !save.WriteFields( "Value", this, NULL, m_DataMap.dataDesc, m_DataMap.dataNumFields ) )
		return 0;

	for ( CEventAction *ev = m_ActionList; ev != NULL; ev = ev->m_pNext )
	{
		if ( !save.WriteFields( "EntityOutput", ev, NULL, ev->m_DataMap.dataDesc, ev->m_DataMap.dataNumFields ) )
			return 0;
	}

	return 1;
}

int CBaseEntityOutput::Restore( IRestore &restore, int elementCount )
{
	// load the number of items saved
	if ( !restore.ReadFields( "Value", this, NULL, m_DataMap.dataDesc, m_DataMap.dataNumFields ) )
		return 0;

	m_ActionList = NULL;

	// read in all the fields
	CEventAction *lastEv = NULL;
	for ( int i = 0; i < elementCount; i++ )
	{
		CEventAction *ev = new CEventAction(NULL);

		if ( !restore.ReadFields( "EntityOutput", ev, NULL, ev->m_DataMap.dataDesc, ev->m_DataMap.dataNumFields ) )
			return 0;

		// add it to the list in the same order it was saved in
		if ( lastEv )
		{
			lastEv->m_pNext = ev;
		}
		else
		{
			m_ActionList = ev;
		}
		ev->m_pNext = NULL;
		lastEv = ev;
	}

	return 1;
}

int CBaseEntityOutput::NumberOfElements( void )
{
	int count = 0;
	for ( CEventAction *ev = m_ActionList; ev != NULL; ev = ev->m_pNext )
	{
		count++;
	}
	return count;
}

/// Delete every single action in the action list. 
void CBaseEntityOutput::DeleteAllElements( void ) 
{
	// walk front to back, deleting as we go. We needn't fix up pointers because
	// EVERYTHING will die.

	CEventAction *pNext = m_ActionList;
	// wipe out the head
	m_ActionList = NULL;
	while (pNext)
	{
		register CEventAction *strikeThis = pNext;
		pNext = pNext->m_pNext;
		delete strikeThis;
	}

}

/// EVENTS save/restore parsing wrapper

class CEventsSaveDataOps : public ISaveRestoreOps
{
	virtual void Save( const SaveRestoreFieldInfo_t &fieldInfo, ISave *pSave )
	{
		AssertMsg( fieldInfo.pTypeDesc->fieldSize == 1, "CEventsSaveDataOps does not support arrays");

		CBaseEntityOutput *ev = (CBaseEntityOutput*)fieldInfo.pField;
		const int fieldSize = fieldInfo.pTypeDesc->fieldSize;
 		for ( int i = 0; i < fieldSize; i++, ev++ )
		{
			// save out the number of fields
			int numElements = ev->NumberOfElements();
			pSave->WriteInt( &numElements, 1 );

			// save the event data
			ev->Save( *pSave );
		}
	}

	virtual void Restore( const SaveRestoreFieldInfo_t &fieldInfo, IRestore *pRestore )
	{
		AssertMsg( fieldInfo.pTypeDesc->fieldSize == 1, "CEventsSaveDataOps does not support arrays");

		CBaseEntityOutput *ev = (CBaseEntityOutput*)fieldInfo.pField;
		const int fieldSize = fieldInfo.pTypeDesc->fieldSize;
		for ( int i = 0; i < fieldSize; i++, ev++ )
		{
			int nElements = pRestore->ReadInt();
			
			Assert( nElements < 100 );

			ev->Restore( *pRestore, nElements );
		}
	}

	virtual bool IsEmpty( const SaveRestoreFieldInfo_t &fieldInfo )
	{
		AssertMsg( fieldInfo.pTypeDesc->fieldSize == 1, "CEventsSaveDataOps does not support arrays");
		
		// check all the elements of the array (usually only 1)
		CBaseEntityOutput *ev = (CBaseEntityOutput*)fieldInfo.pField;
		const int fieldSize = fieldInfo.pTypeDesc->fieldSize;
		for ( int i = 0; i < fieldSize; i++, ev++ )
		{
			// It's not empty if it has events or if it has a non-void variant value
			if (( ev->NumberOfElements() != 0 ) || ( ev->ValueFieldType() != FIELD_VOID ))
				return 0;
		}

		// variant has no data
		return 1;
	}

	virtual void MakeEmpty( const SaveRestoreFieldInfo_t &fieldInfo )
	{
		// Don't no how to. This is okay, since objects of this type
		// are always born clean before restore, and not reused
	}

	virtual bool Parse( const SaveRestoreFieldInfo_t &fieldInfo, char const* szValue )
	{
		CBaseEntityOutput *ev = (CBaseEntityOutput*)fieldInfo.pField;
		ev->ParseEventAction( szValue );
		return true;
	}
};

CEventsSaveDataOps g_EventsSaveDataOps;
ISaveRestoreOps *eventFuncs = &g_EventsSaveDataOps;

//-----------------------------------------------------------------------------
//			CMultiInputVar implementation
//
// Purpose: holds a list of inputs and their ID tags
//			used for entities that hold inputs from a set of other entities
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Purpose: destructor, frees the data list
//-----------------------------------------------------------------------------
CMultiInputVar::~CMultiInputVar()
{
	if ( m_InputList )
	{
		while ( m_InputList->next != NULL )
		{
			inputitem_t *input = m_InputList->next;
			m_InputList->next = input->next;
			delete input;
		}
		delete m_InputList;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Updates the data set with a new value
// Input  : newVal - the new value to add to or update in the list
//			outputID - the source of the value
//-----------------------------------------------------------------------------
void CMultiInputVar::AddValue( variant_t newVal, int outputID )
{
	// see if it's already in the list
	inputitem_t *inp;
	for ( inp = m_InputList; inp != NULL; inp = inp->next )
	{
		// already in list, so just update this link
		if ( inp->outputID == outputID )
		{
			inp->value = newVal;
			return;
		}
	}

	// add to start of list
	inp = new inputitem_t;
	inp->value = newVal;
	inp->outputID = outputID;
	if ( !m_InputList )
	{
		m_InputList = inp;
		inp->next = NULL;
	}
	else
	{
		inp->next = m_InputList;
		m_InputList = inp;
	}
}


#include "tier0/memdbgoff.h"

//-----------------------------------------------------------------------------
// Purpose: allocates memory from the entitylist pool
//-----------------------------------------------------------------------------
void *CMultiInputVar::inputitem_t::operator new( size_t stAllocateBlock )
{
	return g_EntityListPool.Alloc( stAllocateBlock );
}

void *CMultiInputVar::inputitem_t::operator new( size_t stAllocateBlock, int nBlockUse, const char *pFileName, int nLine )
{
	return g_EntityListPool.Alloc( stAllocateBlock );
}

//-----------------------------------------------------------------------------
// Purpose: frees memory from the entitylist pool
//-----------------------------------------------------------------------------
void CMultiInputVar::inputitem_t::operator delete( void *pMem )
{
	g_EntityListPool.Free( pMem );
}

#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
//			CEventQueue implementation
//
// Purpose: holds and executes a global prioritized queue of entity actions
//-----------------------------------------------------------------------------
DEFINE_FIXEDSIZE_ALLOCATOR( EventQueuePrioritizedEvent_t, 128, CUtlMemoryPool::GROW_SLOW );

CEventQueue g_EventQueue;

CEventQueue::CEventQueue()
{
	m_Events.m_flFireTime = -FLT_MAX;
	m_Events.m_pNext = NULL;

	Init();
}

CEventQueue::~CEventQueue()
{
	Clear();
}

// Robin: Left here for backwards compatability.
class CEventQueueSaveLoadProxy : public CLogicalEntity
{
	DECLARE_CLASS( CEventQueueSaveLoadProxy, CLogicalEntity );

	int Save( ISave &save )
	{
		if ( !BaseClass::Save(save) )
			return 0;

		// save out the message queue
		return g_EventQueue.Save( save );
	}


	int Restore( IRestore &restore )
	{
		if ( !BaseClass::Restore(restore) )
			return 0;

		// restore the event queue
		int iReturn = g_EventQueue.Restore( restore );

		// Now remove myself, because the CEventQueue_SaveRestoreBlockHandler
		// will handle future saves.
		UTIL_Remove( this );

		return iReturn;
	}
};

LINK_ENTITY_TO_CLASS(event_queue_saveload_proxy, CEventQueueSaveLoadProxy);

//-----------------------------------------------------------------------------
// EVENT QUEUE SAVE / RESTORE
//-----------------------------------------------------------------------------
static short EVENTQUEUE_SAVE_RESTORE_VERSION = 1;

class CEventQueue_SaveRestoreBlockHandler : public CDefSaveRestoreBlockHandler
{
public:
	const char *GetBlockName()
	{
		return "EventQueue";
	}

	//---------------------------------

	void Save( ISave *pSave )
	{
		g_EventQueue.Save( *pSave );
	}

	//---------------------------------

	void WriteSaveHeaders( ISave *pSave )
	{
		pSave->WriteShort( &EVENTQUEUE_SAVE_RESTORE_VERSION );
	}

	//---------------------------------

	void ReadRestoreHeaders( IRestore *pRestore )
	{
		// No reason why any future version shouldn't try to retain backward compatability. The default here is to not do so.
		short version;
		pRestore->ReadShort( &version );
		m_fDoLoad = ( version == EVENTQUEUE_SAVE_RESTORE_VERSION );
	}

	//---------------------------------

	void Restore( IRestore *pRestore, bool createPlayers )
	{
		if ( m_fDoLoad )
		{
			g_EventQueue.Restore( *pRestore );
		}
	}

private:
	bool m_fDoLoad;
};

//-----------------------------------------------------------------------------

CEventQueue_SaveRestoreBlockHandler g_EventQueue_SaveRestoreBlockHandler;

//-------------------------------------

ISaveRestoreBlockHandler *GetEventQueueSaveRestoreBlockHandler()
{
	return &g_EventQueue_SaveRestoreBlockHandler;
}


void CEventQueue::Init( void )
{
	Clear();
}

void CEventQueue::Clear( void )
{
	// delete all the events in the queue
	EventQueuePrioritizedEvent_t *pe = m_Events.m_pNext;
	
	while ( pe != NULL )
	{
		EventQueuePrioritizedEvent_t *next = pe->m_pNext;
		delete pe;
		pe = next;
	}

	m_Events.m_pNext = NULL;
}

void CEventQueue::Dump( void )
{
	EventQueuePrioritizedEvent_t *pe = m_Events.m_pNext;

	Msg( "Dumping event queue. Current time is: %.2f\n", engine->GetServerTime() );

	while ( pe != NULL )
	{
		EventQueuePrioritizedEvent_t *next = pe->m_pNext;

		Msg("   (%.2f) Target: '%s', Input: '%s', Parameter '%s'. Activator: '%s', Caller '%s'.  \n", 
			pe->m_flFireTime, 
			STRING(pe->m_iTarget), 
			STRING(pe->m_iTargetInput), 
			pe->m_VariantValue.String(),
			pe->m_pActivator ? pe->m_pActivator->GetDebugName() : "None", 
			pe->m_pCaller ? pe->m_pCaller->GetDebugName() : "None"  );

		pe = next;
	}

	Msg("Finished dump.\n");
}


//-----------------------------------------------------------------------------
// Purpose: adds the action into the correct spot in the priority queue, targeting entity via string name
//-----------------------------------------------------------------------------
void CEventQueue::AddEvent( const char *target, const char *targetInput, variant_t Value, float fireDelay, CBaseEntity *pActivator, CBaseEntity *pCaller, int outputID )
{
	// build the new event
	EventQueuePrioritizedEvent_t *newEvent = new EventQueuePrioritizedEvent_t;
	newEvent->m_flFireTime = engine->GetServerTime() + fireDelay;	// priority key in the priority queue
	newEvent->m_iTarget = MAKE_STRING( target );
	newEvent->m_pEntTarget = NULL;
	newEvent->m_iTargetInput = MAKE_STRING( targetInput );
	newEvent->m_pActivator = pActivator;
	newEvent->m_pCaller = pCaller;
	newEvent->m_VariantValue = Value;
	newEvent->m_iOutputID = outputID;

	AddEvent( newEvent );
}

//-----------------------------------------------------------------------------
// Purpose: adds the action into the correct spot in the priority queue, targeting entity via pointer
//-----------------------------------------------------------------------------
void CEventQueue::AddEvent( CBaseEntity *target, const char *targetInput, variant_t Value, float fireDelay, CBaseEntity *pActivator, CBaseEntity *pCaller, int outputID )
{
	// build the new event
	EventQueuePrioritizedEvent_t *newEvent = new EventQueuePrioritizedEvent_t;
	newEvent->m_flFireTime = engine->GetServerTime() + fireDelay;	// primary priority key in the priority queue
	newEvent->m_iTarget = NULL_STRING;
	newEvent->m_pEntTarget = target;
	newEvent->m_iTargetInput = MAKE_STRING( targetInput );
	newEvent->m_pActivator = pActivator;
	newEvent->m_pCaller = pCaller;
	newEvent->m_VariantValue = Value;
	newEvent->m_iOutputID = outputID;

	AddEvent( newEvent );
}

void CEventQueue::AddEvent( CBaseEntity *target, const char *action, float fireDelay, CBaseEntity *pActivator, CBaseEntity *pCaller, int outputID )
{
	variant_t Value;
	Value.Set( FIELD_VOID, NULL );
	AddEvent( target, action, Value, fireDelay, pActivator, pCaller, outputID );
}


//-----------------------------------------------------------------------------
// Purpose: private function, adds an event into the list
// Input  : *newEvent - the (already built) event to add
//-----------------------------------------------------------------------------
void CEventQueue::AddEvent( EventQueuePrioritizedEvent_t *newEvent )
{
	// loop through the actions looking for a place to insert
	EventQueuePrioritizedEvent_t *pe;
	for ( pe = &m_Events; pe->m_pNext != NULL; pe = pe->m_pNext )
	{
		if ( pe->m_pNext->m_flFireTime > newEvent->m_flFireTime )
		{
			break;
		}
	}

	Assert( pe );

	// insert
	newEvent->m_pNext = pe->m_pNext;
	newEvent->m_pPrev = pe;
	pe->m_pNext = newEvent;
	if ( newEvent->m_pNext )
	{
		newEvent->m_pNext->m_pPrev = newEvent;
	}
}

void CEventQueue::RemoveEvent( EventQueuePrioritizedEvent_t *pe )
{
	Assert( pe->m_pPrev );
	pe->m_pPrev->m_pNext = pe->m_pNext;
	if ( pe->m_pNext )
	{
		pe->m_pNext->m_pPrev = pe->m_pPrev;
	}
}


//-----------------------------------------------------------------------------
// Purpose: fires off any events in the queue who's fire time is (or before) the present time
//-----------------------------------------------------------------------------
void CEventQueue::ServiceEvents( void )
{
	if (!CBaseEntity::Debug_ShouldStep())
	{
		return;
	}

	EventQueuePrioritizedEvent_t *pe = m_Events.m_pNext;

	while ( pe != NULL && pe->m_flFireTime <= engine->GetServerTime() )
	{
		MDLCACHE_CRITICAL_SECTION();

		bool targetFound = false;

		// find the targets
		if ( pe->m_iTarget != NULL_STRING )
		{
			// In the context the event, the searching entity is also the caller
			CBaseEntity *pSearchingEntity = pe->m_pCaller;
			CBaseEntity *target = NULL;
			while ( 1 )
			{
				target = gEntList.FindEntityByName( target, pe->m_iTarget, pSearchingEntity, pe->m_pActivator, pe->m_pCaller );
				if ( !target )
					break;

				// pump the action into the target
				target->AcceptInput( STRING(pe->m_iTargetInput), pe->m_pActivator, pe->m_pCaller, pe->m_VariantValue, pe->m_iOutputID );
				targetFound = true;
			}
		}

		// direct pointer
		if ( pe->m_pEntTarget != NULL )
		{
			pe->m_pEntTarget->AcceptInput( STRING(pe->m_iTargetInput), pe->m_pActivator, pe->m_pCaller, pe->m_VariantValue, pe->m_iOutputID );
			targetFound = true;
		}

		if ( !targetFound )
		{
			// See if we can find a target if we treat the target as a classname
			if ( pe->m_iTarget != NULL_STRING )
			{
				CBaseEntity *target = NULL;
				while ( 1 )
				{
					target = gEntList.FindEntityByClassname( target, STRING(pe->m_iTarget) );
					if ( !target )
						break;

					// pump the action into the target
					target->AcceptInput( STRING(pe->m_iTargetInput), pe->m_pActivator, pe->m_pCaller, pe->m_VariantValue, pe->m_iOutputID );
					targetFound = true;
				}
			}
		}

		if ( !targetFound )
		{
			const char *pClass ="", *pName = "";
			
			// might be NULL
			if ( pe->m_pCaller )
			{
				pClass = STRING(pe->m_pCaller->m_iClassname);
				pName = STRING(pe->m_pCaller->GetEntityName());
			}
			
			char szBuffer[256];
			Q_snprintf( szBuffer, sizeof(szBuffer), "unhandled input: (%s) -> (%s), from (%s,%s); target entity not found\n", STRING(pe->m_iTargetInput), STRING(pe->m_iTarget), pClass, pName );
			DevMsg( 2, "%s", szBuffer );
			ADD_DEBUG_HISTORY( HISTORY_ENTITY_IO, szBuffer );
		}

		// remove the event from the list (remembering that the queue may have been added to)
		RemoveEvent( pe );
		delete pe;

		//
		// If we are in debug mode, exit the loop if we have fired the correct number of events.
		//
		if (CBaseEntity::Debug_IsPaused())
		{
			if (!CBaseEntity::Debug_Step())
			{
				break;
			}
		}

		// restart the list (to catch any new items have probably been added to the queue)
		pe = m_Events.m_pNext;	
	}
}

//-----------------------------------------------------------------------------
// Purpose: Dumps the contents of the Entity I/O event queue to the console.
//-----------------------------------------------------------------------------
void CC_DumpEventQueue()
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	g_EventQueue.Dump();
}
static ConCommand dumpeventqueue( "dumpeventqueue", CC_DumpEventQueue, "Dump the contents of the Entity I/O event queue to the console." );

//-----------------------------------------------------------------------------
// Purpose: Removes all pending events from the I/O queue that were added by the
//			given caller.
//
//			TODO: This is only as reliable as callers are in passing the correct
//				  caller pointer when they fire the outputs. Make more foolproof.
//-----------------------------------------------------------------------------
void CEventQueue::CancelEvents( CBaseEntity *pCaller )
{
	if (!pCaller)
		return;

	EventQueuePrioritizedEvent_t *pCur = m_Events.m_pNext;

	while (pCur != NULL)
	{
		bool bDelete = false;
		if (pCur->m_pCaller == pCaller)
		{
			// Pointers match; make sure everything else matches.
			if (!stricmp(STRING(pCur->m_pCaller->GetEntityName()), STRING(pCaller->GetEntityName())) &&
				!stricmp(pCur->m_pCaller->GetClassname(), pCaller->GetClassname()))
			{
				// Found a matching event; delete it from the queue.
				bDelete = true;
			}
		}

		EventQueuePrioritizedEvent_t *pCurSave = pCur;
		pCur = pCur->m_pNext;

		if (bDelete)
		{
			RemoveEvent( pCurSave );
			delete pCurSave;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Removes all pending events of the specified type from the I/O queue of the specified target
//
//			TODO: This is only as reliable as callers are in passing the correct
//				  caller pointer when they fire the outputs. Make more foolproof.
//-----------------------------------------------------------------------------
void CEventQueue::CancelEventOn( CBaseEntity *pTarget, const char *sInputName )
{
	if (!pTarget)
		return;

	EventQueuePrioritizedEvent_t *pCur = m_Events.m_pNext;

	while (pCur != NULL)
	{
		bool bDelete = false;
		if (pCur->m_pEntTarget == pTarget)
		{
			if ( !Q_strncmp( STRING(pCur->m_iTargetInput), sInputName, strlen(sInputName) ) )
			{
				// Found a matching event; delete it from the queue.
				bDelete = true;
			}
		}

		EventQueuePrioritizedEvent_t *pCurSave = pCur;
		pCur = pCur->m_pNext;

		if (bDelete)
		{
			RemoveEvent( pCurSave );
			delete pCurSave;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return true if the target has any pending inputs.
// Input  : *pTarget - 
//			*sInputName - NULL for any input, or a specified one
//-----------------------------------------------------------------------------
bool CEventQueue::HasEventPending( CBaseEntity *pTarget, const char *sInputName )
{
	if (!pTarget)
		return false;

	EventQueuePrioritizedEvent_t *pCur = m_Events.m_pNext;

	while (pCur != NULL)
	{
		if (pCur->m_pEntTarget == pTarget)
		{
			if ( !sInputName )
				return true;

			if ( !Q_strncmp( STRING(pCur->m_iTargetInput), sInputName, strlen(sInputName) ) )
				return true;
		}

		pCur = pCur->m_pNext;
	}

	return false;
}

void ServiceEventQueue( void )
{
	VPROF("ServiceEventQueue()");

	g_EventQueue.ServiceEvents();
}



// save data description for the event queue
BEGIN_SIMPLE_DATADESC( CEventQueue )
	// These are saved explicitly in CEventQueue::Save below
	// DEFINE_FIELD( m_Events, EventQueuePrioritizedEvent_t ),

	DEFINE_FIELD( m_iListCount, FIELD_INTEGER ),	// this value is only used during save/restore
END_DATADESC()


// save data for a single event in the queue
BEGIN_SIMPLE_DATADESC( EventQueuePrioritizedEvent_t )
	DEFINE_FIELD( m_flFireTime, FIELD_TIME ),
	DEFINE_FIELD( m_iTarget, FIELD_STRING ),
	DEFINE_FIELD( m_iTargetInput, FIELD_STRING ),
	DEFINE_FIELD( m_pActivator, FIELD_EHANDLE ),
	DEFINE_FIELD( m_pCaller, FIELD_EHANDLE ),
	DEFINE_FIELD( m_pEntTarget, FIELD_EHANDLE ),
	DEFINE_FIELD( m_iOutputID, FIELD_INTEGER ),
	DEFINE_CUSTOM_FIELD( m_VariantValue, variantFuncs ),

//	DEFINE_FIELD( m_pNext, FIELD_??? ),
//	DEFINE_FIELD( m_pPrev, FIELD_??? ),
END_DATADESC()


int CEventQueue::Save( ISave &save )
{
	// count the number of items in the queue
	EventQueuePrioritizedEvent_t *pe;

	m_iListCount = 0;
	for ( pe = m_Events.m_pNext; pe != NULL; pe = pe->m_pNext )
	{
		m_iListCount++;
	}

	// save that value out to disk, so we know how many to restore
	if ( !save.WriteFields( "EventQueue", this, NULL, m_DataMap.dataDesc, m_DataMap.dataNumFields ) )
		return 0;
	
	// cycle through all the events, saving them all
	for ( pe = m_Events.m_pNext; pe != NULL; pe = pe->m_pNext )
	{
		if ( !save.WriteFields( "PEvent", pe, NULL, pe->m_DataMap.dataDesc, pe->m_DataMap.dataNumFields ) )
			return 0;
	}

	return 1;
}


int CEventQueue::Restore( IRestore &restore )
{
	// clear the event queue
	Clear();

	// rebuild the event queue by restoring all the queue items
	EventQueuePrioritizedEvent_t tmpEvent;

	// load the number of items saved
	if ( !restore.ReadFields( "EventQueue", this, NULL, m_DataMap.dataDesc, m_DataMap.dataNumFields ) )
		return 0;
	
	for ( int i = 0; i < m_iListCount; i++ )
	{
		if ( !restore.ReadFields( "PEvent", &tmpEvent, NULL, tmpEvent.m_DataMap.dataDesc, tmpEvent.m_DataMap.dataNumFields ) )
			return 0;

		// add the restored event into the list
		if ( tmpEvent.m_pEntTarget )
		{
			AddEvent( tmpEvent.m_pEntTarget,
					  STRING(tmpEvent.m_iTargetInput),
					  tmpEvent.m_VariantValue,
					  tmpEvent.m_flFireTime - engine->GetServerTime(),
					  tmpEvent.m_pActivator,
					  tmpEvent.m_pCaller,
					  tmpEvent.m_iOutputID );
		}
		else
		{
			AddEvent( STRING(tmpEvent.m_iTarget),
					  STRING(tmpEvent.m_iTargetInput),
					  tmpEvent.m_VariantValue,
					  tmpEvent.m_flFireTime - engine->GetServerTime(),
					  tmpEvent.m_pActivator,
					  tmpEvent.m_pCaller,
					  tmpEvent.m_iOutputID );
		}
	}

	return 1;
}

////////////////////////// variant_t implementation //////////////////////////

// BUGBUG: Add support for function pointer save/restore to variants
// BUGBUG: Must pass datamap_t to read/write fields 
void variant_t::Set( fieldtype_t ftype, void *data )
{
	fieldType = ftype;

	switch ( ftype )
	{
	case FIELD_BOOLEAN:		bVal = *((bool *)data);				break;
	case FIELD_CHARACTER:	iVal = *((char *)data);				break;
	case FIELD_SHORT:		iVal = *((short *)data);			break;
	case FIELD_INTEGER:		iVal = *((int *)data);				break;
	case FIELD_STRING:		iszVal = *((string_t *)data);		break;
	case FIELD_FLOAT:		flVal = *((float *)data);			break;
	case FIELD_COLOR32:		rgbaVal = *((color32 *)data);		break;

	case FIELD_VECTOR:
	case FIELD_POSITION_VECTOR:
	{
		vecVal[0] = ((float *)data)[0];
		vecVal[1] = ((float *)data)[1];
		vecVal[2] = ((float *)data)[2];
		break;
	}

	case FIELD_EHANDLE:		eVal = *((EHANDLE *)data);			break;
	case FIELD_CLASSPTR:	eVal = *((CBaseEntity **)data);		break;
	case FIELD_VOID:		
	default:
		iVal = 0; fieldType = FIELD_VOID;	
		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Copies the value in the variant into a block of memory
// Input  : *data - the block to write into
//-----------------------------------------------------------------------------
void variant_t::SetOther( void *data )
{
	switch ( fieldType )
	{
	case FIELD_BOOLEAN:		*((bool *)data) = bVal != 0;		break;
	case FIELD_CHARACTER:	*((char *)data) = iVal;				break;
	case FIELD_SHORT:		*((short *)data) = iVal;			break;
	case FIELD_INTEGER:		*((int *)data) = iVal;				break;
	case FIELD_STRING:		*((string_t *)data) = iszVal;		break;
	case FIELD_FLOAT:		*((float *)data) = flVal;			break;
	case FIELD_COLOR32:		*((color32 *)data) = rgbaVal;		break;

	case FIELD_VECTOR:
	case FIELD_POSITION_VECTOR:
	{
		((float *)data)[0] = vecVal[0];
		((float *)data)[1] = vecVal[1];
		((float *)data)[2] = vecVal[2];
		break;
	}

	case FIELD_EHANDLE:		*((EHANDLE *)data) = eVal;			break;
	case FIELD_CLASSPTR:	*((CBaseEntity **)data) = eVal;		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Converts the variant to a new type. This function defines which I/O
//			types can be automatically converted between. Connections that require
//			an unsupported conversion will cause an error message at runtime.
// Input  : newType - the type to convert to
// Output : Returns true on success, false if the conversion is not legal
//-----------------------------------------------------------------------------
bool variant_t::Convert( fieldtype_t newType )
{
	if ( newType == fieldType )
	{
		return true;
	}

	//
	// Converting to a null value is easy.
	//
	if ( newType == FIELD_VOID )
	{
		Set( FIELD_VOID, NULL );
		return true;
	}

	//
	// FIELD_INPUT accepts the variant type directly.
	//
	if ( newType == FIELD_INPUT )
	{
		return true;
	}

	switch ( fieldType )
	{
		case FIELD_INTEGER:
		{
			switch ( newType )
			{
				case FIELD_FLOAT:
				{
					SetFloat( (float) iVal );
					return true;
				}

				case FIELD_BOOLEAN:
				{
					SetBool( iVal != 0 );
					return true;
				}
			}
			break;
		}

		case FIELD_FLOAT:
		{
			switch ( newType )
			{
				case FIELD_INTEGER:
				{
					SetInt( (int) flVal );
					return true;
				}

				case FIELD_BOOLEAN:
				{
					SetBool( flVal != 0 );
					return true;
				}
			}
			break;
		}

		//
		// Everyone must convert from FIELD_STRING if possible, since
		// parameter overrides are always passed as strings.
		//
		case FIELD_STRING:
		{
			switch ( newType )
			{
				case FIELD_INTEGER:
				{
					if (iszVal != NULL_STRING)
					{
						SetInt(atoi(STRING(iszVal)));
					}
					else
					{
						SetInt(0);
					}
					return true;
				}

				case FIELD_FLOAT:
				{
					if (iszVal != NULL_STRING)
					{
						SetFloat(atof(STRING(iszVal)));
					}
					else
					{
						SetFloat(0);
					}
					return true;
				}

				case FIELD_BOOLEAN:
				{
					if (iszVal != NULL_STRING)
					{
						SetBool( atoi(STRING(iszVal)) != 0 );
					}
					else
					{
						SetBool(false);
					}
					return true;
				}

				case FIELD_VECTOR:
				{
					Vector tmpVec = vec3_origin;
					if (sscanf(STRING(iszVal), "[%f %f %f]", &tmpVec[0], &tmpVec[1], &tmpVec[2]) == 0)
					{
						// Try sucking out 3 floats with no []s
						sscanf(STRING(iszVal), "%f %f %f", &tmpVec[0], &tmpVec[1], &tmpVec[2]);
					}
					SetVector3D( tmpVec );
					return true;
				}

				case FIELD_COLOR32:
				{
					int nRed = 0;
					int nGreen = 0;
					int nBlue = 0;
					int nAlpha = 255;

					sscanf(STRING(iszVal), "%d %d %d %d", &nRed, &nGreen, &nBlue, &nAlpha);
					SetColor32( nRed, nGreen, nBlue, nAlpha );
					return true;
				}

				case FIELD_EHANDLE:
				{
					// convert the string to an entity by locating it by classname
					CBaseEntity *ent = NULL;
					if ( iszVal != NULL_STRING )
					{
						// FIXME: do we need to pass an activator in here?
						ent = gEntList.FindEntityByName( NULL, iszVal );
					}
					SetEntity( ent );
					return true;
				}
			}
		
			break;
		}

		case FIELD_EHANDLE:
		{
			switch ( newType )
			{
				case FIELD_STRING:
				{
					// take the entities targetname as the string
					string_t iszStr = NULL_STRING;
					if ( eVal != NULL )
					{
						SetString( eVal->GetEntityName() );
					}
					return true;
				}
			}
			break;
		}
	}

	// invalid conversion
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: All types must be able to display as strings for debugging purposes.
// Output : Returns a pointer to the string that represents this value.
//
//			NOTE: The returned pointer should not be stored by the caller as
//				  subsequent calls to this function will overwrite the contents
//				  of the buffer!
//-----------------------------------------------------------------------------
const char *variant_t::ToString( void ) const
{
	COMPILE_TIME_ASSERT( sizeof(string_t) == sizeof(int) );

	static char szBuf[512];

	switch (fieldType)
	{
	case FIELD_STRING:
		{
			return(STRING(iszVal));
		}

	case FIELD_BOOLEAN:
		{
			if (bVal == 0)
			{
				Q_strncpy(szBuf, "false",sizeof(szBuf));
			}
			else
			{
				Q_strncpy(szBuf, "true",sizeof(szBuf));
			}
			return(szBuf);
		}

	case FIELD_INTEGER:
		{
			Q_snprintf( szBuf, sizeof( szBuf ), "%i", iVal );
			return(szBuf);
		}

	case FIELD_FLOAT:
		{
			Q_snprintf(szBuf,sizeof(szBuf), "%g", flVal);
			return(szBuf);
		}

	case FIELD_COLOR32:
		{
			Q_snprintf(szBuf,sizeof(szBuf), "%d %d %d %d", (int)rgbaVal.r, (int)rgbaVal.g, (int)rgbaVal.b, (int)rgbaVal.a);
			return(szBuf);
		}

	case FIELD_VECTOR:
		{
			Q_snprintf(szBuf,sizeof(szBuf), "[%g %g %g]", (double)vecVal[0], (double)vecVal[1], (double)vecVal[2]);
			return(szBuf);
		}

	case FIELD_VOID:
		{
			szBuf[0] = '\0';
			return(szBuf);
		}

	case FIELD_EHANDLE:
		{
			const char *pszName = (Entity()) ? STRING(Entity()->GetEntityName()) : "<<null entity>>";
			Q_strncpy( szBuf, pszName, 512 );
			return (szBuf);
		}
	}

	return("No conversion to string");
}

#define classNameTypedef variant_t // to satisfy DEFINE... macros

typedescription_t variant_t::m_SaveBool[] =
{
	DEFINE_FIELD( bVal, FIELD_BOOLEAN ),
};
typedescription_t variant_t::m_SaveInt[] =
{
	DEFINE_FIELD( iVal, FIELD_INTEGER ),
};
typedescription_t variant_t::m_SaveFloat[] =
{
	DEFINE_FIELD( flVal, FIELD_FLOAT ),
};
typedescription_t variant_t::m_SaveEHandle[] =
{
	DEFINE_FIELD( eVal, FIELD_EHANDLE ),
};
typedescription_t variant_t::m_SaveString[] =
{
	DEFINE_FIELD( iszVal, FIELD_STRING ),
};
typedescription_t variant_t::m_SaveColor[] =
{
	DEFINE_FIELD( rgbaVal, FIELD_COLOR32 ),
};

#undef classNameTypedef

//
// Struct for saving and restoring vector variants, since they are
// stored as float[3] and we want to take advantage of position vector
// fixup across level transitions.
//
#define classNameTypedef variant_savevector_t // to satisfy DEFINE... macros

struct variant_savevector_t
{
	Vector vecSave;
};
typedescription_t variant_t::m_SaveVector[] =
{
	// Just here to shut up ClassCheck
//	DEFINE_ARRAY( vecVal, FIELD_FLOAT, 3 ),

	DEFINE_FIELD( vecSave, FIELD_VECTOR ),
};
typedescription_t variant_t::m_SavePositionVector[] =
{
	DEFINE_FIELD( vecSave, FIELD_POSITION_VECTOR ),
};
#undef classNameTypedef

#define classNameTypedef variant_savevmatrix_t // to satisfy DEFINE... macros
struct variant_savevmatrix_t
{
	VMatrix matSave;
};
typedescription_t variant_t::m_SaveVMatrix[] =
{
	DEFINE_FIELD( matSave, FIELD_VMATRIX ),
};
typedescription_t variant_t::m_SaveVMatrixWorldspace[] =
{
	DEFINE_FIELD( matSave, FIELD_VMATRIX_WORLDSPACE ),
};
#undef classNameTypedef

#define classNameTypedef variant_savevmatrix3x4_t // to satisfy DEFINE... macros
struct variant_savevmatrix3x4_t
{
	matrix3x4_t matSave;
};
typedescription_t variant_t::m_SaveMatrix3x4Worldspace[] =
{
	DEFINE_FIELD( matSave, FIELD_MATRIX3X4_WORLDSPACE ),
};
#undef classNameTypedef

class CVariantSaveDataOps : public CDefSaveRestoreOps
{
	// saves the entire array of variables
	virtual void Save( const SaveRestoreFieldInfo_t &fieldInfo, ISave *pSave )
	{
		variant_t *var = (variant_t*)fieldInfo.pField;

		int type = var->FieldType();
		pSave->WriteInt( &type, 1 );

		switch ( var->FieldType() )
		{
		case FIELD_VOID:
			break;
		case FIELD_BOOLEAN:	
			pSave->WriteFields( fieldInfo.pTypeDesc->fieldName, var, NULL, variant_t::m_SaveBool, 1 );
			break;
		case FIELD_INTEGER:	
			pSave->WriteFields( fieldInfo.pTypeDesc->fieldName, var, NULL, variant_t::m_SaveInt, 1 );
			break;
		case FIELD_FLOAT:
			pSave->WriteFields( fieldInfo.pTypeDesc->fieldName, var, NULL, variant_t::m_SaveFloat, 1 );
			break;
		case FIELD_EHANDLE:
			pSave->WriteFields( fieldInfo.pTypeDesc->fieldName, var, NULL, variant_t::m_SaveEHandle, 1 );
			break;
		case FIELD_STRING:
			pSave->WriteFields( fieldInfo.pTypeDesc->fieldName, var, NULL, variant_t::m_SaveString, 1 );
			break;
		case FIELD_COLOR32:
			pSave->WriteFields( fieldInfo.pTypeDesc->fieldName, var, NULL, variant_t::m_SaveColor, 1 );
			break;
		case FIELD_VECTOR:
		{
			variant_savevector_t Temp;
			var->Vector3D(Temp.vecSave);
			pSave->WriteFields( fieldInfo.pTypeDesc->fieldName, &Temp, NULL, variant_t::m_SaveVector, 1 );
			break;
		}

		case FIELD_POSITION_VECTOR:
		{
			variant_savevector_t Temp;
			var->Vector3D(Temp.vecSave);
			pSave->WriteFields( fieldInfo.pTypeDesc->fieldName, &Temp, NULL, variant_t::m_SavePositionVector, 1 );
			break;
		}

		default:
			Warning( "Bad type %d in saved variant_t\n", var->FieldType() );
			Assert(0);
		}
	}

	// restores a single instance of the variable
	virtual void Restore( const SaveRestoreFieldInfo_t &fieldInfo, IRestore *pRestore )
	{
		variant_t *var = (variant_t*)fieldInfo.pField;

		*var = variant_t();

		var->fieldType = (_fieldtypes)pRestore->ReadInt();

		switch ( var->fieldType )
		{
		case FIELD_VOID:
			break;
		case FIELD_BOOLEAN:	
			pRestore->ReadFields( fieldInfo.pTypeDesc->fieldName, var, NULL, variant_t::m_SaveBool, 1 );
			break;
		case FIELD_INTEGER:	
			pRestore->ReadFields( fieldInfo.pTypeDesc->fieldName, var, NULL, variant_t::m_SaveInt, 1 );
			break;
		case FIELD_FLOAT:
			pRestore->ReadFields( fieldInfo.pTypeDesc->fieldName, var, NULL, variant_t::m_SaveFloat, 1 );
			break;
		case FIELD_EHANDLE:
			pRestore->ReadFields( fieldInfo.pTypeDesc->fieldName, var, NULL, variant_t::m_SaveEHandle, 1 );
			break;
		case FIELD_STRING:
			pRestore->ReadFields( fieldInfo.pTypeDesc->fieldName, var, NULL, variant_t::m_SaveString, 1 );
			break;
		case FIELD_COLOR32:
			pRestore->ReadFields( fieldInfo.pTypeDesc->fieldName, var, NULL, variant_t::m_SaveColor, 1 );
			break;
		case FIELD_VECTOR:
		{
			variant_savevector_t Temp;
			pRestore->ReadFields( fieldInfo.pTypeDesc->fieldName, &Temp, NULL, variant_t::m_SaveVector, 1 );
			var->SetVector3D(Temp.vecSave);
			break;
		}
		case FIELD_POSITION_VECTOR:
		{
			variant_savevector_t Temp;
			pRestore->ReadFields( fieldInfo.pTypeDesc->fieldName, &Temp, NULL, variant_t::m_SavePositionVector, 1 );
			var->SetPositionVector3D(Temp.vecSave);
			break;
		}
		default:
			Warning( "Bad type %d in saved variant_t\n", var->FieldType() );
			Assert(0);
			break;
		}
	}


	virtual bool IsEmpty( const SaveRestoreFieldInfo_t &fieldInfo )
	{
		// check all the elements of the array (usually only 1)
		variant_t *var = (variant_t*)fieldInfo.pField;
		for ( int i = 0; i < fieldInfo.pTypeDesc->fieldSize; i++, var++ )
		{
			if ( var->FieldType() != FIELD_VOID )
				return 0;
		}

		// variant has no data
		return 1;
	}

	virtual void MakeEmpty( const SaveRestoreFieldInfo_t &fieldInfo )
	{
		// Don't no how to. This is okay, since objects of this type
		// are always born clean before restore, and not reused
	}
};

CVariantSaveDataOps g_VariantSaveDataOps;
ISaveRestoreOps *variantFuncs = &g_VariantSaveDataOps;

/////////////////////// entitylist /////////////////////

CUtlMemoryPool g_EntListMemPool( sizeof(entitem_t), 256, CUtlMemoryPool::GROW_NONE, "g_EntListMemPool" );

#include "tier0/memdbgoff.h"

void *entitem_t::operator new( size_t stAllocateBlock )
{
	return g_EntListMemPool.Alloc( stAllocateBlock );
}

void *entitem_t::operator new( size_t stAllocateBlock, int nBlockUse, const char *pFileName, int nLine )
{
	return g_EntListMemPool.Alloc( stAllocateBlock );
}

void entitem_t::operator delete( void *pMem )
{
	g_EntListMemPool.Free( pMem );
}

#include "tier0/memdbgon.h"


CEntityList::CEntityList()
{
	m_pItemList = NULL;
	m_iNumItems = 0;
}

CEntityList::~CEntityList()
{
	// remove all items from the list
	entitem_t *next, *e = m_pItemList;
	while ( e != NULL )
	{
		next = e->pNext;
		delete e;
		e = next;
	}
	m_pItemList = NULL;
}

void CEntityList::AddEntity( CBaseEntity *pEnt )
{
	// check if it's already in the list; if not, add it
	entitem_t *e = m_pItemList;
	while ( e != NULL )
	{
		if ( e->hEnt == pEnt )
		{
			// it's already in the list
			return;
		}

		if ( e->pNext == NULL )
		{
			// we've hit the end of the list, so tack it on
			e->pNext = new entitem_t;
			e->pNext->hEnt = pEnt;
			e->pNext->pNext = NULL;
			m_iNumItems++;
			return;
		}

		e = e->pNext;
	}
	
	// empty list
	m_pItemList = new entitem_t;
	m_pItemList->hEnt = pEnt;
	m_pItemList->pNext = NULL;
	m_iNumItems = 1;
}

void CEntityList::DeleteEntity( CBaseEntity *pEnt )
{
	// find the entry in the list and delete it
	entitem_t *prev = NULL, *e = m_pItemList;
	while ( e != NULL )
	{
		// delete the link if it's the matching entity OR if the link is NULL
		if ( e->hEnt == pEnt || e->hEnt == NULL )
		{
			if ( prev )
			{
				prev->pNext = e->pNext;
			}
			else
			{
				m_pItemList = e->pNext;
			}

			delete e;
			m_iNumItems--;

			// REVISIT: Is this correct?  Is this just here to clean out dead EHANDLEs?
			// restart the loop
			e = m_pItemList;
			prev = NULL;
			continue;
		}

		prev = e;
		e = e->pNext;
	}
}

// HL1

void EntvarsKeyvalue( entvars_t *pev, KeyValueData *pkvd );

extern "C" void PM_Move ( struct playermove_s *ppmove, int server );
extern "C" void PM_Init ( struct playermove_s *ppmove  );
extern "C" char PM_FindTextureType( char *name );

extern VectorHL1 VecBModelOriginHL1( entvars_t* pevBModel );
extern DLL_GLOBAL VectorHL1		g_vecAttackDir;
extern DLL_GLOBAL int			g_iSkillLevel;

static DLL_FUNCTIONS gFunctionTable = 
{
	GameDLLInit,				//pfnGameInit
	DispatchSpawn,				//pfnSpawn
	DispatchThink,				//pfnThink
	DispatchUse,				//pfnUse
	DispatchTouch,				//pfnTouch
	DispatchBlocked,			//pfnBlocked
	DispatchKeyValue,			//pfnKeyValue
	DispatchSave,				//pfnSave
	DispatchRestore,			//pfnRestore
	DispatchObjectCollsionBox,	//pfnAbsBox

	SaveWriteFields,			//pfnSaveWriteFields
	SaveReadFields,				//pfnSaveReadFields

	SaveGlobalState,			//pfnSaveGlobalState
	RestoreGlobalState,			//pfnRestoreGlobalState
	ResetGlobalState,			//pfnResetGlobalState

	ClientConnect,				//pfnClientConnect
	ClientDisconnect,			//pfnClientDisconnect
	ClientKill,					//pfnClientKill
	ClientPutInServer,			//pfnClientPutInServer
	ClientCommand,				//pfnClientCommand
	ClientUserInfoChanged,		//pfnClientUserInfoChanged
	ServerActivate,				//pfnServerActivate
	ServerDeactivate,			//pfnServerDeactivate

	PlayerPreThink,				//pfnPlayerPreThink
	PlayerPostThink,			//pfnPlayerPostThink

	StartFrame,					//pfnStartFrame
	ParmsNewLevel,				//pfnParmsNewLevel
	ParmsChangeLevel,			//pfnParmsChangeLevel

	GetGameDescription,         //pfnGetGameDescription    Returns string describing current .dll game.
	PlayerCustomization,        //pfnPlayerCustomization   Notifies .dll of new customization for player.

	SpectatorConnect,			//pfnSpectatorConnect      Called when spectator joins server
	SpectatorDisconnect,        //pfnSpectatorDisconnect   Called when spectator leaves the server
	SpectatorThink,				//pfnSpectatorThink        Called when spectator sends a command packet (usercmd_t)

	Sys_Error,					//pfnSys_Error				Called when engine has encountered an error

	PM_Move,					//pfnPM_Move
	PM_Init,					//pfnPM_Init				Server version of player movement initialization
	PM_FindTextureType,			//pfnPM_FindTextureType
	
	SetupVisibility,			//pfnSetupVisibility        Set up PVS and PAS for networking for this client
	UpdateClientData,			//pfnUpdateClientData       Set up data sent only to specific client
	AddToFullPack,				//pfnAddToFullPack
	CreateBaseline,				//pfnCreateBaseline			Tweak entity baseline for network encoding, allows setup of player baselines, too.
	RegisterEncoders,			//pfnRegisterEncoders		Callbacks for network encoding
	GetWeaponData,				//pfnGetWeaponData
	CmdStart,					//pfnCmdStart
	CmdEnd,						//pfnCmdEnd
	ConnectionlessPacket,		//pfnConnectionlessPacket
	GetHullBounds,				//pfnGetHullBounds
	CreateInstancedBaselines,   //pfnCreateInstancedBaselines
	InconsistentFile,			//pfnInconsistentFile
	AllowLagCompensation,		//pfnAllowLagCompensation
};

static void SetObjectCollisionBox( entvars_t *pev );

#ifndef _WIN32
extern "C" {
#endif
int GetEntityAPI( DLL_FUNCTIONS *pFunctionTable, int interfaceVersion )
{
	if ( !pFunctionTable || interfaceVersion != INTERFACE_VERSION )
	{
		return FALSE;
	}
	
	memcpy( pFunctionTable, &gFunctionTable, sizeof( DLL_FUNCTIONS ) );
	return TRUE;
}

int GetEntityAPI2( DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion )
{
	if ( !pFunctionTable || *interfaceVersion != INTERFACE_VERSION )
	{
		// Tell engine what version we had, so it can figure out who is out of date.
		*interfaceVersion = INTERFACE_VERSION;
		return FALSE;
	}
	
	memcpy( pFunctionTable, &gFunctionTable, sizeof( DLL_FUNCTIONS ) );
	return TRUE;
}

#ifndef _WIN32
}
#endif


int DispatchSpawn( edict_t_hl1 *pent )
{
	CBaseEntityHL1 *pEntity = (CBaseEntityHL1 *)GET_PRIVATE(pent);

	if (pEntity)
	{
		// Initialize these or entities who don't link to the world won't have anything in here
		pEntity->pev->absmin = pEntity->pev->origin - VectorHL1(1,1,1);
		pEntity->pev->absmax = pEntity->pev->origin + VectorHL1(1,1,1);

		pEntity->Spawn();

		// Try to get the pointer again, in case the spawn function deleted the entity.
		// UNDONE: Spawn() should really return a code to ask that the entity be deleted, but
		// that would touch too much code for me to do that right now.
		pEntity = (CBaseEntityHL1 *)GET_PRIVATE(pent);

		if ( pEntity )
		{
			if ( g_pGameRules && !g_pGameRules->IsAllowedToSpawn( pEntity ) )
				return -1;	// return that this entity should be deleted
			if ( pEntity->pev->flags & FL_KILLME )
				return -1;
		}


		// Handle global stuff here
		if ( pEntity && pEntity->pev->globalname ) 
		{
			const globalentity_t *pGlobal = gGlobalState.EntityFromTable( pEntity->pev->globalname );
			if ( pGlobal )
			{
				// Already dead? delete
				if ( pGlobal->state == GLOBAL_DEAD )
					return -1;
				else if ( !FStrEq( STRING_HL1(gpGlobalsHL1->mapname), pGlobal->levelName ) )
					pEntity->MakeDormant();	// Hasn't been moved to this level yet, wait but stay alive
				// In this level & not dead, continue on as normal
			}
			else
			{
				// Spawned entities default to 'On'
				gGlobalState.EntityAdd( pEntity->pev->globalname, gpGlobalsHL1->mapname, GLOBAL_ON );
//				ALERT( at_console, "Added global entity %s (%s)\n", STRING_HL1(pEntity->pev->classname), STRING_HL1(pEntity->pev->globalname) );
			}
		}

	}

	return 0;
}

void DispatchKeyValue( edict_t_hl1 *pentKeyvalue, KeyValueData *pkvd )
{
	if ( !pkvd || !pentKeyvalue )
		return;

	EntvarsKeyvalue( VARS(pentKeyvalue), pkvd );

	// If the key was an entity variable, or there's no class set yet, don't look for the object, it may
	// not exist yet.
	if ( pkvd->fHandled || pkvd->szClassName == NULL )
		return;

	// Get the actualy entity object
	CBaseEntityHL1 *pEntity = (CBaseEntityHL1 *)GET_PRIVATE(pentKeyvalue);

	if ( !pEntity )
		return;

	pEntity->KeyValue( pkvd );
}


// HACKHACK -- this is a hack to keep the node graph entity from "touching" things (like triggers)
// while it builds the graph
BOOL gTouchDisabled = FALSE;
void DispatchTouch( edict_t_hl1 *pentTouched, edict_t_hl1 *pentOther )
{
	if ( gTouchDisabled )
		return;

	CBaseEntityHL1 *pEntity = (CBaseEntityHL1 *)GET_PRIVATE(pentTouched);
	CBaseEntityHL1 *pOther = (CBaseEntityHL1 *)GET_PRIVATE( pentOther );

	if ( pEntity && pOther && ! ((pEntity->pev->flags | pOther->pev->flags) & FL_KILLME) )
		pEntity->Touch( pOther );
}


void DispatchUse( edict_t_hl1 *pentUsed, edict_t_hl1 *pentOther )
{
	CBaseEntityHL1 *pEntity = (CBaseEntityHL1 *)GET_PRIVATE(pentUsed);
	CBaseEntityHL1 *pOther = (CBaseEntityHL1 *)GET_PRIVATE(pentOther);

	if (pEntity && !(pEntity->pev->flags & FL_KILLME) )
		pEntity->Use( pOther, pOther, USE_TOGGLE, 0 );
}

void DispatchThink( edict_t_hl1 *pent )
{
	CBaseEntityHL1 *pEntity = (CBaseEntityHL1 *)GET_PRIVATE(pent);
	if (pEntity)
	{
		if ( FBitSet( pEntity->pev->flags, FL_DORMANT ) )
			ALERT( at_error, "Dormant entity %s is thinking!!\n", STRING_HL1(pEntity->pev->classname) );
				
		pEntity->Think();
	}
}

void DispatchBlocked( edict_t_hl1 *pentBlocked, edict_t_hl1 *pentOther )
{
	CBaseEntityHL1 *pEntity = (CBaseEntityHL1 *)GET_PRIVATE( pentBlocked );
	CBaseEntityHL1 *pOther = (CBaseEntityHL1 *)GET_PRIVATE( pentOther );

	if (pEntity)
		pEntity->Blocked( pOther );
}

void DispatchSave( edict_t_hl1 *pent, SAVERESTOREDATA *pSaveData )
{
	CBaseEntityHL1 *pEntity = (CBaseEntityHL1 *)GET_PRIVATE(pent);
	
	if ( pEntity && pSaveData )
	{
		ENTITYTABLE *pTable = &pSaveData->pTable[ pSaveData->currentIndex ];

		if ( pTable->pent != pent )
			ALERT( at_error, "ENTITY TABLE OR INDEX IS WRONG!!!!\n" );

		if ( pEntity->ObjectCaps() & FCAP_DONT_SAVE )
			return;

		// These don't use ltime & nextthink as times really, but we'll fudge around it.
		if ( pEntity->pev->movetype == MOVETYPE_PUSH )
		{
			float delta = pEntity->pev->nextthink - pEntity->pev->ltime;
			pEntity->pev->ltime = gpGlobalsHL1->time;
			pEntity->pev->nextthink = pEntity->pev->ltime + delta;
		}

		pTable->location = pSaveData->size;		// Remember entity position for file I/O
		pTable->classname = pEntity->pev->classname;	// Remember entity class for respawn

		CSaveHL1 saveHelper( pSaveData );
		pEntity->SaveHL1( saveHelper );

		pTable->size = pSaveData->size - pTable->location;	// Size of entity block is data size written to block
	}
}


// Find the matching global entity.  Spit out an error if the designer made entities of
// different classes with the same global name
CBaseEntityHL1 *FindGlobalEntity( string_t_hl1 classname, string_t_hl1 globalname )
{
	edict_t_hl1 *pent = FIND_ENTITY_BY_STRING( NULL, "globalname", STRING_HL1(globalname) );
	CBaseEntityHL1 *pReturn = CBaseEntityHL1::Instance( pent );
	if ( pReturn )
	{
		if ( !FClassnameIs( pReturn->pev, STRING_HL1(classname) ) )
		{
			ALERT( at_console, "Global entity found %s, wrong class %s\n", STRING_HL1(globalname), STRING_HL1(pReturn->pev->classname) );
			pReturn = NULL;
		}
	}

	return pReturn;
}


int DispatchRestore( edict_t_hl1 *pent, SAVERESTOREDATA *pSaveData, int globalEntity )
{
	CBaseEntityHL1 *pEntity = (CBaseEntityHL1 *)GET_PRIVATE(pent);

	if ( pEntity && pSaveData )
	{
		entvars_t tmpVars;
		VectorHL1 oldOffset;

		CRestoreHL1 restoreHelper( pSaveData );
		if ( globalEntity )
		{
			CRestoreHL1 tmpRestore( pSaveData );
			tmpRestore.PrecacheMode( 0 );
			tmpRestore.ReadEntVars( "ENTVARS", &tmpVars );

			// HACKHACK - reset the save pointers, we're going to restore for real this time
			pSaveData->size = pSaveData->pTable[pSaveData->currentIndex].location;
			pSaveData->pCurrentData = pSaveData->pBaseData + pSaveData->size;
			// -------------------


			const globalentity_t *pGlobal = gGlobalState.EntityFromTable( tmpVars.globalname );
			
			// Don't overlay any instance of the global that isn't the latest
			// pSaveData->szCurrentMapName is the level this entity is coming from
			// pGlobla->levelName is the last level the global entity was active in.
			// If they aren't the same, then this global update is out of date.
			if ( !FStrEq( pSaveData->szCurrentMapName, pGlobal->levelName ) )
				return 0;

			// Compute the new global offset
			oldOffset = pSaveData->vecLandmarkOffset;
			CBaseEntityHL1 *pNewEntity = FindGlobalEntity( tmpVars.classname, tmpVars.globalname );
			if ( pNewEntity )
			{
//				ALERT( at_console, "Overlay %s with %s\n", STRING_HL1(pNewEntity->pev->classname), STRING_HL1(tmpVars.classname) );
				// Tell the restore code we're overlaying a global entity from another level
				restoreHelper.SetGlobalMode( 1 );	// Don't overwrite global fields
				pSaveData->vecLandmarkOffset = (pSaveData->vecLandmarkOffset - pNewEntity->pev->mins) + tmpVars.mins;
				pEntity = pNewEntity;// we're going to restore this data OVER the old entity
				pent = ENT( pEntity->pev );
				// Update the global table to say that the global definition of this entity should come from this level
				gGlobalState.EntityUpdate( pEntity->pev->globalname, gpGlobalsHL1->mapname );
			}
			else
			{
				// This entity will be freed automatically by the engine.  If we don't do a restore on a matching entity (below)
				// or call EntityUpdate() to move it to this level, we haven't changed global state at all.
				return 0;
			}

		}

		if ( pEntity->ObjectCaps() & FCAP_MUST_SPAWN )
		{
			pEntity->RestoreHL1( restoreHelper );
			pEntity->Spawn();
		}
		else
		{
			pEntity->RestoreHL1( restoreHelper );
			pEntity->Precache( );
		}

		// Again, could be deleted, get the pointer again.
		pEntity = (CBaseEntityHL1 *)GET_PRIVATE(pent);

#if 0
		if ( pEntity && pEntity->pev->globalname && globalEntity ) 
		{
			ALERT( at_console, "Global %s is %s\n", STRING_HL1(pEntity->pev->globalname), STRING_HL1(pEntity->pev->model) );
		}
#endif

		// Is this an overriding global entity (coming over the transition), or one restoring in a level
		if ( globalEntity )
		{
//			ALERT( at_console, "After: %f %f %f %s\n", pEntity->pev->origin.x, pEntity->pev->origin.y, pEntity->pev->origin.z, STRING_HL1(pEntity->pev->model) );
			pSaveData->vecLandmarkOffset = oldOffset;
			if ( pEntity )
			{
				UTIL_SetOrigin( pEntity->pev, pEntity->pev->origin );
				pEntity->OverrideReset();
			}
		}
		else if ( pEntity && pEntity->pev->globalname ) 
		{
			const globalentity_t *pGlobal = gGlobalState.EntityFromTable( pEntity->pev->globalname );
			if ( pGlobal )
			{
				// Already dead? delete
				if ( pGlobal->state == GLOBAL_DEAD )
					return -1;
				else if ( !FStrEq( STRING_HL1(gpGlobalsHL1->mapname), pGlobal->levelName ) )
				{
					pEntity->MakeDormant();	// Hasn't been moved to this level yet, wait but stay alive
				}
				// In this level & not dead, continue on as normal
			}
			else
			{
				ALERT( at_error, "Global Entity %s (%s) not in table!!!\n", STRING_HL1(pEntity->pev->globalname), STRING_HL1(pEntity->pev->classname) );
				// Spawned entities default to 'On'
				gGlobalState.EntityAdd( pEntity->pev->globalname, gpGlobalsHL1->mapname, GLOBAL_ON );
			}
		}
	}
	return 0;
}


void DispatchObjectCollsionBox( edict_t_hl1 *pent )
{
	CBaseEntityHL1 *pEntity = (CBaseEntityHL1 *)GET_PRIVATE(pent);
	if (pEntity)
	{
		pEntity->SetObjectCollisionBox();
	}
	else
		SetObjectCollisionBox( &pent->v );
}


void SaveWriteFields( SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount )
{
	CSaveHL1 saveHelper( pSaveData );
	saveHelper.WriteFields( pname, pBaseData, pFields, fieldCount );
}


void SaveReadFields( SAVERESTOREDATA *pSaveData, const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount )
{
	CRestoreHL1 restoreHelper( pSaveData );
	restoreHelper.ReadFields( pname, pBaseData, pFields, fieldCount );
}


edict_t_hl1 * EHANDLEHL1::Get( void ) 
{ 
	if (m_pent)
	{
		if (m_pent->serialnumber == m_serialnumber) 
			return m_pent; 
		else
			return NULL;
	}
	return NULL; 
};

edict_t_hl1 * EHANDLEHL1::Set( edict_t_hl1 *pent ) 
{ 
	m_pent = pent;  
	if (pent) 
		m_serialnumber = m_pent->serialnumber; 
	return pent; 
};


EHANDLEHL1 :: operator CBaseEntityHL1 *() 
{ 
	return (CBaseEntityHL1 *)GET_PRIVATE( Get( ) ); 
};


CBaseEntityHL1 * EHANDLEHL1 :: operator = (CBaseEntityHL1 *pEntity)
{
	if (pEntity)
	{
		m_pent = ENT( pEntity->pev );
		if (m_pent)
			m_serialnumber = m_pent->serialnumber;
	}
	else
	{
		m_pent = NULL;
		m_serialnumber = 0;
	}
	return pEntity;
}

EHANDLEHL1 :: operator int ()
{
	return Get() != NULL;
}

CBaseEntityHL1 * EHANDLEHL1 :: operator -> ()
{
	return (CBaseEntityHL1 *)GET_PRIVATE( Get( ) ); 
}


// give health
int CBaseEntityHL1 :: TakeHealth( float flHealth, int bitsDamageType )
{
	if (!pev->takedamage)
		return 0;

// heal
	if ( pev->health >= pev->max_health )
		return 0;

	pev->health += flHealth;

	if (pev->health > pev->max_health)
		pev->health = pev->max_health;

	return 1;
}

// inflict damage on this entity.  bitsDamageType indicates type of damage inflicted, ie: DMG_CRUSH

int CBaseEntityHL1 :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	VectorHL1		vecTemp;

	if (!pev->takedamage)
		return 0;

	// UNDONE: some entity types may be immune or resistant to some bitsDamageType
	
	// if Attacker == Inflictor, the attack was a melee or other instant-hit attack.
	// (that is, no actual entity projectile was involved in the attack so use the shooter's origin). 
	if ( pevAttacker == pevInflictor )	
	{
		vecTemp = pevInflictor->origin - ( VecBModelOrigin(pev) );
	}
	else
	// an actual missile was involved.
	{
		vecTemp = pevInflictor->origin - ( VecBModelOrigin(pev) );
	}

// this global is still used for glass and other non-monster killables, along with decals.
	g_vecAttackDir = vecTemp.Normalize();
		
// save damage based on the target's armor level

// figure momentum add (don't let hurt brushes or other triggers move player)
	if ((!FNullEnt(pevInflictor)) && (pev->movetype == MOVETYPE_WALK || pev->movetype == MOVETYPE_STEP) && (pevAttacker->solid != SOLID_TRIGGER) )
	{
		VectorHL1 vecDir = pev->origin - (pevInflictor->absmin + pevInflictor->absmax) * 0.5;
		vecDir = vecDir.Normalize();

		float flForce = flDamage * ((32 * 32 * 72.0) / (pev->size.x * pev->size.y * pev->size.z)) * 5;
		
		if (flForce > 1000.0) 
			flForce = 1000.0;
		pev->velocity = pev->velocity + vecDir * flForce;
	}

// do the damage
	pev->health -= flDamage;
	if (pev->health <= 0)
	{
		Killed( pevAttacker, GIB_NORMAL );
		return 0;
	}

	return 1;
}


void CBaseEntityHL1 :: Killed( entvars_t *pevAttacker, int iGib )
{
	pev->takedamage = DAMAGE_NO;
	pev->deadflag = DEAD_DEAD;
	UTIL_Remove( this );
}


CBaseEntityHL1 *CBaseEntityHL1::GetNextTarget( void )
{
	if ( FStringNull( pev->target ) )
		return NULL;
	edict_t_hl1 *pTarget = FIND_ENTITY_BY_TARGETNAME ( NULL, STRING_HL1(pev->target) );
	if ( FNullEnt(pTarget) )
		return NULL;

	return Instance( pTarget );
}

// Global Savedata for Delay
TYPEDESCRIPTION	CBaseEntityHL1::m_SaveData[] = 
{
	DEFINE_FIELD( CBaseEntityHL1, m_pGoalEnt, FIELD_CLASSPTR ),

	DEFINE_FIELD( CBaseEntityHL1, m_pfnThink, FIELD_FUNCTION ),		// UNDONE: Build table of these!!!
	DEFINE_FIELD( CBaseEntityHL1, m_pfnTouch, FIELD_FUNCTION ),
	DEFINE_FIELD( CBaseEntityHL1, m_pfnUse, FIELD_FUNCTION ),
	DEFINE_FIELD( CBaseEntityHL1, m_pfnBlocked, FIELD_FUNCTION ),
};


int CBaseEntityHL1::SaveHL1( CSaveHL1 &save )
{
	if ( save.WriteEntVars( "ENTVARS", pev ) )
		return save.WriteFields( "BASE", this, m_SaveData, ARRAYSIZE(m_SaveData) );

	return 0;
}

int CBaseEntityHL1::RestoreHL1( CRestoreHL1 &restore )
{
	int status;

	status = restore.ReadEntVars( "ENTVARS", pev );
	if ( status )
		status = restore.ReadFields( "BASE", this, m_SaveData, ARRAYSIZE(m_SaveData) );

    if ( pev->modelindex != 0 && !FStringNull(pev->model) )
	{
		VectorHL1 mins, maxs;
		mins = pev->mins;	// Set model is about to destroy these
		maxs = pev->maxs;


		PRECACHE_MODEL( (char *)STRING_HL1(pev->model) );
		SET_MODEL(ENT(pev), STRING_HL1(pev->model));
		UTIL_SetSizeHL1(pev, mins, maxs);	// Reset them
	}

	return status;
}


// Initialize absmin & absmax to the appropriate box
void SetObjectCollisionBox( entvars_t *pev )
{
	if ( (pev->solid == SOLID_BSP) && 
		 (pev->angles.x || pev->angles.y|| pev->angles.z) )
	{	// expand for rotation
		float		max, v;
		int			i;

		max = 0;
		for (i=0 ; i<3 ; i++)
		{
			v = fabs( ((float *)pev->mins)[i]);
			if (v > max)
				max = v;
			v = fabs( ((float *)pev->maxs)[i]);
			if (v > max)
				max = v;
		}
		for (i=0 ; i<3 ; i++)
		{
			((float *)pev->absmin)[i] = ((float *)pev->origin)[i] - max;
			((float *)pev->absmax)[i] = ((float *)pev->origin)[i] + max;
		}
	}
	else
	{
		pev->absmin = pev->origin + pev->mins;
		pev->absmax = pev->origin + pev->maxs;
	}

	pev->absmin.x -= 1;
	pev->absmin.y -= 1;
	pev->absmin.z -= 1;
	pev->absmax.x += 1;
	pev->absmax.y += 1;
	pev->absmax.z += 1;
}


void CBaseEntityHL1::SetObjectCollisionBox( void )
{
	::SetObjectCollisionBox( pev );
}


int	CBaseEntityHL1 :: Intersects( CBaseEntityHL1 *pOther )
{
	if ( pOther->pev->absmin.x > pev->absmax.x ||
		 pOther->pev->absmin.y > pev->absmax.y ||
		 pOther->pev->absmin.z > pev->absmax.z ||
		 pOther->pev->absmax.x < pev->absmin.x ||
		 pOther->pev->absmax.y < pev->absmin.y ||
		 pOther->pev->absmax.z < pev->absmin.z )
		 return 0;
	return 1;
}

void CBaseEntityHL1 :: MakeDormant( void )
{
	SetBits( pev->flags, FL_DORMANT );
	
	// Don't touch
	pev->solid = SOLID_NOT;
	// Don't move
	pev->movetype = MOVETYPE_NONE;
	// Don't draw
	SetBits( pev->effects, EF_NODRAW );
	// Don't think
	pev->nextthink = 0;
	// Relink
	UTIL_SetOrigin( pev, pev->origin );
}

int CBaseEntityHL1 :: IsDormant( void )
{
	return FBitSet( pev->flags, FL_DORMANT );
}

BOOL CBaseEntityHL1 :: IsInWorld( void )
{
	// position 
	if (pev->origin.x >= 4096) return FALSE;
	if (pev->origin.y >= 4096) return FALSE;
	if (pev->origin.z >= 4096) return FALSE;
	if (pev->origin.x <= -4096) return FALSE;
	if (pev->origin.y <= -4096) return FALSE;
	if (pev->origin.z <= -4096) return FALSE;
	// speed
	if (pev->velocity.x >= 2000) return FALSE;
	if (pev->velocity.y >= 2000) return FALSE;
	if (pev->velocity.z >= 2000) return FALSE;
	if (pev->velocity.x <= -2000) return FALSE;
	if (pev->velocity.y <= -2000) return FALSE;
	if (pev->velocity.z <= -2000) return FALSE;

	return TRUE;
}

int CBaseEntityHL1::ShouldToggle( USE_TYPE useType, BOOL currentState )
{
	if ( useType != USE_TOGGLE && useType != USE_SET )
	{
		if ( (currentState && useType == USE_ON) || (!currentState && useType == USE_OFF) )
			return 0;
	}
	return 1;
}


int	CBaseEntityHL1 :: DamageDecal( int bitsDamageType )
{
	if ( pev->rendermode == kRenderTransAlpha )
		return -1;

	if ( pev->rendermode != kRenderNormal )
		return DECAL_BPROOF1;

	return DECAL_GUNSHOT1 + RANDOM_LONG(0,4);
}



// NOTE: szName must be a pointer to constant memory, e.g. "monster_class" because the entity
// will keep a pointer to it after this call.
CBaseEntityHL1 * CBaseEntityHL1::Create( char *szName, const VectorHL1 &vecOrigin, const VectorHL1 &vecAngles, edict_t_hl1 *pentOwner )
{
	edict_t_hl1	*pent;
	CBaseEntityHL1 *pEntity;

	pent = CREATE_NAMED_ENTITY( MAKE_STRING_HL1( szName ));
	if ( FNullEnt( pent ) )
	{
		ALERT ( at_console, "NULL Ent in Create!\n" );
		return NULL;
	}
	pEntity = Instance( pent );
	pEntity->pev->owner = pentOwner;
	pEntity->pev->origin = vecOrigin;
	pEntity->pev->angles = vecAngles;
	DispatchSpawn( pEntity->edict() );
	return pEntity;
}

