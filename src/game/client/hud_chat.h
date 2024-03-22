//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef HUD_CHAT_H
#define HUD_CHAT_H
#ifdef _WIN32
#pragma once
#endif

#include <hud_basechat.h>

class CHudChat : public CBaseHudChat
{
	DECLARE_CLASS_SIMPLE( CHudChat, CBaseHudChat );

public:
	explicit CHudChat( const char *pElementName );

	virtual void	Init( void );

	bool			MsgFunc_SayText(const CUsrMsg_SayText &msg);
	bool			MsgFunc_SayText2( const CUsrMsg_SayText2 &msg);
	bool			MsgFunc_TextMsg(const CUsrMsg_TextMsg &msg);
	// Binders
	CUserMessageBinder m_UMCMsgSayText;
	CUserMessageBinder m_UMCMsgSayText2;
	CUserMessageBinder m_UMCMsgTextMsg;
};

#endif	//HUD_CHAT_H