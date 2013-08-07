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

#ifndef	ACTIVITY_H
#define	ACTIVITY_H


typedef enum {
	ACT_RESET_HL1 = 0,		// Set m_Activity to this invalid value to force a reset to m_IdealActivity
	ACT_IDLE_HL1 = 1,
	ACT_GUARD_HL1,
	ACT_WALK_HL1,
	ACT_RUN_HL1,
	ACT_FLY_HL1,				// Fly (and flap if appropriate)
	ACT_SWIM_HL1,
	ACT_HOP_HL1,				// vertical jump
	ACT_LEAP_HL1,				// long forward jump
	ACT_FALL_HL1,
	ACT_LAND_HL1,
	ACT_STRAFE_LEFT_HL1,
	ACT_STRAFE_RIGHT_HL1,
	ACT_ROLL_LEFT_HL1,			// tuck and roll, left
	ACT_ROLL_RIGHT_HL1,			// tuck and roll, right
	ACT_TURN_LEFT_HL1,			// turn quickly left (stationary)
	ACT_TURN_RIGHT_HL1,			// turn quickly right (stationary)
	ACT_CROUCH_HL1,				// the act of crouching down from a standing position
	ACT_CROUCHIDLE_HL1,			// holding body in crouched position (loops)
	ACT_STAND_HL1,				// the act of standing from a crouched position
	ACT_USE_HL1,
	ACT_SIGNAL1_HL1,
	ACT_SIGNAL2_HL1,
	ACT_SIGNAL3_HL1,
	ACT_TWITCH_HL1,
	ACT_COWER_HL1,
	ACT_SMALL_FLINCH_HL1,
	ACT_BIG_FLINCH_HL1,
	ACT_RANGE_ATTACK1_HL1,
	ACT_RANGE_ATTACK2_HL1,
	ACT_MELEE_ATTACK1_HL1,
	ACT_MELEE_ATTACK2_HL1,
	ACT_RELOAD_HL1,
	ACT_ARM_HL1,				// pull out gun, for instance
	ACT_DISARM_HL1,				// reholster gun
	ACT_EAT_HL1,				// monster chowing on a large food item (loop)
	ACT_DIESIMPLE_HL1,
	ACT_DIEBACKWARD_HL1,
	ACT_DIEFORWARD_HL1,
	ACT_DIEVIOLENT_HL1,
	ACT_BARNACLE_HIT_HL1,		// barnacle tongue hits a monster
	ACT_BARNACLE_PULL_HL1,		// barnacle is lifting the monster ( loop )
	ACT_BARNACLE_CHOMP_HL1,		// barnacle latches on to the monster
	ACT_BARNACLE_CHEW_HL1,		// barnacle is holding the monster in its mouth ( loop )
	ACT_SLEEP_HL1,
	ACT_INSPECT_FLOOR_HL1,		// for active idles, look at something on or near the floor
	ACT_INSPECT_WALL_HL1,		// for active idles, look at something directly ahead of you ( doesn't HAVE to be a wall or on a wall )
	ACT_IDLE_ANGRY_HL1,			// alternate idle animation in which the monster is clearly agitated. (loop)
	ACT_WALK_HURT_HL1,			// limp  (loop)
	ACT_RUN_HURT_HL1,			// limp  (loop)
	ACT_HOVER_HL1,				// Idle while in flight
	ACT_GLIDE_HL1,				// Fly (don't flap)
	ACT_FLY_LEFT_HL1,			// Turn left in flight
	ACT_FLY_RIGHT_HL1,			// Turn right in flight
	ACT_DETECT_SCENT_HL1,		// this means the monster smells a scent carried by the air
	ACT_SNIFF_HL1,				// this is the act of actually sniffing an item in front of the monster
	ACT_BITE_HL1,				// some large monsters can eat small things in one bite. This plays one time, EAT loops.
	ACT_THREAT_DISPLAY_HL1,		// without attacking, monster demonstrates that it is angry. (Yell, stick out chest, etc )
	ACT_FEAR_DISPLAY_HL1,		// monster just saw something that it is afraid of
	ACT_EXCITED_HL1,			// for some reason, monster is excited. Sees something he really likes to eat, or whatever.
	ACT_SPECIAL_ATTACK1_HL1,	// very monster specific special attacks.
	ACT_SPECIAL_ATTACK2_HL1,	
	ACT_COMBAT_IDLE_HL1,		// agitated idle.
	ACT_WALK_SCARED_HL1,
	ACT_RUN_SCARED_HL1,
	ACT_VICTORY_DANCE_HL1,		// killed a player, do a victory dance.
	ACT_DIE_HEADSHOT_HL1,		// die, hit in head. 
	ACT_DIE_CHESTSHOT_HL1,		// die, hit in chest
	ACT_DIE_GUTSHOT_HL1,		// die, hit in gut
	ACT_DIE_BACKSHOT_HL1,		// die, hit in back
	ACT_FLINCH_HEAD_HL1,
	ACT_FLINCH_CHEST_HL1,
	ACT_FLINCH_STOMACH_HL1,
	ACT_FLINCH_LEFTARM_HL1,
	ACT_FLINCH_RIGHTARM_HL1,
	ACT_FLINCH_LEFTLEG_HL1,
	ACT_FLINCH_RIGHTLEG_HL1,
} ActivityHL1;


typedef struct {
	int	type;
	char *name;
} activity_map_t;

extern activity_map_t activity_map[];


#endif	//ACTIVITY_H
