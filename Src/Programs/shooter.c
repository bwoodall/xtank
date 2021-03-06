/*-
 * Copyright (c) 1988 Terry Donahue
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <xtanklib.h>
#include "sysdep.h"
#include <stdio.h>
#include <math.h>

static void main();

Prog_desc shooter_prog = {
    "shooter",
    "BASE",
    "Sits in one place and fires at the nearest enemy.",
    "stripes@eng.umd.edu (orig. Terry Donahue)",
    PLAYS_COMBAT | DOES_SHOOT | USES_TEAMS,
    1,
    main
};

#define ENEMY_NONE		0
#define ENEMY_RADAR		1
#define ENEMY_SCREEN	2


typedef struct
{
	int num;					/* #weps in turret */
	Weapon_info *winfo[MAX_WEAPONS];
} tinf;							/* "Turret info" */

typedef struct
{
	int nt;
	Weapon_info weap[MAX_WEAPONS];
	tinf tinfo[MAX_TURRETS];
	Team team;
} shooter_info;


static void main()
{
	Vehicle_info enemy;
	Location myloc;
	int enemy_flag;
	shooter_info data;
	shooter_info *sinfo = &data;
	extern int frame;
	int cur_frame;


	/* Find out info about weapons */
	shooter_init(sinfo);

	for (;;)
	{
		/* figure out where we are */
		get_location(&myloc);

		/* find the nearest enemy */
		enemy_flag = shooter_find(&myloc, sinfo->team, &enemy);

		/* try to shoot at enemy if on screen and we have a weapon */
		if (enemy_flag == ENEMY_SCREEN)
			shooter_shoot(sinfo, &myloc, &enemy);

		/* Give up remaining cpu time to improve speed of game */
		cur_frame = frame;
		done();
		if (cur_frame == frame)
		{
			char *crash = NULL;

			/* assert apparently doesn't work too well... */
			*crash = 0;
		}
	}
}

/*
** Looks for vehicles on the screen.  Puts closest vehicle with
** a clear path to it into t and returns ENEMY_SCREEN.  If no
** such vehicle, returns ENEMY_NONE.
*/
shooter_find(myloc, myteam, t)
Location *myloc;
Team myteam;
Vehicle_info *t;
{
	Vehicle_info vinfo[MAX_VEHICLES];
	int num_vinfos;
	Vehicle_info *v;
	int dx, dy, range, min_range;
	int i;

	get_vehicles(&num_vinfos, vinfo);

	/* * Find the closest vehicle with a clear path to it. */
	min_range = 99999999;
	for (i = 0; i < num_vinfos; i++)
	{
		v = &vinfo[i];

		/* ignore vehicles on the same team */
		if (myteam == v->team && myteam != NEUTRAL)
			continue;

		/* ignore vehicles that have no clear path to them */
		if (!clear_path(myloc, &v->loc))
			continue;

		dx = v->loc.x - myloc->x;
		dy = v->loc.y - myloc->y;
		range = dx * dx + dy * dy;
		if (range < min_range)
		{
			min_range = range;
			*t = *v;
		}
	}

	if (min_range == 99999999)
		return ENEMY_NONE;
	else
		return ENEMY_SCREEN;
}

/*
** Shoots at vehicle t with all weapons.  Uses quick leading fan algorithm.
*/
shooter_shoot(sinfo, myloc, t)
    shooter_info *sinfo;
    Location *myloc;
    Vehicle_info *t;
{
    extern FLOAT turret_angle(), aim_turret();
    extern int frame;
    int dx, dy, range;
    FLOAT lead_factor, ang;
    FLOAT kludge;
    int i, j, k;
    int weap_type;  /* changed to int (HAK 4/93) */
    Weapon_info *wi, *twi;

    dx = t->loc.x - myloc->x;
    dy = t->loc.y - myloc->y;

    range = dx * dx + dy * dy;
    for (i = 0; i < sinfo->nt; i++)
    {
	for (j = 0; j < sinfo->tinfo[i].num; j++)
	{
	    wi = sinfo->tinfo[i].winfo[j];
	    if (range > wi->range)
		continue;
	    /* Lead the target approximately, shoot fanning */
	    lead_factor = 2 * SQRT(0.0 + range) / wi->ammo_speed;
	    dx += (int) (t->xspeed * lead_factor * (FLOAT) rnd(20) / 19.0);
	    dy += (int) (t->yspeed * lead_factor * (FLOAT) rnd(20) / 19.0);

	    /* Point the turret towards where he is going to be */
            ang = aim_turret((TurretNum)i, dx, dy);

	    /* Forget shooting if you won't hit... */
	    /* (I want a better algo..) */
            kludge = ang - turret_angle((TurretNum)i);
	    ang = remainder(kludge, (2.0 * PI));
	    /* ang = (ang - turret_angle(i)) % 2.0*PI; */
	    ang = (ang <= 0) ? -ang : ang;

#ifdef DEBUG
	    if (ang >= PI / 8)
	    {
		fprintf(stderr, "turret %d off by %f, frame %d skiping\n", i, ang, frame);
		break;
	    }
#endif

	    /* Shoot the weapon */
	    weap_type = wi->type;
	    fire_weapon(wi - sinfo->weap);
	    for (twi = wi + 1, k = j + 1; k < sinfo->tinfo[i].num; k++, twi++)
	    {
		if (twi->type == weap_type)
		    fire_weapon(twi - sinfo->weap);
	    }
	    break;
	}
    }
}

shooter_init(sinfo)
shooter_info *sinfo;
{
	int i, tnum, j, numweaps;
	Weapon_info *w, *wx;
	tinf *ti;
	Vehicle_info v;

	get_self(&v);
	sinfo->team = v.team;

	for (i = 0; i < MAX_TURRETS; i++)
	{
		sinfo->tinfo[i].num = 0;
	}
	sinfo->nt = 0;

	numweaps = num_weapons();
	for (i = 0; i < numweaps; i++)
	{
		w = &sinfo->weap[i];
		get_weapon(i, w);

		switch (w->mount)
		{
			case (MOUNT_TURRET1):
				tnum = 0;
				break;
			case (MOUNT_TURRET2):
				tnum = 1;
				break;
			case (MOUNT_TURRET3):
				tnum = 2;
				break;
			case (MOUNT_TURRET4):
				tnum = 3;
				break;
			default:
				fprintf(stderr, "shooter_init bad mount=%d, ignored\n", w->mount);
		}
		w->range *= w->range;
		ti = sinfo->tinfo + tnum;
		for (j = 0; j < ti->num; j++)
		{
			if (ti->winfo[j]->range > w->range)
			{
				/* struct assign is ANSI std, you can memcy if you have a
				   losing compiler... or get gcc */
				wx = w;
				w = ti->winfo[j];
				ti->winfo[j] = wx;
			}
		}
		ti->winfo[j] = w;
		ti->num += 1;
		if (tnum >= sinfo->nt)
			sinfo->nt = tnum + 1;
	}
	/* Next do secondary sort baised on sinfo->tinfo[*].winfo[*].damage */
	/* (want big to low) */

#if 0
	for (i = 0; i < sinfo->nt; i++)
	{
		ti = sinfo->tinfo;
		for (j = 0; j < ti->num; j++)
		{
			fprintf(stderr, "turret %d, wep %d, range %d\n", i, j, ti->winfo[j]->range);
		}
	}
#endif
}
