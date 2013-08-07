//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Network data for screen shake and screen fade.
//
// $NoKeywords: $
//=============================================================================//

#ifndef SHAKE_H
#define SHAKE_H
#ifdef _WIN32
#pragma once
#endif


//
// Commands for the screen shake effect.
//

struct ScreenShake_t
{
	int		command;
	float	amplitude;
	float	frequency;
	float	duration;
};

enum ShakeCommand_t
{
	SHAKE_START = 0,		// Starts the screen shake for all players within the radius.
	SHAKE_STOP,				// Stops the screen shake for all players within the radius.
	SHAKE_AMPLITUDE,		// Modifies the amplitude of an active screen shake for all players within the radius.
	SHAKE_FREQUENCY,		// Modifies the frequency of an active screen shake for all players within the radius.
	SHAKE_START_RUMBLEONLY,	// Starts a shake effect that only rumbles the controller, no screen effect.
	SHAKE_START_NORUMBLE,	// Starts a shake that does NOT rumble the controller.
};


//
// Screen shake message.
//
extern int gmsgShake;

// Fade in/out
extern int gmsgFade;

#define FFADE_IN			0x0001		// Just here so we don't pass 0 into the function
#define FFADE_OUT			0x0002		// Fade out (not in)
#define FFADE_MODULATE		0x0004		// Modulate (don't blend)
#define FFADE_STAYOUT		0x0008		// ignores the duration, stays faded out until new ScreenFade message received
#define FFADE_PURGE			0x0010		// Purges all other fades, replacing them with this one

#define SCREENFADE_FRACBITS		9		// which leaves 16-this for the integer part
// This structure is sent over the net to describe a screen fade event
struct ScreenFade_t
{
	unsigned short 	duration;		// FIXED 16 bit, with SCREENFADE_FRACBITS fractional, seconds duration
	unsigned short 	holdTime;		// FIXED 16 bit, with SCREENFADE_FRACBITS fractional, seconds duration until reset (fade & hold)
	short			fadeFlags;		// flags
	byte			r, g, b, a;		// fade to color ( max alpha )
};

// HL1

// Screen / View effects

// screen shake
extern int gmsgShakeHL1;

// This structure is sent over the net to describe a screen shake event
typedef struct
{
	unsigned short	amplitude;		// FIXED 4.12 amount of shake
	unsigned short 	duration;		// FIXED 4.12 seconds duration
	unsigned short	frequency;		// FIXED 8.8 noise frequency (low frequency is a jerk,high frequency is a rumble)
} ScreenShake;

extern void V_ApplyShake( float *origin, float *angles, float factor );
extern void V_CalcShake( void );
extern int V_ScreenShake( const char *pszName, int iSize, void *pbuf );
extern int V_ScreenFade( const char *pszName, int iSize, void *pbuf );


// Fade in/out
extern int gmsgFadeHL1;

#define FFADE_IN_HL1			0x0000		// Just here so we don't pass 0 into the function
#define FFADE_OUT_HL1			0x0001		// Fade out (not in)
#define FFADE_MODULATE_HL1		0x0002		// Modulate (don't blend)
#define FFADE_STAYOUT_HL1		0x0004		// ignores the duration, stays faded out until new ScreenFade message received

// This structure is sent over the net to describe a screen fade event
typedef struct
{
	unsigned short 	duration;		// FIXED 4.12 seconds duration
	unsigned short 	holdTime;		// FIXED 4.12 seconds duration until reset (fade & hold)
	short			fadeFlags;		// flags
	byte			r, g, b, a;		// fade to color ( max alpha )
} ScreenFade;


#endif // SHAKE_H
