//========== Copyright � 2008, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#include "cbase.h"
#include "vscript_server.h"
#include "icommandline.h"
#include "tier1/utlbuffer.h"
#include "tier1/fmtstr.h"
#include "filesystem.h"
#include "eventqueue.h"
#include "characterset.h"
#include "sceneentity.h"		// for exposing scene precache function
#include "isaverestore.h"
#include "gamerules.h"
#include "particle_parse.h"
#if defined( _WIN32 ) || defined( POSIX )
#include "vscript_server_nut.h"
#endif

#ifdef DOTA_DLL
#include "dota_animation.h"
#endif

extern ScriptClassDesc_t * GetScriptDesc( CBaseEntity * );

// #define VMPROFILE 1

#ifdef VMPROFILE

#define VMPROF_START float debugStartTime = Plat_FloatTime();
#define VMPROF_SHOW( funcname, funcdesc  ) DevMsg("***VSCRIPT PROFILE***: %s %s: %6.4f milliseconds\n", (##funcname), (##funcdesc), (Plat_FloatTime() - debugStartTime)*1000.0 );

#else // !VMPROFILE

#define VMPROF_START
#define VMPROF_SHOW

#endif // VMPROFILE

ConVar script_connect_debugger_on_mapspawn( "script_connect_debugger_on_mapspawn", "0" );

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CScriptEntityIterator
{
public:
	HSCRIPT First() { return Next(NULL); }

	HSCRIPT Next( HSCRIPT hStartEntity )
	{
		return ToHScript( gEntList.NextEnt( ToEnt( hStartEntity ) ) );
	}

	HSCRIPT CreateByClassname( const char *className )
	{
		return ToHScript( CreateEntityByName( className ) );
	}

	HSCRIPT FindByClassname( HSCRIPT hStartEntity, const char *szName )
	{
		return ToHScript( gEntList.FindEntityByClassname( ToEnt( hStartEntity ), szName ) );
	}

	HSCRIPT FindByName( HSCRIPT hStartEntity, const char *szName )
	{
		return ToHScript( gEntList.FindEntityByName( ToEnt( hStartEntity ), szName ) );
	}

	HSCRIPT FindInSphere( HSCRIPT hStartEntity, const Vector &vecCenter, float flRadius )
	{
		return ToHScript( gEntList.FindEntityInSphere( ToEnt( hStartEntity ), vecCenter, flRadius ) );
	}

	HSCRIPT FindByTarget( HSCRIPT hStartEntity, const char *szName )
	{
		return ToHScript( gEntList.FindEntityByTarget( ToEnt( hStartEntity ), szName ) );
	}

	HSCRIPT FindByModel( HSCRIPT hStartEntity, const char *szModelName )
	{
		return ToHScript( gEntList.FindEntityByModel( ToEnt( hStartEntity ), szModelName ) );
	}

	HSCRIPT FindByNameNearest( const char *szName, const Vector &vecSrc, float flRadius )
	{
		return ToHScript( gEntList.FindEntityByNameNearest( szName, vecSrc, flRadius ) );
	}

	HSCRIPT FindByNameWithin( HSCRIPT hStartEntity, const char *szName, const Vector &vecSrc, float flRadius )
	{
		return ToHScript( gEntList.FindEntityByNameWithin( ToEnt( hStartEntity ), szName, vecSrc, flRadius ) );
	}

	HSCRIPT FindByClassnameNearest( const char *szName, const Vector &vecSrc, float flRadius )
	{
		return ToHScript( gEntList.FindEntityByClassnameNearest( szName, vecSrc, flRadius ) );
	}

	HSCRIPT FindByClassnameWithin( HSCRIPT hStartEntity , const char *szName, const Vector &vecSrc, float flRadius )
	{
		return ToHScript( gEntList.FindEntityByClassnameWithin( ToEnt( hStartEntity ), szName, vecSrc, flRadius ) );
	}

private:
} g_ScriptEntityIterator;

BEGIN_SCRIPTDESC_ROOT_NAMED( CScriptEntityIterator, "CEntities", SCRIPT_SINGLETON "The global list of entities" )
	DEFINE_SCRIPTFUNC( First, "Begin an iteration over the list of entities" )
	DEFINE_SCRIPTFUNC( Next, "Continue an iteration over the list of entities, providing reference to a previously found entity" )
	DEFINE_SCRIPTFUNC( CreateByClassname, "Creates an entity by classname" )
	DEFINE_SCRIPTFUNC( FindByClassname, "Find entities by class name. Pass 'null' to start an iteration, or reference to a previously found entity to continue a search"  )
	DEFINE_SCRIPTFUNC( FindByName, "Find entities by name. Pass 'null' to start an iteration, or reference to a previously found entity to continue a search"  )
	DEFINE_SCRIPTFUNC( FindInSphere, "Find entities within a radius. Pass 'null' to start an iteration, or reference to a previously found entity to continue a search"  )
	DEFINE_SCRIPTFUNC( FindByTarget, "Find entities by targetname. Pass 'null' to start an iteration, or reference to a previously found entity to continue a search"  )
	DEFINE_SCRIPTFUNC( FindByModel, "Find entities by model name. Pass 'null' to start an iteration, or reference to a previously found entity to continue a search"  )
	DEFINE_SCRIPTFUNC( FindByNameNearest, "Find entities by name nearest to a point."  )
	DEFINE_SCRIPTFUNC( FindByNameWithin, "Find entities by name within a radius. Pass 'null' to start an iteration, or reference to a previously found entity to continue a search"  )
	DEFINE_SCRIPTFUNC( FindByClassnameNearest, "Find entities by class name nearest to a point."  )
	DEFINE_SCRIPTFUNC( FindByClassnameWithin, "Find entities by class name within a radius. Pass 'null' to start an iteration, or reference to a previously found entity to continue a search"  )
END_SCRIPTDESC();

// ----------------------------------------------------------------------------
// KeyValues access - CBaseEntity::ScriptGetKeyFromModel returns root KeyValues
// ----------------------------------------------------------------------------

BEGIN_SCRIPTDESC_ROOT( CScriptKeyValues, "Wrapper class over KeyValues instance" )
	DEFINE_SCRIPT_CONSTRUCTOR()	
	DEFINE_SCRIPTFUNC_NAMED( ScriptFindKey, "FindKey", "Given a KeyValues object and a key name, find a KeyValues object associated with the key name" );
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetFirstSubKey, "GetFirstSubKey", "Given a KeyValues object, return the first sub key object" );
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetNextKey, "GetNextKey", "Given a KeyValues object, return the next key object in a sub key group" );
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetKeyValueInt, "GetKeyInt", "Given a KeyValues object and a key name, return associated integer value" );
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetKeyValueFloat, "GetKeyFloat", "Given a KeyValues object and a key name, return associated float value" );
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetKeyValueBool, "GetKeyBool", "Given a KeyValues object and a key name, return associated bool value" );
	DEFINE_SCRIPTFUNC_NAMED( ScriptGetKeyValueString, "GetKeyString", "Given a KeyValues object and a key name, return associated string value" );
	DEFINE_SCRIPTFUNC_NAMED( ScriptIsKeyValueEmpty, "IsKeyEmpty", "Given a KeyValues object and a key name, return true if key name has no value" );
	DEFINE_SCRIPTFUNC_NAMED( ScriptReleaseKeyValues, "ReleaseKeyValues", "Given a root KeyValues object, release its contents" );
END_SCRIPTDESC();

HSCRIPT CScriptKeyValues::ScriptFindKey( const char *pszName )
{
	KeyValues *pKeyValues = m_pKeyValues->FindKey(pszName);
	if ( pKeyValues == NULL )
		return NULL;

	CScriptKeyValues *pScriptKey = new CScriptKeyValues( pKeyValues );

	// UNDONE: who calls ReleaseInstance on this??
	HSCRIPT hScriptInstance = g_pScriptVM->RegisterInstance( pScriptKey );
	return hScriptInstance;
}

HSCRIPT CScriptKeyValues::ScriptGetFirstSubKey( void )
{
	KeyValues *pKeyValues = m_pKeyValues->GetFirstSubKey();
	if ( pKeyValues == NULL )
		return NULL;

	CScriptKeyValues *pScriptKey = new CScriptKeyValues( pKeyValues );

	// UNDONE: who calls ReleaseInstance on this??
	HSCRIPT hScriptInstance = g_pScriptVM->RegisterInstance( pScriptKey );
	return hScriptInstance;
}

HSCRIPT CScriptKeyValues::ScriptGetNextKey( void )
{
	KeyValues *pKeyValues = m_pKeyValues->GetNextKey();
	if ( pKeyValues == NULL )
		return NULL;

	CScriptKeyValues *pScriptKey = new CScriptKeyValues( pKeyValues );

	// UNDONE: who calls ReleaseInstance on this??
	HSCRIPT hScriptInstance = g_pScriptVM->RegisterInstance( pScriptKey );
	return hScriptInstance;
}

int CScriptKeyValues::ScriptGetKeyValueInt( const char *pszName )
{
	int i = m_pKeyValues->GetInt( pszName );
	return i;
}

float CScriptKeyValues::ScriptGetKeyValueFloat( const char *pszName )
{
	float f = m_pKeyValues->GetFloat( pszName );
	return f;
}

const char *CScriptKeyValues::ScriptGetKeyValueString( const char *pszName )
{
	const char *psz = m_pKeyValues->GetString( pszName );
	return psz;
}

bool CScriptKeyValues::ScriptIsKeyValueEmpty( const char *pszName )
{
	bool b = m_pKeyValues->IsEmpty( pszName );
	return b;
}

bool CScriptKeyValues::ScriptGetKeyValueBool( const char *pszName )
{
	bool b = m_pKeyValues->GetBool( pszName );
	return b;
}

void CScriptKeyValues::ScriptReleaseKeyValues( )
{
	m_pKeyValues->deleteThis();
	m_pKeyValues = NULL;
}


// constructors
CScriptKeyValues::CScriptKeyValues( KeyValues *pKeyValues )
{
	m_pKeyValues = pKeyValues;
}

// destructor
CScriptKeyValues::~CScriptKeyValues( )
{
	if (m_pKeyValues)
	{
		m_pKeyValues->deleteThis();
	}
	m_pKeyValues = NULL;
}




//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static float Time()
{
	return gpGlobals->curtime;
}

static float FrameTime()
{
	return gpGlobals->frametime;
}

static void SendToConsole( const char *pszCommand )
{
	CBasePlayer *pPlayer = UTIL_GetLocalPlayerOrListenServerHost();
	if ( !pPlayer )
	{
		DevMsg ("Cannot execute \"%s\", no player\n", pszCommand );
		return;
	}

	engine->ClientCommand( pPlayer->edict(), "%s", pszCommand );
}

static void SendToConsoleServer( const char *pszCommand )
{
#if defined( CSTRIKE15 )
	// Parse the text into distinct commands
	const char *pCurrentCommand = pszCommand;
	int nOffsetToNextCommand;
	int	nLen = Q_strlen( pszCommand );
	for( ; nLen > 0; nLen -= nOffsetToNextCommand+1, pCurrentCommand += nOffsetToNextCommand+1 )
	{
		// find a \n or ; line break
		int nCommandLength;
		UTIL_GetNextCommandLength( pCurrentCommand, nLen, &nCommandLength, &nOffsetToNextCommand );
		if ( nCommandLength <= 0 )
			continue;

		engine->ServerCommand( UTIL_VarArgs( "whitelistcmd %.*s\n", nCommandLength, pCurrentCommand ) );
	}
#else 
	engine->ServerCommand( pszCommand );
#endif
}

static const char *GetMapName()
{
	return STRING( gpGlobals->mapname );
}

extern ConVar	loopsingleplayermaps;

static bool LoopSinglePlayerMaps()
{
	return loopsingleplayermaps.GetBool();
}

static const char *DoUniqueString( const char *pszBase )
{
	static char szBuf[512];
	g_pScriptVM->GenerateUniqueKey( pszBase, szBuf, ARRAYSIZE(szBuf) );
	return szBuf;
}

static void DoEntFire( const char *pszTarget, const char *pszAction, const char *pszValue, float delay, HSCRIPT hActivator, HSCRIPT hCaller )
{
	const char *target = "", *action = "Use";
	variant_t value;

	target = STRING( AllocPooledString( pszTarget ) );

	// Don't allow them to run anything on a point_servercommand unless they're the host player. Otherwise they can ent_fire
	// and run any command on the server. Admittedly, they can only do the ent_fire if sv_cheats is on, but 
	// people complained about users resetting the rcon password if the server briefly turned on cheats like this:
	//    give point_servercommand
	//    ent_fire point_servercommand command "rcon_password mynewpassword"
	if ( gpGlobals->maxClients > 1 && V_stricmp( target, "point_servercommand" ) == 0 )
	{
		return;
	}

	if ( *pszAction )
	{
		action = STRING( AllocPooledString( pszAction ) );
	}
	if ( *pszValue )
	{
		value.SetString( AllocPooledString( pszValue ) );
	}
	if ( delay < 0 )
	{
		delay = 0;
	}

	g_EventQueue.AddEvent( target, action, value, delay, ToEnt(hActivator), ToEnt(hCaller) );
}

static void DoRecordAchievementEvent( const char *pszAchievementname, int iPlayerIndex )
{
	if ( iPlayerIndex < 0 )
	{
		DevWarning( "DoRecordAchievementEvent called with invalid player index (%s, %d)!\n", pszAchievementname, iPlayerIndex );
		return;
	}
	CBasePlayer *pPlayer = NULL;
	if ( iPlayerIndex > 0 )
	{
		pPlayer = UTIL_PlayerByIndex( iPlayerIndex );
		if ( !pPlayer )
		{
			DevWarning( "DoRecordAchievementEvent called with a player index that doesn't resolve to a player (%s, %d)!\n", pszAchievementname, iPlayerIndex );
			return;
		}
	}
	UTIL_RecordAchievementEvent( pszAchievementname, pPlayer );
}


bool DoIncludeScript( const char *pszScript, HSCRIPT hScope )
{
	if ( !VScriptRunScript( pszScript, hScope, true ) )
	{
		g_pScriptVM->RaiseException( CFmtStr( "Failed to include script \"%s\"", ( pszScript ) ? pszScript : "unknown" ) );
		return false;
	}
	return true;
}

int GetDeveloperLevel()
{
	return developer.GetInt();
}

static void ScriptDispatchParticleEffect( const char *pszParticleName, const Vector &vOrigin, const Vector &vAngle )
{
	QAngle qAngle;
	VectorAngles( vAngle, qAngle );
	DispatchParticleEffect( pszParticleName, vOrigin, qAngle );
}

HSCRIPT CreateProp( const char *pszEntityName, const Vector &vOrigin, const char *pszModelName, int iAnim )
{
	CBaseAnimating *pBaseEntity = (CBaseAnimating *)CreateEntityByName( pszEntityName );
	pBaseEntity->SetAbsOrigin( vOrigin );
	pBaseEntity->SetModel( pszModelName );
	pBaseEntity->SetPlaybackRate( 1.0f );

	int iSequence = pBaseEntity->SelectWeightedSequence( (Activity)iAnim );

	if ( iSequence != -1 )
	{
		pBaseEntity->SetSequence( iSequence );
	}

	return ToHScript( pBaseEntity );
}

//--------------------------------------------------------------------------------------------------
// Use an entity's script instance to add an entity IO event (used for firing events on unnamed entities from vscript)
//--------------------------------------------------------------------------------------------------
static void DoEntFireByInstanceHandle( HSCRIPT hTarget, const char *pszAction, const char *pszValue, float delay, HSCRIPT hActivator, HSCRIPT hCaller )
{
	const char *action = "Use";
	variant_t value;

	if ( *pszAction )
	{
		action = STRING( AllocPooledString( pszAction ) );
	}
	if ( *pszValue )
	{
		value.SetString( AllocPooledString( pszValue ) );
	}
	if ( delay < 0 )
	{
		delay = 0;
	}

	CBaseEntity* pTarget = ToEnt(hTarget);

	if ( !pTarget )
	{
		Warning( "VScript error: DoEntFire was passed an invalid entity instance.\n" );
		return;
	}

	g_EventQueue.AddEvent( pTarget, action, value, delay, ToEnt(hActivator), ToEnt(hCaller) );
}

static float ScriptTraceLine( const Vector &vecStart, const Vector &vecEnd, HSCRIPT entIgnore )
{
	// UTIL_TraceLine( vecAbsStart, vecAbsEnd, MASK_BLOCKLOS, pLooker, COLLISION_GROUP_NONE, ptr );
	trace_t tr;
	CBaseEntity *pLooker = ToEnt(entIgnore);
	UTIL_TraceLine( vecStart, vecEnd, MASK_NPCWORLDSTATIC, pLooker, COLLISION_GROUP_NONE, &tr);
	if (tr.fractionleftsolid && tr.startsolid)
	{
		return 1.0 - tr.fractionleftsolid;
	}
	else
	{
		return tr.fraction;
	}
}

#if defined ( PORTAL2 )
static void SetDucking( const char *pszLayerName, const char *pszMixGroupName, float factor )
{
	CReliableBroadcastRecipientFilter filter;
	UserMessageBegin( filter, "SetMixLayerTriggerFactor" );
		WRITE_STRING( pszLayerName );
		WRITE_STRING( pszMixGroupName );
		WRITE_FLOAT( factor );
	MessageEnd();
}
#endif

bool VScriptServerInit()
{
	VMPROF_START

	if( scriptmanager != NULL )
	{
		ScriptLanguage_t scriptLanguage = SL_DEFAULT;

		char const *pszScriptLanguage;
		if ( CommandLine()->CheckParm( "-scriptlang", &pszScriptLanguage ) )
		{
			if( !Q_stricmp(pszScriptLanguage, "gamemonkey") )
			{
				scriptLanguage = SL_GAMEMONKEY;
			}
			else if( !Q_stricmp(pszScriptLanguage, "squirrel") )
			{
				scriptLanguage = SL_SQUIRREL;
			}
			else if( !Q_stricmp(pszScriptLanguage, "python") )
			{
				scriptLanguage = SL_PYTHON;
			}
			else
			{
				DevWarning("-server_script does not recognize a language named '%s'. virtual machine did NOT start.\n", pszScriptLanguage );
				scriptLanguage = SL_NONE;
			}

		}
		if( scriptLanguage != SL_NONE )
		{
			if ( g_pScriptVM == NULL )
				g_pScriptVM = scriptmanager->CreateVM( scriptLanguage );

			if( g_pScriptVM )
			{
				Log_Msg( LOG_VScript, "VSCRIPT: Started VScript virtual machine using script language '%s'\n", g_pScriptVM->GetLanguageName() );
				ScriptRegisterFunctionNamed( g_pScriptVM, UTIL_ShowMessageAll, "ShowMessage", "Print a hud message on all clients" );

				ScriptRegisterFunction( g_pScriptVM, SendToConsole, "Send a string to the console as a command" );
				ScriptRegisterFunction( g_pScriptVM, SendToConsoleServer, "Send a string that gets executed on the server as a ServerCommand" );
				ScriptRegisterFunction( g_pScriptVM, GetMapName, "Get the name of the map.");
				ScriptRegisterFunction( g_pScriptVM, LoopSinglePlayerMaps, "Run the single player maps in a continuous loop.");

				ScriptRegisterFunctionNamed( g_pScriptVM, ScriptTraceLine, "TraceLine", "given 2 points & ent to ignore, return fraction along line that hits world or models" );

				ScriptRegisterFunction( g_pScriptVM, Time, "Get the current server time" );
				ScriptRegisterFunction( g_pScriptVM, FrameTime, "Get the time spent on the server in the last frame" );
				ScriptRegisterFunction( g_pScriptVM, DoEntFire, SCRIPT_ALIAS( "EntFire", "Generate and entity i/o event" ) );
				ScriptRegisterFunctionNamed( g_pScriptVM, DoEntFireByInstanceHandle, "EntFireByHandle", "Generate and entity i/o event. First parameter is an entity instance." );
				ScriptRegisterFunction( g_pScriptVM, DoUniqueString, SCRIPT_ALIAS( "UniqueString", "Generate a string guaranteed to be unique across the life of the script VM, with an optional root string. Useful for adding data to tables when not sure what keys are already in use in that table." ) );
				ScriptRegisterFunctionNamed( g_pScriptVM, ScriptCreateSceneEntity, "CreateSceneEntity", "Create a scene entity to play the specified scene." );
				ScriptRegisterFunctionNamed( g_pScriptVM, NDebugOverlay::Box, "DebugDrawBox", "Draw a debug overlay box" );
				ScriptRegisterFunctionNamed( g_pScriptVM, NDebugOverlay::Line, "DebugDrawLine", "Draw a debug overlay box" );
				ScriptRegisterFunction( g_pScriptVM, DoIncludeScript, "Execute a script (internal)" );
				ScriptRegisterFunction( g_pScriptVM, CreateProp, "Create a physics prop" );
				ScriptRegisterFunctionNamed( g_pScriptVM, DoRecordAchievementEvent, "RecordAchievementEvent", "Records achievement event or progress" );
				ScriptRegisterFunction( g_pScriptVM, GetDeveloperLevel, "Gets the level of 'developer'" );
				ScriptRegisterFunctionNamed( g_pScriptVM, ScriptDispatchParticleEffect, "DispatchParticleEffect", "Dispatches a one-off particle system" );
#if defined ( PORTAL2 )
				ScriptRegisterFunction( g_pScriptVM, SetDucking, "Set the level of an audio ducking channel" );
#endif

				g_pScriptVM->RegisterAllClasses();
				
				if ( GameRules() )
				{
					//GameRules()->RegisterScriptFunctions(); IF we want vscript, were gonna have to define these functions, see portalmp_gamerules for an example.
				}

				g_pScriptVM->RegisterInstance( &g_ScriptEntityIterator, "Entities" );

				if ( scriptLanguage == SL_SQUIRREL )
				{
					g_pScriptVM->Run( g_Script_vscript_server );
				}

				VScriptRunScript( "mapspawn", false );

				if ( script_connect_debugger_on_mapspawn.GetBool() )
				{
					g_pScriptVM->ConnectDebugger();
				}

				VMPROF_SHOW( pszScriptLanguage, "virtual machine startup" );

				return true;
			}
			else
			{
				DevWarning("VM Did not start!\n");
			}
		}
	}
	else
	{
		Log_Msg( LOG_VScript, "\nVSCRIPT: Scripting is disabled.\n" );
	}
	g_pScriptVM = NULL;
	return false;
}

void VScriptServerTerm()
{
	if( g_pScriptVM != NULL )
	{
		if( g_pScriptVM )
		{
			scriptmanager->DestroyVM( g_pScriptVM );
			g_pScriptVM = NULL;
		}
	}
}


bool VScriptServerReplaceClosures( const char *pszScriptName, HSCRIPT hScope, bool bWarnMissing )
{
	if ( !g_pScriptVM )
	{
		return false;
	}

	HSCRIPT hReplaceClosuresFunc = g_pScriptVM->LookupFunction( "__ReplaceClosures" );
	if ( !hReplaceClosuresFunc )
	{
		return false;
	}
	HSCRIPT hNewScript =  VScriptCompileScript( pszScriptName, bWarnMissing );
	if ( !hNewScript )
	{
		g_pScriptVM->ReleaseFunction( hReplaceClosuresFunc );
		return false;
	}

	g_pScriptVM->Call( hReplaceClosuresFunc, NULL, true, NULL, hNewScript, hScope );
	g_pScriptVM->ReleaseFunction( hReplaceClosuresFunc );
	g_pScriptVM->ReleaseScript( hNewScript );
	return true;
}

CON_COMMAND( script_reload_code, "Execute a vscript file, replacing existing functions with the functions in the run script" )
{
	if ( !*args[1] )
	{
		Log_Warning( LOG_VScript, "No script specified\n" );
		return;
	}

	if ( !g_pScriptVM )
	{
		Log_Warning( LOG_VScript, "Scripting disabled or no server running\n" );
		return;
	}

	VScriptServerReplaceClosures( args[1], NULL, true );
}

CON_COMMAND( script_reload_entity_code, "Execute all of this entity's VScripts, replacing existing functions with the functions in the run scripts" )
{
	extern CBaseEntity *GetNextCommandEntity( CBasePlayer *pPlayer, const char *name, CBaseEntity *ent );

	const char *pszTarget = "";
	if ( *args[1] )
	{
		pszTarget = args[1];
	}

	if ( !g_pScriptVM )
	{
		Log_Warning( LOG_VScript, "Scripting disabled or no server running\n" );
		return;
	}

	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if ( !pPlayer )
		return;

	CBaseEntity *pEntity = NULL;
	while ( (pEntity = GetNextCommandEntity( pPlayer, pszTarget, pEntity )) != NULL )
	{
		if ( pEntity->m_ScriptScope.IsInitialized() && pEntity->m_iszVScripts != NULL_STRING )
		{
			char szScriptsList[255];
			V_strcpy_safe( szScriptsList, STRING(pEntity->m_iszVScripts) );
			CUtlStringList szScripts;
			V_SplitString( szScriptsList, " ", szScripts);

			for( int i = 0 ; i < szScripts.Count() ; i++ )
			{
				VScriptServerReplaceClosures( szScripts[i], pEntity->m_ScriptScope, true );
			}
		}
	}
}

CON_COMMAND( script_reload_think, "Execute an activation script, replacing existing functions with the functions in the run script" )
{
	extern CBaseEntity *GetNextCommandEntity( CBasePlayer *pPlayer, const char *name, CBaseEntity *ent );

	const char *pszTarget = "";
	if ( *args[1] )
	{
		pszTarget = args[1];
	}

	if ( !g_pScriptVM )
	{
		Log_Warning( LOG_VScript, "Scripting disabled or no server running\n" );
		return;
	}

	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if ( !pPlayer )
		return;

	CBaseEntity *pEntity = NULL;
	while ( (pEntity = GetNextCommandEntity( pPlayer, pszTarget, pEntity )) != NULL )
	{
		if ( pEntity->m_ScriptScope.IsInitialized() && pEntity->m_iszScriptThinkFunction != NULL_STRING )
		{
			VScriptServerReplaceClosures( STRING(pEntity->m_iszScriptThinkFunction), pEntity->m_ScriptScope, true );
		}
	}
}

class CVScriptGameSystem : public CAutoGameSystemPerFrame
{
public:
	// Inherited from IAutoServerSystem
	virtual void LevelInitPreEntity( void )
	{
		// <sergiy> Note: we may need script VM garbage collection at this point in the future. Currently, VM does not persist 
		//          across level boundaries. GC is not necessary because our scripts are supposed to never create circular references
		//          and everything else is handled with ref counting. For the case of bugs creating circular references, the plan is to add
		//          diagnostics that detects such loops and warns the developer.
		
		m_bAllowEntityCreationInScripts = true;
		VScriptServerInit();
	}

	virtual void LevelInitPostEntity( void )
	{
		m_bAllowEntityCreationInScripts = false;
	}

	virtual void LevelShutdownPostEntity( void )
	{
		VScriptServerTerm();
	}

	virtual void FrameUpdatePostEntityThink() 
	{ 
		if ( g_pScriptVM )
			g_pScriptVM->Frame( gpGlobals->frametime );
	}

	bool m_bAllowEntityCreationInScripts;
};

CVScriptGameSystem g_VScriptGameSystem;

bool IsEntityCreationAllowedInScripts( void )
{
	return g_VScriptGameSystem.m_bAllowEntityCreationInScripts;
}

static short VSCRIPT_SERVER_SAVE_RESTORE_VERSION = 2;


//-----------------------------------------------------------------------------

class CVScriptSaveRestoreBlockHandler : public CDefSaveRestoreBlockHandler
{
public:
	CVScriptSaveRestoreBlockHandler() :
		m_InstanceMap( DefLessFunc(const char *) )
	{
	}
	const char *GetBlockName()
	{
		return "VScriptServer";
	}

	//---------------------------------

	void Save( ISave *pSave )
	{
		pSave->StartBlock();

		int temp = g_pScriptVM != NULL;
		pSave->WriteInt( &temp );
		if ( g_pScriptVM )
		{
			temp = g_pScriptVM->GetLanguage();
			pSave->WriteInt( &temp );
			CUtlBuffer buffer;
			g_pScriptVM->WriteState( &buffer );
			temp = buffer.TellPut();
			pSave->WriteInt( &temp );
			if ( temp > 0 )
			{
				pSave->WriteData( (const char *)buffer.Base(), temp );
			}
		}

		pSave->EndBlock();
	}

	//---------------------------------

	void WriteSaveHeaders( ISave *pSave )
	{
		pSave->WriteShort( &VSCRIPT_SERVER_SAVE_RESTORE_VERSION );
	}

	//---------------------------------

	void ReadRestoreHeaders( IRestore *pRestore )
	{
		// No reason why any future version shouldn't try to retain backward compatability. The default here is to not do so.
		short version;
		pRestore->ReadShort( &version );
		m_fDoLoad = ( version == VSCRIPT_SERVER_SAVE_RESTORE_VERSION );
	}

	//---------------------------------

	void Restore( IRestore *pRestore, bool createPlayers )
	{
		if ( !m_fDoLoad && g_pScriptVM )
		{
			return;
		}
		CBaseEntity *pEnt = gEntList.FirstEnt();
		while ( pEnt )
		{
			if ( pEnt->m_iszScriptId != NULL_STRING )
			{
				g_pScriptVM->RegisterClass( pEnt->GetScriptDesc() );
				m_InstanceMap.Insert( STRING( pEnt->m_iszScriptId ), pEnt );
			}
			pEnt = gEntList.NextEnt( pEnt );
		}

		pRestore->StartBlock();
		if ( pRestore->ReadInt() && pRestore->ReadInt() == g_pScriptVM->GetLanguage() )
		{
			int nBytes = pRestore->ReadInt();
			if ( nBytes > 0 )
			{
				CUtlBuffer buffer;
				buffer.EnsureCapacity( nBytes );
				pRestore->ReadData( (char *)buffer.AccessForDirectRead( nBytes ), nBytes, 0 );
				g_pScriptVM->ReadState( &buffer );
			}
		}
		pRestore->EndBlock();
	}

	void PostRestore( void )
	{
		for ( int i = m_InstanceMap.FirstInorder(); i != m_InstanceMap.InvalidIndex(); i = m_InstanceMap.NextInorder( i ) )
		{
			CBaseEntity *pEnt = m_InstanceMap[i];
			if ( pEnt->m_hScriptInstance )
			{
				ScriptVariant_t variant;
				if ( g_pScriptVM->GetValue( STRING(pEnt->m_iszScriptId), &variant ) && variant.m_type == FIELD_HSCRIPT )
				{
					pEnt->m_ScriptScope.Init( variant.m_hScript, false );
					pEnt->RunPrecacheScripts();
				}
			}
			else
			{
				// Script system probably has no internal references
				pEnt->m_iszScriptId = NULL_STRING;
			}
		}
		m_InstanceMap.Purge();
	}


	CUtlMap<const char *, CBaseEntity *> m_InstanceMap;

private:
	bool m_fDoLoad;
};

//-----------------------------------------------------------------------------

CVScriptSaveRestoreBlockHandler g_VScriptSaveRestoreBlockHandler;

//-------------------------------------

ISaveRestoreBlockHandler *GetVScriptSaveRestoreBlockHandler()
{
	return &g_VScriptSaveRestoreBlockHandler;
}

//-----------------------------------------------------------------------------

bool CBaseEntityScriptInstanceHelper::ToString( void *p, char *pBuf, int bufSize )	
{
	CBaseEntity *pEntity = (CBaseEntity *)p;
	if ( pEntity->GetEntityName() != NULL_STRING )
	{
		V_snprintf( pBuf, bufSize, "([%d] %s: %s)", pEntity->entindex(), STRING(pEntity->m_iClassname), STRING( pEntity->GetEntityName() ) );
	}
	else
	{
		V_snprintf( pBuf, bufSize, "([%d] %s)", pEntity->entindex(), STRING(pEntity->m_iClassname) );
	}
	return true; 
}

void *CBaseEntityScriptInstanceHelper::BindOnRead( HSCRIPT hInstance, void *pOld, const char *pszId )
{
	int iEntity = g_VScriptSaveRestoreBlockHandler.m_InstanceMap.Find( pszId );
	if ( iEntity != g_VScriptSaveRestoreBlockHandler.m_InstanceMap.InvalidIndex() )
	{
		CBaseEntity *pEnt = g_VScriptSaveRestoreBlockHandler.m_InstanceMap[iEntity];
		pEnt->m_hScriptInstance = hInstance;
		return pEnt;
	}
	return NULL;
}


CBaseEntityScriptInstanceHelper g_BaseEntityScriptInstanceHelper;


