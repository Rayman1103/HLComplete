//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Helper classes and functions for the save/restore system. These
// 			classes are internally structured to distinguish simple from
//			complex types.
//
// $NoKeywords: $
//=============================================================================//

#ifndef SAVERESTORE_H
#define SAVERESTORE_H

#include "isaverestore.h"
#include "utlvector.h"
#include "filesystem.h"

#ifdef _WIN32
#pragma once
#endif

//-------------------------------------

class CSaveRestoreData;
class CSaveRestoreSegment;
class CGameSaveRestoreInfo;
struct typedescription_t;
struct edict_t;
struct datamap_t;
class CBaseEntity;
struct interval_t;

//-----------------------------------------------------------------------------
//
// CSave
//
//-----------------------------------------------------------------------------

class CSave : public ISave
{
public:
	CSave( CSaveRestoreData *pdata );
	
	//---------------------------------
	// Logging
	void			StartLogging( const char *pszLogName );
	void			EndLogging( void );

	//---------------------------------
	bool			IsAsync();

	//---------------------------------

	int				GetWritePos() const;
	void			SetWritePos(int pos);

	//---------------------------------
	// Datamap based writing
	//
	
	int				WriteAll( const void *pLeafObject, datamap_t *pLeafMap )	{ return DoWriteAll( pLeafObject, pLeafMap, pLeafMap ); }
	
	int				WriteFields( const char *pname, const void *pBaseData, datamap_t *pMap, typedescription_t *pFields, int fieldCount );

	//---------------------------------
	// Block support
	//
	
	virtual void	StartBlock( const char *pszBlockName );
	virtual void	StartBlock();
	virtual void	EndBlock();
	
	//---------------------------------
	// Primitive types
	//
	
	void			WriteShort( const short *value, int count = 1 );
	void			WriteInt( const int *value, int count = 1 );		                           // Save an int
	void			WriteBool( const bool *value, int count = 1 );		                           // Save a bool
	void			WriteFloat( const float *value, int count = 1 );	                           // Save a float
	void			WriteData( const char *pdata, int size );		                               // Save a binary data block
	void			WriteString( const char *pstring );			                                   // Save a null-terminated string
	void			WriteString( const string_t *stringId, int count = 1 );	                       // Save a null-terminated string (engine string)
	void			WriteVector( const Vector &value );				                               // Save a vector
	void			WriteVector( const Vector *value, int count = 1 );	                           // Save a vector array
	void			WriteQuaternion( const Quaternion &value );				                        // Save a Quaternion
	void			WriteQuaternion( const Quaternion *value, int count = 1 );	                    // Save a Quaternion array
	void			WriteVMatrix( const VMatrix *value, int count = 1 );							// Save a vmatrix array

	// Note: All of the following will write out both a header and the data. On restore,
	// this needs to be cracked
	void			WriteShort( const char *pname, const short *value, int count = 1 );
	void			WriteInt( const char *pname, const int *value, int count = 1 );		           // Save an int
	void			WriteBool( const char *pname, const bool *value, int count = 1 );		       // Save a bool
	void			WriteFloat( const char *pname, const float *value, int count = 1 );	           // Save a float
	void			WriteData( const char *pname, int size, const char *pdata );		           // Save a binary data block
	void			WriteString( const char *pname, const char *pstring );			               // Save a null-terminated string
	void			WriteString( const char *pname, const string_t *stringId, int count = 1 );	   // Save a null-terminated string (engine string)
	void			WriteVector( const char *pname, const Vector &value );				           // Save a vector
	void			WriteVector( const char *pname, const Vector *value, int count = 1 );	       // Save a vector array
	void			WriteQuaternion( const char *pname, const Quaternion &value );				   // Save a Quaternion
	void			WriteQuaternion( const char *pname, const Quaternion *value, int count = 1 );  // Save a Quaternion array
	void			WriteVMatrix( const char *pname, const VMatrix *value, int count = 1 );
	//---------------------------------
	// Game types
	//
	
	void			WriteTime( const char *pname, const float *value, int count = 1 );	           // Save a float (timevalue)
	void			WriteTick( const char *pname, const int *value, int count = 1 );	           // Save a int (timevalue)
	void			WritePositionVector( const char *pname, const Vector &value );		           // Offset for landmark if necessary
	void			WritePositionVector( const char *pname, const Vector *value, int count = 1 );  // array of pos vectors
	void			WriteFunction( datamap_t *pMap, const char *pname, inputfunc_t **value, int count = 1 ); // Save a function pointer

	void			WriteEntityPtr( const char *pname, CBaseEntity **ppEntity, int count = 1 );
	void			WriteEdictPtr( const char *pname, edict_t **ppEdict, int count = 1 );
	void			WriteEHandle( const char *pname, const EHANDLE *pEHandle, int count = 1 );

	virtual void	WriteTime( const float *value, int count = 1 );	// Save a float (timevalue)
	virtual void	WriteTick( const int *value, int count = 1 );	// Save a int (timevalue)
	virtual void	WritePositionVector( const Vector &value );		// Offset for landmark if necessary
	virtual void	WritePositionVector( const Vector *value, int count = 1 );	// array of pos vectors

	virtual void	WriteEntityPtr( CBaseEntity **ppEntity, int count = 1 );
	virtual void	WriteEdictPtr( edict_t **ppEdict, int count = 1 );
	virtual void	WriteEHandle( const EHANDLE *pEHandle, int count = 1 );
	void			WriteVMatrixWorldspace( const char *pname, const VMatrix *value, int count = 1 );	       // Save a vmatrix array
	void			WriteVMatrixWorldspace( const VMatrix *value, int count = 1 );	       // Save a vmatrix array
	void			WriteMatrix3x4Worldspace( const matrix3x4_t *value, int count );
	void			WriteMatrix3x4Worldspace( const char *pname, const matrix3x4_t *value, int count );

	void			WriteInterval( const interval_t *value, int count = 1 );						// Save an interval
	void			WriteInterval( const char *pname, const interval_t *value, int count = 1 );

	//---------------------------------
	
	int				EntityIndex( const CBaseEntity *pEntity );
	int				EntityFlagsSet( int entityIndex, int flags );

	CGameSaveRestoreInfo *GetGameSaveRestoreInfo()	{ return m_pGameInfo; }

private:

	//---------------------------------
	bool			IsLogging( void );
	void			Log( const char *pName, fieldtype_t fieldType, void *value, int count );

	//---------------------------------
	
	void			BufferField( const char *pname, int size, const char *pdata );
	void			BufferData( const char *pdata, int size );
	void			WriteHeader( const char *pname, int size );

	int				DoWriteAll( const void *pLeafObject, datamap_t *pLeafMap, datamap_t *pCurMap );
	bool 			WriteField( const char *pname, void *pData, datamap_t *pRootMap, typedescription_t *pField );
	
	bool 			WriteBasicField( const char *pname, void *pData, datamap_t *pRootMap, typedescription_t *pField );
	
	int				DataEmpty( const char *pdata, int size );
	void			BufferString( char *pdata, int len );

	int				CountFieldsToSave( const void *pBaseData, typedescription_t *pFields, int fieldCount );
	bool			ShouldSaveField( const void *pData, typedescription_t *pField );

	//---------------------------------
	// Game info methods
	//
	
	bool			WriteGameField( const char *pname, void *pData, datamap_t *pRootMap, typedescription_t *pField );
	int				EntityIndex( const edict_t *pentLookup );
	
	//---------------------------------
	
	CUtlVector<int> m_BlockStartStack;
	
	// Stream data
	CSaveRestoreSegment *m_pData;
	
	// Game data
	CGameSaveRestoreInfo *m_pGameInfo;

	FileHandle_t		m_hLogFile;
	bool				m_bAsync;
};

//-----------------------------------------------------------------------------
//
// CRestore
//
//-----------------------------------------------------------------------------

class CRestore : public IRestore
{
public:
	CRestore( CSaveRestoreData *pdata );
	
	int				GetReadPos() const;
	void			SetReadPos( int pos );

	//---------------------------------
	// Datamap based reading
	//
	
	int				ReadAll( void *pLeafObject, datamap_t *pLeafMap )	{ return DoReadAll( pLeafObject, pLeafMap, pLeafMap ); }
	
	int				ReadFields( const char *pname, void *pBaseData, datamap_t *pMap, typedescription_t *pFields, int fieldCount );
	void 			EmptyFields( void *pBaseData, typedescription_t *pFields, int fieldCount );

	//---------------------------------
	// Block support
	//
	
	virtual void	StartBlock( SaveRestoreRecordHeader_t *pHeader );
	virtual void	StartBlock( char szBlockName[SIZE_BLOCK_NAME_BUF] );
	virtual void	StartBlock();
	virtual void	EndBlock();
	
	//---------------------------------
	// Field header cracking
	//
	
	void			ReadHeader( SaveRestoreRecordHeader_t *pheader );
	int				SkipHeader()	{ SaveRestoreRecordHeader_t header; ReadHeader( &header ); return header.size; }
	const char *	StringFromHeaderSymbol( int symbol );

	//---------------------------------
	// Primitive types
	//
	
	short			ReadShort( void );
	int				ReadShort( short *pValue, int count = 1, int nBytesAvailable = 0);
	int				ReadInt( int *pValue, int count = 1, int nBytesAvailable = 0);
	int				ReadInt( void );
	int 			ReadBool( bool *pValue, int count = 1, int nBytesAvailable = 0);
	int				ReadFloat( float *pValue, int count = 1, int nBytesAvailable = 0);
	int				ReadData( char *pData, int size, int nBytesAvailable );
	void			ReadString( char *pDest, int nSizeDest, int nBytesAvailable );			// A null-terminated string
	int				ReadString( string_t *pString, int count = 1, int nBytesAvailable = 0);
	int				ReadVector( Vector *pValue );
	int				ReadVector( Vector *pValue, int count = 1, int nBytesAvailable = 0);
	int				ReadQuaternion( Quaternion *pValue );
	int				ReadQuaternion( Quaternion *pValue, int count = 1, int nBytesAvailable = 0);
	int				ReadVMatrix( VMatrix *pValue, int count = 1, int nBytesAvailable = 0);
	
	//---------------------------------
	// Game types
	//
	
	int				ReadTime( float *pValue, int count = 1, int nBytesAvailable = 0);
	int				ReadTick( int *pValue, int count = 1, int nBytesAvailable = 0);
	int				ReadPositionVector( Vector *pValue );
	int				ReadPositionVector( Vector *pValue, int count = 1, int nBytesAvailable = 0);
	int				ReadFunction( datamap_t *pMap, inputfunc_t **pValue, int count = 1, int nBytesAvailable = 0);
	
	int 			ReadEntityPtr( CBaseEntity **ppEntity, int count = 1, int nBytesAvailable = 0 );
	int				ReadEdictPtr( edict_t **ppEdict, int count = 1, int nBytesAvailable = 0 );
	int				ReadEHandle( EHANDLE *pEHandle, int count = 1, int nBytesAvailable = 0 );
	int				ReadVMatrixWorldspace( VMatrix *pValue, int count = 1, int nBytesAvailable = 0);
	int				ReadMatrix3x4Worldspace( matrix3x4_t *pValue, int nElems = 1, int nBytesAvailable = 0 );
	int				ReadInterval( interval_t *interval, int count = 1, int nBytesAvailable = 0 );
	
	//---------------------------------
	
	void			SetGlobalMode( int global ) { m_global = global; }
	void			PrecacheMode( bool mode ) { m_precache = mode; }
	bool			GetPrecacheMode( void ) { return m_precache; }

	CGameSaveRestoreInfo *GetGameSaveRestoreInfo()	{ return m_pGameInfo; }

private:
	//---------------------------------
	// Read primitives
	//
	
	char *			BufferPointer( void );
	void			BufferSkipBytes( int bytes );
	
	int				DoReadAll( void *pLeafObject, datamap_t *pLeafMap, datamap_t *pCurMap );
	
	typedescription_t *FindField( const char *pszFieldName, typedescription_t *pFields, int fieldCount, int *pIterator );
	void			ReadField( const SaveRestoreRecordHeader_t &header, void *pDest, datamap_t *pRootMap, typedescription_t *pField );
	
	void 			ReadBasicField( const SaveRestoreRecordHeader_t &header, void *pDest, datamap_t *pRootMap, typedescription_t *pField );
	
	void			BufferReadBytes( char *pOutput, int size );

	template <typename T>
	int ReadSimple( T *pValue, int nElems, int nBytesAvailable ) // must be inline in class to keep MSVS happy
	{
		int desired = nElems * sizeof(T);
		int actual;
		
		if ( nBytesAvailable == 0 )
			actual = desired;
		else
		{
			Assert( nBytesAvailable % sizeof(T) == 0 );
			actual = MIN( desired, nBytesAvailable );
		}

		BufferReadBytes( (char *)pValue, actual );
		
		if ( actual < nBytesAvailable )
			BufferSkipBytes( nBytesAvailable - actual );
		
		return ( actual / sizeof(T) );
	}

	bool			ShouldReadField( typedescription_t *pField );
	bool 			ShouldEmptyField( typedescription_t *pField );

	//---------------------------------
	// Game info methods
	//
	CBaseEntity *	EntityFromIndex( int entityIndex );
	void 			ReadGameField( const SaveRestoreRecordHeader_t &header, void *pDest, datamap_t *pRootMap, typedescription_t *pField );
	
	//---------------------------------
	
	CUtlVector<int> m_BlockEndStack;
	
	// Stream data
	CSaveRestoreSegment *m_pData;

	// Game data
	CGameSaveRestoreInfo *	m_pGameInfo;
	int						m_global;		// Restoring a global entity?
	bool					m_precache;
};


//-----------------------------------------------------------------------------
// An interface passed into the OnSave method of all entities
//-----------------------------------------------------------------------------
abstract_class IEntitySaveUtils
{
public:
	// Adds a level transition save dependency
	virtual void AddLevelTransitionSaveDependency( CBaseEntity *pEntity1, CBaseEntity *pEntity2 ) = 0;

	// Gets the # of dependencies for a particular entity
	virtual int GetEntityDependencyCount( CBaseEntity *pEntity ) = 0;

	// Gets all dependencies for a particular entity
	virtual int GetEntityDependencies( CBaseEntity *pEntity, int nCount, CBaseEntity **ppEntList ) = 0;
};


//-----------------------------------------------------------------------------
// Singleton interface
//-----------------------------------------------------------------------------
IEntitySaveUtils *GetEntitySaveUtils();


//=============================================================================

// HL1

class CBaseEntity;

class CSaveRestoreBuffer
{
public:
	CSaveRestoreBuffer( void );
	CSaveRestoreBuffer( SAVERESTOREDATA *pdata );
	~CSaveRestoreBuffer( void );

	int			EntityIndex( entvars_t *pevLookup );
	int			EntityIndex( edict_t_hl1 *pentLookup );
	int			EntityIndex( EOFFSET eoLookup );
	int			EntityIndex( CBaseEntity *pEntity );

	int			EntityFlags( int entityIndex, int flags ) { return EntityFlagsSet( entityIndex, 0 ); }
	int			EntityFlagsSet( int entityIndex, int flags );

	edict_t_hl1		*EntityFromIndex( int entityIndex );

	unsigned short	TokenHash( const char *pszToken );

protected:
	SAVERESTOREDATA		*m_pdata;
	void		BufferRewind( int size );
	unsigned int	HashString( const char *pszToken );
};


class CSaveHL1 : public CSaveRestoreBuffer
{
public:
	CSaveHL1( SAVERESTOREDATA *pdata ) : CSaveRestoreBuffer( pdata ) {};

	void	WriteShort( const char *pname, const short *value, int count );
	void	WriteInt( const char *pname, const int *value, int count );		// Save an int
	void	WriteFloat( const char *pname, const float *value, int count );	// Save a float
	void	WriteTime( const char *pname, const float *value, int count );	// Save a float (timevalue)
	void	WriteData( const char *pname, int size, const char *pdata );		// Save a binary data block
	void	WriteString( const char *pname, const char *pstring );			// Save a null-terminated string
	void	WriteString( const char *pname, const int *stringId, int count );	// Save a null-terminated string (engine string)
	void	WriteVector( const char *pname, const VectorHL1 &value );				// Save a vector
	void	WriteVector( const char *pname, const float *value, int count );	// Save a vector
	void	WritePositionVector( const char *pname, const VectorHL1 &value );		// Offset for landmark if necessary
	void	WritePositionVector( const char *pname, const float *value, int count );	// array of pos vectors
	void	WriteFunction( const char *pname, const int *value, int count );		// Save a function pointer
	int		WriteEntVars( const char *pname, entvars_t *pev );		// Save entvars_t (entvars_t)
	int		WriteFields( const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount );

private:
	int		DataEmpty( const char *pdata, int size );
	void	BufferField( const char *pname, int size, const char *pdata );
	void	BufferString( char *pdata, int len );
	void	BufferData( const char *pdata, int size );
	void	BufferHeader( const char *pname, int size );
};

typedef struct 
{
	unsigned short		size;
	unsigned short		token;
	char				*pData;
} HEADER;

class CRestoreHL1 : public CSaveRestoreBuffer
{
public:
	CRestoreHL1( SAVERESTOREDATA *pdata ) : CSaveRestoreBuffer( pdata ) { m_global = 0; m_precache = TRUE; }
	int		ReadEntVars( const char *pname, entvars_t *pev );		// entvars_t
	int		ReadFields( const char *pname, void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount );
	int		ReadField( void *pBaseData, TYPEDESCRIPTION *pFields, int fieldCount, int startField, int size, char *pName, void *pData );
	int		ReadInt( void );
	short	ReadShort( void );
	int		ReadNamedInt( const char *pName );
	char	*ReadNamedString( const char *pName );
	int		Empty( void ) { return (m_pdata == NULL) || ((m_pdata->pCurrentData-m_pdata->pBaseData)>=m_pdata->bufferSize); }
	inline	void SetGlobalMode( int global ) { m_global = global; }
	void	PrecacheMode( BOOL mode ) { m_precache = mode; }

private:
	char	*BufferPointer( void );
	void	BufferReadBytes( char *pOutput, int size );
	void	BufferSkipBytes( int bytes );
	int		BufferSkipZString( void );
	int		BufferCheckZString( const char *string );

	void	BufferReadHeader( HEADER *pheader );

	int		m_global;		// Restoring a global entity?
	BOOL	m_precache;
};

#define MAX_ENTITYARRAY 64

//#define ARRAYSIZE(p)		(sizeof(p)/sizeof(p[0]))

#define IMPLEMENT_SAVERESTORE(derivedClass,baseClass) \
	int derivedClass::SaveHL1( CSaveHL1 &save )\
	{\
		if ( !baseClass::SaveHL1(save) )\
			return 0;\
		return save.WriteFields( #derivedClass, this, m_SaveData, ARRAYSIZE(m_SaveData) );\
	}\
	int derivedClass::RestoreHL1( CRestoreHL1 &restore )\
	{\
		if ( !baseClass::RestoreHL1(restore) )\
			return 0;\
		return restore.ReadFields( #derivedClass, this, m_SaveData, ARRAYSIZE(m_SaveData) );\
	}


typedef enum { GLOBAL_OFF = 0, GLOBAL_ON = 1, GLOBAL_DEAD = 2 } GLOBALESTATE;

typedef struct globalentity_s globalentity_t;

struct globalentity_s
{
	char			name[64];
	char			levelName[32];
	GLOBALESTATE	state;
	globalentity_t	*pNext;
};

class CGlobalState
{
public:
					CGlobalState();
	void			Reset( void );
	void			ClearStates( void );
	void			EntityAdd( string_t_hl1 globalname, string_t_hl1 mapName, GLOBALESTATE state );
	void			EntitySetState( string_t_hl1 globalname, GLOBALESTATE state );
	void			EntityUpdate( string_t_hl1 globalname, string_t_hl1 mapname );
	const globalentity_t	*EntityFromTable( string_t_hl1 globalname );
	GLOBALESTATE	EntityGetState( string_t_hl1 globalname );
	int				EntityInTable( string_t_hl1 globalname ) { return (Find( globalname ) != NULL) ? 1 : 0; }
	int				SaveHL1( CSaveHL1 &save );
	int				RestoreHL1( CRestoreHL1 &restore );
	static TYPEDESCRIPTION m_SaveData[];

//#ifdef _DEBUG
	void			DumpGlobals( void );
//#endif

private:
	globalentity_t	*Find( string_t_hl1 globalname );
	globalentity_t	*m_pList;
	int				m_listCount;
};

extern CGlobalState gGlobalState;

#endif // SAVERESTORE_H
