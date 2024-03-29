//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Normal HUD mode
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//


#include "cbase.h"
#include "clientmode_shared.h"
#include "iinput.h"
#include "view_shared.h"
#include "iviewrender.h"
#include "hud_basechat.h"
#include "weapon_selection.h"
#include <vgui/IVGui.h>
#include <vgui/Cursor.h>
#include <vgui/IPanel.h>
#include <vgui/IInput.h>
#include "engine/IEngineSound.h"
#include <keyvalues.h>
#include <vgui_controls/AnimationController.h>
#include "vgui_int.h"
#include "hud_macros.h"
#include "hltvcamera.h"
#include "hud.h"
#include "hud_element_helper.h"
#if defined( INCLUDE_SCALEFORM )
#include "Scaleform/HUD/sfhud_chat.h"
#include "Scaleform/HUD/sfhudfreezepanel.h"
#include "Scaleform/HUD/sfhud_teamcounter.h"
#include "Scaleform/mapoverview.h"
#endif
//#include "hltvreplaysystem.h"
#include "netmessages.h"
#if defined( REPLAY_ENABLED )
#include "replaycamera.h"
#endif
#include "particlemgr.h"
#include "c_vguiscreen.h"
#include "c_team.h"
#include "c_rumble.h"
#include "fmtstr.h"
#include "c_playerresource.h"
#include <localize/ilocalize.h>
#include "menu.h" // CHudMenu
#if defined( _X360 )
#include "xbox/xbox_console.h"
#endif
#include "matchmaking/imatchframework.h"
//#include "clientmode_csnormal.h"


#ifdef PORTAL2
#include "c_basehlplayer.h"
#endif // PORTAL2


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CHudWeaponSelection;
class CHudChat;

static vgui::HContext s_hVGuiContext = DEFAULT_VGUI_CONTEXT;

ConVar cl_drawhud( "cl_drawhud", "1", FCVAR_CHEAT, "Enable the rendering of the hud" );
ConVar hud_takesshots( "hud_takesshots", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Auto-save a scoreboard screenshot at the end of a map." );
ConVar spec_usenumberkeys_nobinds( "spec_usenumberkeys_nobinds", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "If set to 1, map voting and spectator view use the raw number keys instead of the weapon binds (slot1, slot2, etc)." );
ConVar spec_cameraman_ui( "spec_cameraman_ui", "0", FCVAR_CLIENTDLL | FCVAR_CLIENTCMD_CAN_EXECUTE, "If a cameraman is active then use their UI commands (scoreboard, overview, etc.)" );
ConVar spec_cameraman_xray( "spec_cameraman_xray", "0", FCVAR_CLIENTDLL | FCVAR_CLIENTCMD_CAN_EXECUTE, "If a cameraman is active then use their Xray state." );
ConVar spec_cameraman_disable_with_user_control( "spec_cameraman_disable_with_user_control", "0", FCVAR_CLIENTDLL | FCVAR_CLIENTCMD_CAN_EXECUTE, "Disable cameraman UI control when user controls camera." );


extern ConVar v_viewmodel_fov;
extern ConVar spec_show_xray;
extern ConVar spec_hide_players;

extern bool IsInCommentaryMode( void );

CON_COMMAND( hud_reloadscheme, "Reloads hud layout and animation scripts." )
{
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( hh );
		ClientModeShared *mode = ( ClientModeShared * )GetClientModeNormal();
		if ( mode )
		{
			mode->ReloadScheme();
		}
	}
}

#if 0
CON_COMMAND_F( crash, "Crash the client. Optional parameter -- type of crash:\n 0: read from NULL\n 1: write to NULL\n 2: DmCrashDump() (xbox360 only)", FCVAR_CHEAT )
{
	int crashtype = 0;
	int dummy;
	if ( args.ArgC() > 1 )
	{
		crashtype = Q_atoi( args[1] );
	}
	switch (crashtype)
	{
	case 0:
		dummy = *((int *) NULL);
		Msg("Crashed! %d\n", dummy); // keeps dummy from optimizing out
		break;
	case 1:
		*((int *)NULL) = 42;
		break;
#if defined( _GAMECONSOLE )
	case 2:
		XBX_CrashDump( false );
		break;
	case 3:
		XBX_CrashDumpFullHeap( true );
		break;
#endif
	default:
		Msg("Unknown variety of crash. You have now failed to crash. I hope you're happy.\n");
		break;
	}
}
#endif // _DEBUG

static bool __MsgFunc_Rumble( const CUsrMsg_Rumble &msg )
{
	unsigned char waveformIndex;
	unsigned char rumbleData;
	unsigned char rumbleFlags;

	waveformIndex = msg.index();
	rumbleData = msg.data();
	rumbleFlags = msg.flags();

	int userID = XBX_GetActiveUserId();

	RumbleEffect( userID, waveformIndex, rumbleData, rumbleFlags );

	return true;
}

static bool __MsgFunc_VGUIMenu( const CUsrMsg_VGUIMenu &msg )
{
	const char* pszPanelName = msg.name().c_str();
	bool bShow = msg.show();

	ASSERT_LOCAL_PLAYER_RESOLVABLE();

	KeyValues *keys = NULL;

	if ( msg.subkeys_size() > 0 )
	{
		keys = new KeyValues("data");

		for (int i = 0; i < msg.subkeys_size(); i ++ )
		{
			const CUsrMsg_VGUIMenu::Subkey& subkey = msg.subkeys( i );
						
			keys->SetString( subkey.name().c_str(), subkey.str().c_str() );
		}

		// !KLUDGE! Whitelist of URL protocols formats for MOTD
		if ( !V_stricmp( pszPanelName, PANEL_INFO ) // MOTD
			&& keys->GetInt( "type", 0 ) == 2 // URL message type
		) {
			const char *pszURL = keys->GetString( "msg", "" );
			if ( Q_strncmp( pszURL, "http://", 7 ) != 0 && Q_strncmp( pszURL, "https://", 8 ) != 0 && Q_stricmp( pszURL, "about:blank" ) != 0 )
			{
				Warning( "Blocking MOTD URL '%s'; must begin with 'http://' or 'https://' or be about:blank\n", pszURL );
				keys->deleteThis();
				return true;
			}
		}
	}

	GetViewPortInterface()->ShowPanel( msg.name().c_str(), bShow, keys, true );

	// Don't do this since ShowPanel auto-deletes the keys
	// keys->deleteThis();

	// is the server telling us to show the scoreboard (at the end of a map)?
	if ( Q_stricmp( pszPanelName, "scores" ) == 0 )
	{
		if ( hud_takesshots.GetBool() == true )
		{
			GetHud().SetScreenShotTime( gpGlobals->curtime + 1.0 ); // take a screenshot in 1 second
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ClientModeShared::ClientModeShared()
{
	m_pViewport = NULL;
	m_pChatElement = NULL;
	m_pWeaponSelection = NULL;
	m_nRootSize[ 0 ] = m_nRootSize[ 1 ] = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ClientModeShared::~ClientModeShared()
{
	// VGui_Shutdown() should have deleted/NULL'd
	Assert( !m_pViewport );
}

void ClientModeShared::ReloadScheme( void )
{
	ReloadSchemeWithRoot( VGui_GetClientDLLRootPanel() );
}

void ClientModeShared::ReloadSchemeWithRoot( vgui::VPANEL pRoot )
{
	if ( pRoot )
	{
		int wide, tall;
		vgui::ipanel()->GetSize(pRoot, wide, tall);
		m_nRootSize[ 0 ] = wide;
		m_nRootSize[ 1 ] = tall;
	}

	m_pViewport->ReloadScheme( "resource/ClientScheme.res" );
	if ( GET_ACTIVE_SPLITSCREEN_SLOT() == 0 )
	{
		ClearKeyValuesCache();
	}
	// Msg( "Reload scheme [%d]\n", GET_ACTIVE_SPLITSCREEN_SLOT() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeShared::Init()
{
	InitChatHudElement();

	InitWeaponSelectionHudElement();

	// Derived ClientMode class must make sure m_Viewport is instantiated
	Assert( m_pViewport );
	m_pViewport->LoadHudLayout();

	ListenForGameEvent( "player_connect_full" );
	ListenForGameEvent( "player_connect" );
	ListenForGameEvent( "player_disconnect" );
	ListenForGameEvent( "player_team" );
	ListenForGameEvent( "server_cvar" );
	ListenForGameEvent( "player_changename" );
	ListenForGameEvent( "teamplay_broadcast_audio" );
	ListenForGameEvent( "achievement_earned" );

#if defined( TF_CLIENT_DLL ) || defined( CSTRIKE_CLIENT_DLL )
	ListenForGameEvent( "item_found" );
	ListenForGameEvent( "items_gifted" );
#endif

#if defined( INFESTED_DLL )
	ListenForGameEvent( "player_fullyjoined" );	
#endif




	HLTVCamera()->Init();
#if defined( REPLAY_ENABLED )
	ReplayCamera()->Init();
#endif

	m_CursorNone = vgui::dc_none;

	HOOK_MESSAGE( VGUIMenu );
	HOOK_MESSAGE( Rumble );
}

void ClientModeShared::InitChatHudElement()
{
	m_pChatElement = CBaseHudChat::GetHudChat();
	Assert( m_pChatElement );
}

void ClientModeShared::InitWeaponSelectionHudElement()
{
	m_pWeaponSelection = ( CBaseHudWeaponSelection * )GET_HUDELEMENT( CHudWeaponSelection );
	Assert( m_pWeaponSelection );
}

void ClientModeShared::InitViewport()
{
}


void ClientModeShared::VGui_Shutdown()
{
	delete m_pViewport;
	m_pViewport = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeShared::Shutdown()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : frametime - 
//			*cmd - 
//-----------------------------------------------------------------------------
bool ClientModeShared::CreateMove( float flInputSampleTime, CUserCmd *cmd )
{
	// Let the player override the view.
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if(!pPlayer)
		return true;

	// Let the player at it
	return pPlayer->CreateMove( flInputSampleTime, cmd );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSetup - 
//-----------------------------------------------------------------------------
void ClientModeShared::OverrideView( CViewSetup *pSetup )
{
	QAngle camAngles;

	// Let the player override the view.
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if(!pPlayer)
		return;

	pPlayer->OverrideView( pSetup );

	if( ::input->CAM_IsThirdPerson() )
	{
		Vector cam_ofs;

		::input->CAM_GetCameraOffset( cam_ofs );

		camAngles[ PITCH ] = cam_ofs[ PITCH ];
		camAngles[ YAW ] = cam_ofs[ YAW ];
		camAngles[ ROLL ] = 0;

		Vector camForward, camRight, camUp;
		AngleVectors( camAngles, &camForward, &camRight, &camUp );

		float flSavedZ = pSetup->origin.z;
		pSetup->origin = pPlayer->GetThirdPersonViewPosition();
		pSetup->origin.z -= (pSetup->origin.z - flSavedZ);

		VectorMA( pSetup->origin, -cam_ofs[ ROLL ], camForward, pSetup->origin );

		static ConVarRef c_thirdpersonshoulder( "c_thirdpersonshoulder" );
		if ( c_thirdpersonshoulder.GetBool() )
		{
			static ConVarRef c_thirdpersonshoulderoffset( "c_thirdpersonshoulderoffset" );
			static ConVarRef c_thirdpersonshoulderheight( "c_thirdpersonshoulderheight" );
			static ConVarRef c_thirdpersonshoulderaimdist( "c_thirdpersonshoulderaimdist" );

			// add the shoulder offset to the origin in the cameras right vector
			VectorMA( pSetup->origin, c_thirdpersonshoulderoffset.GetFloat(), camRight, pSetup->origin );

			// add the shoulder height to the origin in the cameras up vector
			VectorMA( pSetup->origin, c_thirdpersonshoulderheight.GetFloat(), camUp, pSetup->origin );

			// adjust the yaw to the aim-point
			camAngles[ YAW ] += RAD2DEG( atan(c_thirdpersonshoulderoffset.GetFloat() / (c_thirdpersonshoulderaimdist.GetFloat() + cam_ofs[ ROLL ])) );

			// adjust the pitch to the aim-point
			camAngles[ PITCH ] += RAD2DEG( atan(c_thirdpersonshoulderheight.GetFloat() / (c_thirdpersonshoulderaimdist.GetFloat() + cam_ofs[ ROLL ])) );
		}

		// Override angles from third person camera
		VectorCopy( camAngles, pSetup->angles );
	}
	else if (::input->CAM_IsOrthographic())
	{
		pSetup->m_bOrtho = true;
		float w, h;
		::input->CAM_OrthographicSize( w, h );
		w *= 0.5f;
		h *= 0.5f;
		pSetup->m_OrthoLeft   = -w;
		pSetup->m_OrthoTop    = -h;
		pSetup->m_OrthoRight  = w;
		pSetup->m_OrthoBottom = h;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool ClientModeShared::ShouldDrawEntity(C_BaseEntity *pEnt)
{
	return true;
}

bool ClientModeShared::ShouldDrawParticles( )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Allow weapons to override mouse input (for binoculars)
//-----------------------------------------------------------------------------
void ClientModeShared::OverrideMouseInput( float *x, float *y )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	C_BaseCombatWeapon *pWeapon = pPlayer ? pPlayer->GetActiveWeapon() : NULL;;
	if ( pWeapon )
	{
		pWeapon->OverrideMouseInput( x, y );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool ClientModeShared::ShouldDrawViewModel()
{
	return true;
}

bool ClientModeShared::ShouldDrawDetailObjects( )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool ClientModeShared::ShouldDrawCrosshair( void )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Don't draw the current view entity if we are not in 3rd person
//-----------------------------------------------------------------------------
bool ClientModeShared::ShouldDrawLocalPlayer( C_BasePlayer *pPlayer )
{
	if ( pPlayer->IsViewEntity() && !pPlayer->ShouldDrawLocalPlayer() )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: The mode can choose to not draw fog
//-----------------------------------------------------------------------------
bool ClientModeShared::ShouldDrawFog( void )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeShared::AdjustEngineViewport( int& x, int& y, int& width, int& height )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeShared::PreRender( CViewSetup *pSetup )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeShared::PostRender()
{
	// Let the particle manager simulate things that haven't been simulated.
	ParticleMgr()->PostRender();
}

void ClientModeShared::PostRenderVGui()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeShared::Update()
{
	if ( m_pViewport->IsVisible() != cl_drawhud.GetBool() )
	{
		m_pViewport->SetVisible( cl_drawhud.GetBool() );
	}

	UpdateRumbleEffects( XBX_GetActiveUserId() );
}

//-----------------------------------------------------------------------------
// This processes all input before SV Move messages are sent
//-----------------------------------------------------------------------------

void ClientModeShared::ProcessInput(bool bActive)
{
	GetHud().ProcessInput( bActive );
}

//-----------------------------------------------------------------------------
// Purpose: We've received a keypress from the engine. Return 1 if the engine is allowed to handle it.
//-----------------------------------------------------------------------------
int	ClientModeShared::KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding )
{
	if ( engine->Con_IsVisible() )
		return 1;

	// Should we start typing a message?
	if ( pszCurrentBinding &&
		( Q_strcmp( pszCurrentBinding, "messagemode" ) == 0 ||
		Q_strcmp( pszCurrentBinding, "say" ) == 0 ) )
	{
		if ( down )
		{
			StartMessageMode( MM_SAY );
		}
		return 0;
	}
	else if ( pszCurrentBinding &&
		( Q_strcmp( pszCurrentBinding, "messagemode2" ) == 0 ||
		Q_strcmp( pszCurrentBinding, "say_team" ) == 0 ) )
	{
		if ( down )
		{
			StartMessageMode( MM_SAY_TEAM );
		}
		return 0;
	}

	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();

	if ( IsJoystickCode( keynum ) )
	{
		keynum = GetBaseButtonCode( keynum );
	}

	// If SourceMod menu is open (they use CHudMenu), give it input priority.
	bool bIsHudMenuOpen = false;
	CHudMenu *pHudMenu = GET_HUDELEMENT( CHudMenu );
	bIsHudMenuOpen = ( pHudMenu && pHudMenu->IsMenuOpen() );
	if ( bIsHudMenuOpen && !HudElementKeyInput( down, keynum, pszCurrentBinding ) )
	{
		return 0;
	}

	// if ingame spectator mode, let spectator input intercept key event here
	if( pPlayer &&
		( pPlayer->GetObserverMode() > OBS_MODE_DEATHCAM ) &&
		!HandleSpectatorKeyInput( down, keynum, pszCurrentBinding ) )
	{
		return 0;
	}

	// Let game-specific hud elements get a crack at the key input
	if ( !HudElementKeyInput( down, keynum, pszCurrentBinding ) )
	{
		return 0;
	}

	C_BaseCombatWeapon *pWeapon = pPlayer ? pPlayer->GetActiveWeapon() : NULL;
	if ( pWeapon )
	{
		return pWeapon->KeyInput( down, keynum, pszCurrentBinding );
	}

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Helper to find if a binding exists in a possible chain of bindings
//-----------------------------------------------------------------------------
bool ContainsBinding( const char *pszBindingString, const char *pszBinding, bool bSearchAliases /*= false*/ )
{
	if ( !strchr( pszBindingString, ';' ) && !bSearchAliases )
	{
		return !Q_stricmp( pszBindingString, pszBinding );
	}
	else
	{
		// Tokenize the binding name
		CUtlVectorAutoPurge< char *> cmdStrings;
		V_SplitString( pszBindingString, ";", cmdStrings );
		FOR_EACH_VEC( cmdStrings, i )
		{
			char* szCmd = cmdStrings[ i ];
			if ( bSearchAliases )
			{
				// Search for command in any contained aliases. 
				const char* szAliasCmd = engine->AliasToCommandString( szCmd );
				// NOTE: we could use some kind of recursion guard, but recursive aliases already infinite loop 
				// when being processed by the cmd system. 
				if ( szAliasCmd )
				{
					CUtlString strCmd( szAliasCmd );
					V_StripTrailingWhitespace( strCmd.Access() ); // Alias adds trailing spaces to commands, strip it here so the compare works
					if ( ContainsBinding( strCmd.Get(), pszBinding, true ) )
						return true;
				}
			}

			if ( !Q_stricmp( pszBinding, szCmd ) )
			{
				return true;
			}
		}
		return false;
	}
}

void ClientModeShared::UpdateCameraManUIState( int iType, int nOptionalParam, uint64 xuid )
{
	/* Removed for partner depot */
}

void SendCameraManUIStateChange( HltvUiType_t eventType, int nOptionalParam )
{
	// this sends a client command to the server which will then change the server side states and propagate that out to everyone
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pPlayer && pPlayer->IsActiveCameraMan() )
	{
		char szTemp[ 256 ];
		V_sprintf_safe( szTemp, "cameraman_ui_state %d %d", eventType, nOptionalParam );
		engine->ClientCmd( szTemp );
	}
}

void ClientModeShared::ScoreboardOff()
{
	//SendCameraManUIStateChange( HLTV_UI_SCOREBOARD_OFF );
}

void ClientModeShared::GraphPageChanged()
{
	/* Removed for partner depot */
}

//-----------------------------------------------------------------------------
// Purpose: See if spectator input occurred. Return 0 if the key is swallowed.
//-----------------------------------------------------------------------------
int ClientModeShared::HandleSpectatorKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding )
{
	if ( down && pszCurrentBinding && Q_strcmp( pszCurrentBinding, "+attack" ) == 0 )
	{
		engine->ClientCmd( "spec_next" );
		return 0; // we handled it, don't handle twice or send to server
	}
	else if ( down && pszCurrentBinding && Q_strcmp( pszCurrentBinding, "+attack2" ) == 0 )
	{
		engine->ClientCmd( "spec_prev" );
		return 0;
	}
	else if ( down && pszCurrentBinding && Q_strcmp( pszCurrentBinding, "+jump" ) == 0 )
	{
		engine->ClientCmd( "spec_mode" );
		return 0;
	}
	else if ( down && pszCurrentBinding && Q_strcmp( pszCurrentBinding, "+strafe" ) == 0 )
	{
		HLTVCamera()->SetAutoDirector( true );
#if defined( REPLAY_ENABLED )
		ReplayCamera()->SetAutoDirector( true );
#endif
		return 0;
	}

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: See if hud elements want key input. Return 0 if the key is swallowed
//-----------------------------------------------------------------------------
int ClientModeShared::HudElementKeyInput(int down, ButtonCode_t keynum, const char *pszCurrentBinding)
{
	if (m_pWeaponSelection)
	{
		if (!m_pWeaponSelection->KeyInput(down, keynum, pszCurrentBinding))
		{
			return 0;
		}
	}

#if defined( REPLAY_ENABLED )
	if (m_pReplayReminderPanel)
	{
		if (m_pReplayReminderPanel->HudElementKeyInput(down, keynum, pszCurrentBinding))
		{
			return 0;
		}
	}
#endif

	return 1;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : vgui::Panel
//-----------------------------------------------------------------------------
vgui::Panel *ClientModeShared::GetMessagePanel()
{
	if ( m_pChatElement && m_pChatElement->GetInputPanel() && m_pChatElement->GetInputPanel()->IsVisible() )
		return m_pChatElement->GetInputPanel();

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: The player has started to type a message
//-----------------------------------------------------------------------------
void ClientModeShared::StartMessageMode( int iMessageModeType )
{
	// Can only show chat UI in multiplayer!!!
	if ( gpGlobals->maxClients == 1 )
	{
		return;
	}

#if defined( INCLUDE_SCALEFORM )
	SFHudChat* pChat = GET_HUDELEMENT( SFHudChat );
	if ( pChat )
	{
		pChat->StartMessageMode( iMessageModeType );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *newmap - 
//-----------------------------------------------------------------------------
void ClientModeShared::LevelInit( const char *newmap )
{
	m_pViewport->GetAnimationController()->StartAnimationSequence("LevelInit");

	// Tell the Chat Interface
	if ( m_pChatElement )
	{
		m_pChatElement->LevelInit( newmap );
	}

	// we have to fake this event clientside, because clients connect after that
	IGameEvent *event = gameeventmanager->CreateEvent( "game_newmap" );
	if ( event )
	{
		event->SetString("mapname", newmap );
		gameeventmanager->FireEventClientSide( event );
	}

	// Create a vgui context for all of the in-game vgui panels...
	if ( s_hVGuiContext == DEFAULT_VGUI_CONTEXT )
	{
		s_hVGuiContext = vgui::ivgui()->CreateContext();
	}

	// Reset any player explosion/shock effects
	CLocalPlayerFilter filter;
	enginesound->SetPlayerDSP( filter, 0, true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeShared::LevelShutdown( void )
{
	if ( m_pChatElement )
	{
	m_pChatElement->LevelShutdown();
	}
	if ( s_hVGuiContext != DEFAULT_VGUI_CONTEXT )
	{
		vgui::ivgui()->DestroyContext( s_hVGuiContext );
		s_hVGuiContext = DEFAULT_VGUI_CONTEXT;
	}

	// Reset any player explosion/shock effects
	CLocalPlayerFilter filter;
	enginesound->SetPlayerDSP( filter, 0, true );
}

void ClientModeShared::Enable()
{
	vgui::VPANEL pRoot = VGui_GetClientDLLRootPanel();
	EnableWithRootPanel( pRoot );
}

void ClientModeShared::EnableWithRootPanel( vgui::VPANEL pRoot )
{
	// Add our viewport to the root panel.
	if( pRoot != NULL )
	{
		m_pViewport->SetParent( pRoot );
	}

	// All hud elements should be proportional
	// This sets that flag on the viewport and all child panels
	m_pViewport->SetProportional( true );

	m_pViewport->SetCursor( m_CursorNone );
	vgui::surface()->SetCursor( m_CursorNone );

	m_pViewport->SetVisible( true );
	if ( m_pViewport->IsKeyBoardInputEnabled() )
	{
		m_pViewport->RequestFocus();
	}

	Layout();
}


void ClientModeShared::Disable()
{
	vgui::VPANEL pRoot;

	// Remove our viewport from the root panel.
	if( ( pRoot = VGui_GetClientDLLRootPanel() ) != NULL )
	{
		m_pViewport->SetParent( (vgui::VPANEL)NULL );
	}

	m_pViewport->SetVisible( false );
}


void ClientModeShared::Layout( bool bForce /*= false*/)
{
	vgui::VPANEL pRoot;
	int wide, tall;

	// Make the viewport fill the root panel.
	if( ( pRoot = m_pViewport->GetVParent() ) != NULL )
	{
		vgui::ipanel()->GetSize(pRoot, wide, tall);
		bool changed = wide != m_nRootSize[ 0 ] || tall != m_nRootSize[ 1 ];
		m_pViewport->SetBounds(0, 0, wide, tall);
		if ( changed || bForce )
		{
			ReloadSchemeWithRoot( pRoot );
		}
	}
}

#ifdef IRONSIGHT
#ifdef DEBUG
	ConVar ironsight_scoped_viewmodel_fov( "ironsight_scoped_viewmodel_fov", "54", FCVAR_CHEAT, "The fov of the viewmodel when ironsighted" );
#else
	#define IRONSIGHT_SCOPED_FOV 54.0f
#endif
#endif

float ClientModeShared::GetViewModelFOV( void )
{

#ifdef IRONSIGHT
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pPlayer )
	{
		CWeaponCSBase *pIronSightWeapon = (CWeaponCSBase*)pPlayer->GetActiveWeapon();
		if ( pIronSightWeapon )
		{
			CIronSightController* pIronSightController = pIronSightWeapon->GetIronSightController();
			if ( pIronSightController && pIronSightController->IsInIronSight() )
			{
				return FLerp( v_viewmodel_fov.GetFloat(),	
					#ifdef DEBUG
						ironsight_scoped_viewmodel_fov.GetFloat(),
					#else
						IRONSIGHT_SCOPED_FOV,
					#endif
				pIronSightController->GetIronSightAmount() );
			}
		}
	}
#endif

	return v_viewmodel_fov.GetFloat();
}

vgui::Panel *ClientModeShared::GetPanelFromViewport( const char *pchNamePath )
{
	char szTagetName[ 256 ];
	Q_strncpy( szTagetName, pchNamePath, sizeof(szTagetName) );

	char *pchName = szTagetName;

	char *pchEndToken = strchr( pchName, ';' );
	if ( pchEndToken )
	{
		*pchEndToken = '\0';
	}

	char *pchNextName = strchr( pchName, '/' );
	if ( pchNextName )
	{
		*pchNextName = '\0';
		pchNextName++;
	}

	// Comma means we want to count to a specific instance by name
	int nInstance = 0;

	char *pchInstancePos = strchr( pchName, ',' );
	if ( pchInstancePos )
	{
		*pchInstancePos = '\0';
		pchInstancePos++;

		nInstance = atoi( pchInstancePos );
	}

	// Find the child
	int nCurrentInstance = 0;
	vgui::Panel *pPanel = NULL;

	for ( int i = 0; i < GetViewport()->GetChildCount(); i++ )
	{
		Panel *pChild = GetViewport()->GetChild( i );
		if ( !pChild )
			continue;

		if ( stricmp( pChild->GetName(), pchName ) == 0 )
		{
			nCurrentInstance++;

			if ( nCurrentInstance > nInstance )
			{
				pPanel = pChild;
				break;
			}
		}
	}

	pchName = pchNextName;

	while ( pPanel )
	{
		if ( !pchName || pchName[ 0 ] == '\0' )
		{
			break;
		}

		pchNextName = strchr( pchName, '/' );
		if ( pchNextName )
		{
			*pchNextName = '\0';
			pchNextName++;
		}

		// Comma means we want to count to a specific instance by name
		nInstance = 0;

		pchInstancePos = strchr( pchName, ',' );
		if ( pchInstancePos )
		{
			*pchInstancePos = '\0';
			pchInstancePos++;

			nInstance = atoi( pchInstancePos );
		}

		// Find the child
		nCurrentInstance = 0;
		vgui::Panel *pNextPanel = NULL;

		for ( int i = 0; i < pPanel->GetChildCount(); i++ )
		{
			Panel *pChild = pPanel->GetChild( i );
			if ( !pChild )
				continue;

			if ( stricmp( pChild->GetName(), pchName ) == 0 )
			{
				nCurrentInstance++;

				if ( nCurrentInstance > nInstance )
				{
					pNextPanel = pChild;
					break;
				}
			}
		}

		pPanel = pNextPanel;
		pchName = pchNextName;
	}

	return pPanel;
}

class CHudChat;

bool PlayerNameNotSetYet( const char *pszName )
{
	if ( pszName && pszName[0] )
	{
		// Don't show "unconnected" if we haven't got the players name yet
		if ( StringHasPrefix( pszName, "unconnected" ) )
			return true;
		if ( StringHasPrefix( pszName, "NULLNAME" ) )
			return true;
	}

	return false;
}

void ClientModeShared::FireGameEvent(IGameEvent *event)
{
	CBaseHudChat *hudChat = (CBaseHudChat *)GET_HUDELEMENT(CHudChat);

	const char *eventname = event->GetName();

	if (Q_strcmp("player_connect_client", eventname) == 0)
	{
		if (!hudChat)
			return;
		if (PlayerNameNotSetYet(event->GetString("name")))
			return;

		if (!IsInCommentaryMode())
		{
			wchar_t wszLocalized[100];
			wchar_t wszPlayerName[MAX_PLAYER_NAME_LENGTH];
			g_pVGuiLocalize->ConvertANSIToUnicode(event->GetString("name"), wszPlayerName, sizeof(wszPlayerName));
			g_pVGuiLocalize->ConstructString(wszLocalized, sizeof(wszLocalized), g_pVGuiLocalize->Find("#game_player_connecting"), 1, wszPlayerName);

			char szLocalized[100];
			g_pVGuiLocalize->ConvertUnicodeToANSI(wszLocalized, szLocalized, sizeof(szLocalized));

			hudChat->Printf(CHAT_FILTER_JOINLEAVE, "%s", szLocalized);
		}
	}
	else if (Q_strcmp("player_disconnect", eventname) == 0)
	{
		C_BasePlayer *pPlayer = USERID2PLAYER(event->GetInt("userid"));

		if (!hudChat || !pPlayer)
			return;
		if (PlayerNameNotSetYet(event->GetString("name")))
			return;

		if (!IsInCommentaryMode())
		{
			wchar_t wszPlayerName[MAX_PLAYER_NAME_LENGTH];
			g_pVGuiLocalize->ConvertANSIToUnicode(pPlayer->GetPlayerName(), wszPlayerName, sizeof(wszPlayerName));

			wchar_t wszReason[64];
			const char *pszReason = event->GetString("reason");
			if (pszReason && (pszReason[0] == '#') && g_pVGuiLocalize->Find(pszReason))
			{
				V_wcsncpy(wszReason, g_pVGuiLocalize->Find(pszReason), sizeof(wszReason));
		}
			else
			{
				g_pVGuiLocalize->ConvertANSIToUnicode(pszReason, wszReason, sizeof(wszReason));
			}

			wchar_t wszLocalized[100];
			if (IsPC())
			{
				g_pVGuiLocalize->ConstructString(wszLocalized, sizeof(wszLocalized), g_pVGuiLocalize->Find("#game_player_left_game"), 2, wszPlayerName, wszReason);
			}
			else
			{
				g_pVGuiLocalize->ConstructString(wszLocalized, sizeof(wszLocalized), g_pVGuiLocalize->Find("#game_player_left_game"), 1, wszPlayerName);
			}

			char szLocalized[100];
			g_pVGuiLocalize->ConvertUnicodeToANSI(wszLocalized, szLocalized, sizeof(szLocalized));

			hudChat->Printf(CHAT_FILTER_JOINLEAVE, "%s", szLocalized);
	}
}
	else if (Q_strcmp("player_team", eventname) == 0)
	{
		C_BasePlayer *pPlayer = USERID2PLAYER(event->GetInt("userid"));
		if (!hudChat)
			return;

		bool bDisconnected = event->GetBool("disconnect");

		if (bDisconnected)
			return;

		int team = event->GetInt("team");
		bool bAutoTeamed = event->GetInt("autoteam", false);
		bool bSilent = event->GetInt("silent", false);

		const char *pszName = event->GetString("name");
		if (PlayerNameNotSetYet(pszName))
			return;

		if (!bSilent)
		{
			wchar_t wszPlayerName[MAX_PLAYER_NAME_LENGTH];
			g_pVGuiLocalize->ConvertANSIToUnicode(pszName, wszPlayerName, sizeof(wszPlayerName));

			bool bUsingCustomTeamName = false;
#ifdef TF_CLIENT_DLL
			C_TFTeam *pTeam = GetGlobalTFTeam(team);
			const wchar_t *wszTeam = pTeam ? pTeam->Get_Localized_Name() : L"";
			bUsingCustomTeamName = pTeam ? pTeam->IsUsingCustomTeamName() : false;
#else
			wchar_t wszTeam[64];
			C_Team *pTeam = GetGlobalTeam(team);
			if (pTeam)
			{
				g_pVGuiLocalize->ConvertANSIToUnicode(pTeam->Get_Name(), wszTeam, sizeof(wszTeam));
		}
			else
			{
				_snwprintf(wszTeam, sizeof(wszTeam) / sizeof(wchar_t), L"%d", team);
			}
#endif

			if (!IsInCommentaryMode())
			{
				wchar_t wszLocalized[100];
				if (bAutoTeamed)
				{
					g_pVGuiLocalize->ConstructString(wszLocalized, sizeof(wszLocalized), g_pVGuiLocalize->Find("#game_player_joined_autoteam"), 2, wszPlayerName, wszTeam);
				}
				else
				{
					g_pVGuiLocalize->ConstructString(wszLocalized, sizeof(wszLocalized), g_pVGuiLocalize->Find("#game_player_joined_team"), 2, wszPlayerName, wszTeam);
				}

				char szLocalized[100];
				g_pVGuiLocalize->ConvertUnicodeToANSI(wszLocalized, szLocalized, sizeof(szLocalized));

				hudChat->Printf(CHAT_FILTER_TEAMCHANGE, "%s", szLocalized);
			}
	}

		if (pPlayer && pPlayer->IsLocalPlayer())
		{
			// that's me
			pPlayer->TeamChange(team);
		}
	}
	else if (Q_strcmp("player_changename", eventname) == 0)
	{
		if (!hudChat)
			return;

		const char *pszOldName = event->GetString("oldname");
		if (PlayerNameNotSetYet(pszOldName))
			return;

		wchar_t wszOldName[MAX_PLAYER_NAME_LENGTH];
		g_pVGuiLocalize->ConvertANSIToUnicode(pszOldName, wszOldName, sizeof(wszOldName));

		wchar_t wszNewName[MAX_PLAYER_NAME_LENGTH];
		g_pVGuiLocalize->ConvertANSIToUnicode(event->GetString("newname"), wszNewName, sizeof(wszNewName));

		wchar_t wszLocalized[100];
		g_pVGuiLocalize->ConstructString(wszLocalized, sizeof(wszLocalized), g_pVGuiLocalize->Find("#game_player_changed_name"), 2, wszOldName, wszNewName);

		char szLocalized[100];
		g_pVGuiLocalize->ConvertUnicodeToANSI(wszLocalized, szLocalized, sizeof(szLocalized));

		hudChat->Printf(CHAT_FILTER_NAMECHANGE, "%s", szLocalized);
	}
	else if (Q_strcmp("teamplay_broadcast_audio", eventname) == 0)
	{
		int team = event->GetInt("team");

		bool bValidTeam = false;

		if ((GetLocalTeam() && GetLocalTeam()->GetTeamNumber() == team))
		{
			bValidTeam = true;
		}

		//If we're in the spectator team then we should be getting whatever messages the person I'm spectating gets.
		if (bValidTeam == false)
		{
			CBasePlayer *pSpectatorTarget = UTIL_PlayerByIndex(GetSpectatorTarget());

			if (pSpectatorTarget && (GetSpectatorMode() == OBS_MODE_IN_EYE || GetSpectatorMode() == OBS_MODE_CHASE))
			{
				if (pSpectatorTarget->GetTeamNumber() == team)
				{
					bValidTeam = true;
				}
			}
		}

		if (team == 0 && GetLocalTeam() > 0)
		{
			bValidTeam = false;
		}

		if (team == 255)
		{
			bValidTeam = true;
		}

		if (bValidTeam == true)
		{
			EmitSound_t et;
			et.m_pSoundName = event->GetString("sound");
			et.m_nFlags = event->GetInt("additional_flags");

			CLocalPlayerFilter filter;
			C_BaseEntity::EmitSound(filter, SOUND_FROM_LOCAL_PLAYER, et);
		}
	}
	else if (Q_strcmp("server_cvar", eventname) == 0)
	{
		if (!IsInCommentaryMode())
		{
			wchar_t wszCvarName[64];
			g_pVGuiLocalize->ConvertANSIToUnicode(event->GetString("cvarname"), wszCvarName, sizeof(wszCvarName));

			wchar_t wszCvarValue[64];
			g_pVGuiLocalize->ConvertANSIToUnicode(event->GetString("cvarvalue"), wszCvarValue, sizeof(wszCvarValue));

			wchar_t wszLocalized[256];
			g_pVGuiLocalize->ConstructString(wszLocalized, sizeof(wszLocalized), g_pVGuiLocalize->Find("#game_server_cvar_changed"), 2, wszCvarName, wszCvarValue);

			char szLocalized[256];
			g_pVGuiLocalize->ConvertUnicodeToANSI(wszLocalized, szLocalized, sizeof(szLocalized));

			hudChat->Printf(CHAT_FILTER_SERVERMSG, "%s", szLocalized);
		}
	}
#if defined( TF_CLIENT_DLL )
	else if (Q_strcmp("item_found", eventname) == 0)
	{
		int iPlayerIndex = event->GetInt("player");
		entityquality_t iItemQuality = event->GetInt("quality");
		int iMethod = event->GetInt("method");
		int iItemDef = event->GetInt("itemdef");
		bool bIsStrange = event->GetInt("isstrange");
		bool bIsUnusual = event->GetInt("isunusual");
		float flWear = event->GetFloat("wear");

		C_BasePlayer *pPlayer = UTIL_PlayerByIndex(iPlayerIndex);
		const GameItemDefinition_t *pItemDefinition = dynamic_cast<GameItemDefinition_t *>(GetItemSchema()->GetItemDefinition(iItemDef));

		if (!pPlayer || !pItemDefinition || pItemDefinition->IsHidden())
			return;

		if (g_PR)
		{
			wchar_t wszPlayerName[MAX_PLAYER_NAME_LENGTH];
			g_pVGuiLocalize->ConvertANSIToUnicode(g_PR->GetPlayerName(iPlayerIndex), wszPlayerName, sizeof(wszPlayerName));

			if (iMethod < 0 || iMethod >= ARRAYSIZE(g_pszItemFoundMethodStrings))
			{
				iMethod = 0;
			}

			const char *pszLocString = g_pszItemFoundMethodStrings[iMethod];
			if (pszLocString)
			{
				wchar_t wszItemFound[256];
				_snwprintf(wszItemFound, ARRAYSIZE(wszItemFound), L"%ls", g_pVGuiLocalize->Find(pszLocString));

				wchar_t *colorMarker = wcsstr(wszItemFound, L"::");
				const CEconItemRarityDefinition* pItemRarity = GetItemSchema()->GetRarityDefinition(pItemDefinition->GetRarity());

				if (colorMarker)
				{
					if (pItemRarity)
					{
						attrib_colors_t colorRarity = pItemRarity->GetAttribColor();
						vgui::HScheme scheme = vgui::scheme()->GetScheme("ClientScheme");
						vgui::IScheme *pScheme = vgui::scheme()->GetIScheme(scheme);
						Color color = pScheme->GetColor(GetColorNameForAttribColor(colorRarity), Color(255, 255, 255, 255));
						hudChat->SetCustomColor(color);
					}
					else
					{
						const char *pszQualityColorString = EconQuality_GetColorString((EEconItemQuality)iItemQuality);
						if (pszQualityColorString)
						{
							hudChat->SetCustomColor(pszQualityColorString);
						}
					}

					*(colorMarker + 1) = COLOR_CUSTOM;
				}

				// TODO: Update the localization strings to only have two format parameters since that's all we need.
				locchar_t wszLocalizedString[256];

				locchar_t szItemname[64] = LOCCHAR("");
				locchar_t szRarity[64] = LOCCHAR("");
				locchar_t szWear[64] = LOCCHAR("");
				locchar_t szStrange[64] = LOCCHAR("");
				locchar_t szUnusual[64] = LOCCHAR("");

				loc_scpy_safe(
					szItemname,
					CConstructLocalizedString(g_pVGuiLocalize->Find("TFUI_InvTooltip_ItemFound_Itemname"),
						CEconItemLocalizedFullNameGenerator(GLocalizationProvider(), pItemDefinition, iItemQuality).GetFullName())
				);

				/*g_pVGuiLocalize->ConstructString_safe(
				szItemname,
				LOCCHAR( "%s1 " ),
				1,
				CEconItemLocalizedFullNameGenerator( GLocalizationProvider(), pItemDefinition, iItemQuality ).GetFullName()
				);*/

				locchar_t tempName[MAX_ITEM_NAME_LENGTH];
				// If items have rarity
				if (pItemRarity)
				{
					// Weapon Wear
					if (!IsWearableSlot(pItemDefinition->GetDefaultLoadoutSlot()))
					{
						loc_scpy_safe(szWear, CConstructLocalizedString(g_pVGuiLocalize->Find("TFUI_InvTooltip_ItemFound_Wear"), g_pVGuiLocalize->Find(GetWearLocalizationString(flWear))));
					}

					// Rarity / grade
					loc_scpy_safe(szRarity, CConstructLocalizedString(g_pVGuiLocalize->Find("TFUI_InvTooltip_ItemFound_Rarity"), g_pVGuiLocalize->Find(pItemRarity->GetLocKey())));
				}

				if (bIsUnusual)
				{
					loc_scpy_safe(szUnusual, CConstructLocalizedString(g_pVGuiLocalize->Find("TFUI_InvTooltip_ItemFound_Unusual"), g_pVGuiLocalize->Find("rarity4")));
				}

				if (bIsStrange)
				{
					loc_scpy_safe(szStrange, CConstructLocalizedString(g_pVGuiLocalize->Find("TFUI_InvTooltip_ItemFound_Strange"), g_pVGuiLocalize->Find("strange")));
				}

				// // Strange Unusual Item Grade 		
				loc_scpy_safe(wszLocalizedString, CConstructLocalizedString(g_pVGuiLocalize->Find("TFUI_InvTooltip_ItemFound"), szStrange, szUnusual, szItemname, szRarity, szWear));

				loc_scpy_safe(tempName, wszLocalizedString);
				g_pVGuiLocalize->ConstructString_safe(
					wszLocalizedString,
					wszItemFound,
					3,
					wszPlayerName, tempName, L"");

				char szLocalized[256];
				g_pVGuiLocalize->ConvertUnicodeToANSI(wszLocalizedString, szLocalized, sizeof(szLocalized));

				hudChat->ChatPrintf(iPlayerIndex, CHAT_FILTER_SERVERMSG, "%s", szLocalized);
			}
		}
			}
#endif
#if defined( REPLAY_ENABLED )
	else if (!V_strcmp("replay_servererror", eventname))
	{
		DisplayReplayMessage(event->GetString("error", "#Replay_DefaultServerError"), replay_msgduration_error.GetFloat(), true, NULL, false);
	}
	else if (!V_strcmp("replay_startrecord", eventname))
	{
		m_flReplayStartRecordTime = gpGlobals->curtime;
	}
	else if (!V_strcmp("replay_endrecord", eventname))
	{
		m_flReplayStopRecordTime = gpGlobals->curtime;
	}
	else if (!V_strcmp("replay_replaysavailable", eventname))
	{
		DisplayReplayMessage("#Replay_ReplaysAvailable", replay_msgduration_replaysavailable.GetFloat(), false, NULL, false);
	}

	else if (!V_strcmp("game_newmap", eventname))
	{
		// Make sure the instance count is reset to 0.  Sometimes the count stay in sync and we get replay messages displaying lower than they should.
		CReplayMessagePanel::RemoveAll();
	}
#endif

	else
	{
		DevMsg(2, "Unhandled GameEvent in ClientModeShared::FireGameEvent - %s\n", event->GetName());
	}
		}





//-----------------------------------------------------------------------------
// In-game VGUI context 
//-----------------------------------------------------------------------------
void ClientModeShared::ActivateInGameVGuiContext( vgui::Panel *pPanel )
{
	vgui::ivgui()->AssociatePanelWithContext( s_hVGuiContext, pPanel->GetVPanel() );
	vgui::ivgui()->ActivateContext( s_hVGuiContext );
}

void ClientModeShared::DeactivateInGameVGuiContext()
{
	vgui::ivgui()->ActivateContext( DEFAULT_VGUI_CONTEXT );
}

int ClientModeShared::GetSplitScreenPlayerSlot() const
{
	int nSplitScreenUserSlot = vgui::ipanel()->GetMessageContextId( m_pViewport->GetVPanel() );
	Assert( nSplitScreenUserSlot != -1 );
	return nSplitScreenUserSlot;
}
