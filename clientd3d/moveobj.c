// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
* moveobj.c:  Move objects other than the player.
*
* We occasionally hear from the server that an object has moved.  When this happens, we
* set up the "motion" structure in the room object for smooth motion from its current position
* to its new position.  If the object was already in motion, we adjust the motion to end
* at the new position and adjust the speed accordingly.
*/

#include "client.h"

#define OBJECT_BOUNCE_HEIGHT  (FINENESS >> 4)
#define TIME_FULL_OBJECT_BOUNCE 2000

// if the current position of an object differs more than this value
// from a new position received in BP_MOVE, the move will be applied
// as an instant teleport, even if BP_MOVE contains a SPEED other than 0
// unit here is big/rows cols
#define OBJECT_TELEPORTMOVE_OFFSET 2.5f

extern player_info player;
extern room_type current_room;

static void MoveSingleVertically(Motion *m, int dt);
/************************************************************************/
/*  
* MoveObject2:  Server has told us to move given object to given coordinates.
	Now performs auto-correction whenever something (lag/incorrect speed) causes it
	to fall behind or ahead - mistery
*/
void MoveObject2(ID object_id, int x, int y, BYTE speed, BOOL turnToFace)
{
	room_contents_node *r;
	int dx, dy, z;
	BOOL hanging;
	
	r = GetRoomObjectById(object_id);
	
	if (r == NULL)
	{
		debug(("Couldn't find object #%ld to move\n", object_id));
		return;
	}
	
	hanging = (r->obj.flags & OF_HANGING);
	z = GetFloorBase(x,y);
	if (turnToFace)
	{
		int angle = -1;
		dx = x - r->motion.x;
		dy = y - r->motion.y;
		/* Turn object to face direction traveling */
		if (dy > 0)
		{
			if (dx > 0)
				angle = 1;
			else if (dx < 0)
				angle = 3;
			else
				angle = 2;
		}
		else if (dy < 0)
		{
			if (dx > 0)
				angle = 7;
			else if (dx < 0)
				angle = 5;
			else
				angle = 6;
		}
		else // dy = 0
		{
			if (dx > 0)
				angle = 0;
			else if (dx < 0)
				angle = 4;
		}
		if (angle > -1)
			TurnObject(object_id,(WORD)(angle * MAX_ANGLE / 8));
	}
	
	RedrawAll();
	
	/* Player moves specially */
	if (object_id == player.id)
	{
		player.x = x;
		player.y = y;
		
		r->motion.x = x;
		r->motion.y = y;
		
		RoomObjectSetHeight(r);
		ServerMovedPlayer();
		
		// Don't interpolate or animate our own motion
		return;
	}

	// If animation off, don't interpolate motion
	if (speed == 0 || !config.animate)
	{
		r->motion.x = x;
		r->motion.y = y;
		RoomObjectSetHeight(r);
		RoomObjectSetAnimation(r, False);
		r->moving = False;
		return;
	}
	
	// Set up interpolated motion
	// See how far we should move per frame
	r->motion.progress = 0.0;
   r->motion.speed = speed;

   // lenMove: distance from supposed position to new destination (in server big rows/cols)
   // this is the distance we would have to travel, if we had been at supposed dest already
   dx = r->motion.dest_x - x;
   dy = r->motion.dest_y - y;
   float lenMove = GetFloatSqrt((float)(dx * dx + dy * dy)) / (float)FINENESS;

   // lenMoveReal: distance from current position to new destination (in server big rows/cols)
   //  this is the distance we actually have to travel next
   dx = r->motion.x - x;
   dy = r->motion.y - y;
   float lenMoveReal = GetFloatSqrt((float)(dx * dx + dy * dy)) / (float)FINENESS;

   // if the distance to the new destination is bigger than defined threshold
   // the move will be applied as an instant teleport regardless of attached speed
   // e.g. this teleports all objects after a big LOCAL lagspike to their most recent position
   // it will also instantly set a lagged playerobject to latest position after his/her lagspike
   if (lenMoveReal > OBJECT_TELEPORTMOVE_OFFSET)
   {
      debug(("Object %i exceeded move distance offset, teleporting to latest position. \n", r->obj.id));
      r->motion.increment = r->motion.incrementstart = 1.0f;
   }

   else
   {
      if (r->moving && lenMove > 0.01f)
      {
         // If already in motion, set things up so that combined motions will end at the same
         // time that new motion would end if existing motion were not present.
         float ratio = lenMoveReal / lenMove;

         // ratio > 1.0f: speed up (farer than supposed)
         // ratio < 1.0f: slow down (closer than supposed)
         // reduce speedups by 50% (e.g. 1.5 -> 1.25)
         if (ratio > 1.0f)
            ratio = 1.0f + ((ratio - 1.0f) * 0.5f);

         // use weighted sum of existing speedfactor and suggested speedfactor
         r->motion.speed_factor = 0.75f * r->motion.speed_factor + 0.25f * ratio;
      }
      else
         r->motion.speed_factor = 1.0f;

      // apply the current speed adjustment factor on the speed
      float speedmodified = r->motion.speed * r->motion.speed_factor;

      // Object motion is given in # of grid squares per 10 seconds
      // Increment is percent of move per 1ms
      r->motion.increment = r->motion.incrementstart =
         (speedmodified / 10000.0f) / lenMoveReal;
   }

   //////////////////////////

   // set current position as new source
   r->motion.source_x = r->motion.x;
   r->motion.source_y = r->motion.y;
   r->motion.source_z = r->motion.z;

   // set received destination as new destination
   r->motion.dest_x = x;
   r->motion.dest_y = y;
   r->motion.dest_z = z;

   //////////////////////////

   //debug(("continue %i speed %f lenMove %f lenMoveReal %f speedfactor %f tick %i \n", 
   //   r->moving, r->motion.speed, lenMove, lenMoveReal, r->motion.speed_factor, GetTickCount()));

   //////////////////////////

   r->motion.move_animating = True;
   r->moving = True;

   if (hanging)
   {
      RoomObjectSetHeight(r);
      r->motion.v_z = 0;
   }
   else if (z < r->motion.source_z)
      r->motion.v_z = FALL_VELOCITY_0;
   else if (z > r->motion.source_z)
      r->motion.v_z = CLIMB_VELOCITY_0;

   RoomObjectSetAnimation(r, True);
}
/************************************************************************/
/*  
* ObjectsMove:  Called when animation timer goes off.  Incrementally move
*   objects whose motion is interpolated.
*   dt is number of milliseconds since last time animation timer went off.
*   Return True iff at least one object was moved.
*/
Bool ObjectsMove(int dt)
{
	list_type l;
	Bool retval = False;
	
	for (l = current_room.contents; l != NULL; l = l->next)
	{
		room_contents_node *r = (room_contents_node *) (l->data);
		
		if (r->obj.boundingHeight == 0)
		{
			int width,height;
			if(GetObjectSize(r->obj.icon_res, r->obj.animate->group, 0, *(r->obj.overlays), 
				&width, &height))
			{
				r->obj.boundingHeight = height;
				r->obj.boundingWidth = width;
			}
		}
		if (r->moving)
		{
			retval = True;
			if (MoveSingle(&r->motion, dt))
			{
				// Object has finished its interpolated move
				r->moving = False;
				
				// Restore original animation, if using move animation
				if (r->motion.move_animating)
					RoomObjectSetAnimation(r, False);
				
				r->motion.move_animating = False;
			}
			
			// Rest object on floor if it's not moving vertically
			if (r->motion.v_z == 0)
				RoomObjectSetHeight(r);
		}
		if (OF_BOUNCING == (OF_BOUNCING & r->obj.flags))
		{
			if (!(OF_PLAYER & r->obj.flags))
			{
				int floor,ceiling,angleBounce,bounceHeight;
				r->obj.bounceTime += min(dt,40);
				if (r->obj.bounceTime > TIME_FULL_OBJECT_BOUNCE)
					r->obj.bounceTime -= TIME_FULL_OBJECT_BOUNCE;
				angleBounce = NUMDEGREES * r->obj.bounceTime / TIME_FULL_OBJECT_BOUNCE;
				bounceHeight = FIXED_TO_INT(fpMul(OBJECT_BOUNCE_HEIGHT, SIN(angleBounce)));
				if (GetPointHeights(r->motion.x,r->motion.y,&floor,&ceiling))
				{
					//int midPoint = floor + ((ceiling-floor)>>1);
					r->motion.z = floor + OBJECT_BOUNCE_HEIGHT + bounceHeight;
				}
				retval = True;
			}
		}
		if (r->motion.v_z != 0)
		{
			retval = True;
			MoveSingleVertically(&r->motion, dt);
		}
	}
	return retval;
}
/************************************************************************/
/*
* MoveSingle:  Move object described by given motion structure along its
*   path.  Return True if object has reached the end of its motion.
*   dt is number of milliseconds since last time animation timer went off.
*/
Bool MoveSingle(Motion *m, int dt)
{
	m->progress += (m->increment * dt);
	if (m->progress >= 1.0f)
	{
		m->x = m->dest_x;
		m->y = m->dest_y;
		m->z = m->dest_z;
		return True;
	}
	
	m->x = FloatToInt(m->source_x + m->progress * (m->dest_x - m->source_x));
	m->y = FloatToInt(m->source_y + m->progress * (m->dest_y - m->source_y));
	m->z = FloatToInt(m->source_z + m->progress * (m->dest_z - m->source_z));

   // this decreases replay speed towards the end (per frame/per ~16ms@60FPS)
   // helps with late BP_MOVE intervals, ranges from 0% @ 0% to 20% @ 100%
   m->increment = m->incrementstart * (1.0f - m->progress * 0.2f);
   
	return False;
}
/************************************************************************/
/*
* MoveSingleVertically:  Move object described by given motion structure 
*   vertically up or down.  Simulates gravity on falling objects.
*   dt is number of milliseconds since last time animation timer went off.
*/

float gravityAdjust = 1.0f;

void MoveSingleVertically(Motion *m, int dt)
{
	int dz = dt * m->v_z / 1000;
	
	m->z += FloatToInt((float)dz * gravityAdjust);
	if (dz > 0)   // Rising
	{
		if (m->z >= m->dest_z)
		{
			// Reached destination height; stop rising
			m->z = m->dest_z;
			m->v_z = 0;
		}
	}
	else          // Falling
	{
		if (m->z <= m->dest_z)
		{
			// Reached destination height; stop falling
			m->z = m->dest_z;
			m->v_z = 0;
		}
		else
		{
			// Constant acceleration of gravity
			m->v_z += FloatToInt(gravityAdjust * (float)(GRAVITY_ACCELERATION * dt / 1000));
		}
	}
}

