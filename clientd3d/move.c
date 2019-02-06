// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * move.c:  Handle user motion in room.
 *
 * Moves are performed optimistically:  we inform the server of each move and update the
 * user's position without waiting for a response from the server.  The server can reject
 * a move by telling us to move the user back to his original position.
 *
 * A user's move is divided into a sequence of equally sized small "steps".  At each step,
 * we check if the user has moved off the room, too close to a wall, or across a grid 
 * line.  This allows us to check walls and grid lines in a straightforward way, without
 * worrying about a large move that crosses multiple grid lines, etc.  On fast machines,
 * dividing a small move into many steps can result in roundoff errors, so the number of steps
 * per move depends on the amount of time since the previous move.
 *
 * To check if a move is too close to a wall, we traverse the room's BSP tree.  Nodes
 * whose bounding boxes are too far away from the user's position are ignored.  For other
 * nodes, we first check the distance to each wall in the node, and then descend to the
 * node's children.  If a move is initially disallowed, we try to "slide" along the wall
 * in the way by moving parallel to it (i.e. taking the component of the move along the wall.)
 *
 * If a player's altitude changes as a result of a move, we set the player's vertical velocity
 * field (v_z).  This causes the player to climb steps and fall with gravity if animation is on.
 *
 * Since we need to wait for a response from the server before changing rooms,
 * it's wasteful to keep requesting room changes while waiting for the response.
 * In addition, the extra off-room moves generated while waiting for the response can
 * cause the player to teleport one he does change rooms.  Thus, we delay about a second
 * between consecutive attempts at moving off a room.
 *
 * The kod sends a "moveon" type with each object.  Most objects can be moved onto, but
 * the user cannot enter the square of such things such as monsters and other players.
 * We disallow these moves here to prevent the kod from having to reject them.  Teleporters
 * are a special case:  we are allowed to move onto them, but then the kod will move the
 * user to another square.  After moving onto a teleporter, we prevent any further user 
 * moves until the kod moves the user.
 */

#include "client.h"

#define MAXINTERSECTIONS 2048
typedef struct Intersection
{
   Sector* SectorS;
   Sector* SectorE;
   Sidedef* SideS;
   Sidedef* SideE;
   WallData* Wall;
   V2 Q;
   float FloorHeight;
   float Distance2;
} Intersection;

typedef struct Intersections
{
   Intersection  Data[MAXINTERSECTIONS];
   Intersection* Ptrs[MAXINTERSECTIONS];
   unsigned int  Size;
} Intersections;

// distance from a point (b) to a BspInternal (a)
#define DISTANCETOSPLITTERSIGNED(n,p) ((n).a * (p)->X + (n).b * (p)->Y + (n).c)
// true if point (c) lies inside boundingbox defined by min/max of (a) and (b)
#define ISINBOXINT(p0x, p0y, p1x, p1y, c) \
   (min(p0x, p1x) - EPSILONBIG <= (c)->X && (c)->X <= max(p0x, p1x) + EPSILONBIG && \
    min(p0y, p1y) - EPSILONBIG <= (c)->Y && (c)->Y <= max(p0y, p1y) + EPSILONBIG)

#define QUARTERPERIOD (0.25f * PITWICE)       // Radian of quarter period
#define GRAVITYACCELERATION FINENESSKODTOROO(-0.00032f) // dist/ms² (dist=1:1024)
#define MOVE_DELAY    100                     // Minimum # of milliseconds between moving MOVEUNITS
#define TURN_DELAY    100                     // Minimum number of milliseconds between a full turn action
#define MAXSTEPHEIGHT (HeightKodToClient(24)) // Max vertical step size (FINENESS units)
#define MOVE_INTERVAL  100                    // Inform server once per this many milliseconds
#define MOVE_THRESHOLD (FINENESS / 8)         // Inform server only of moves at least this large
#define MIN_NOMOVEON (FINENESS / 4)           // Closest we can get to a nomoveon object
#define TELEPORT_DELAY 1500                   // # of milliseconds to wait for server to teleport us
#define USER_WALKING_SPEED 25                 // Base walking speed.
#define USER_RUNNING_SPEED 50                 // Base running speed.
#define MOVE_OFF_ROOM_INTERVAL 1000           // Milliseconds between retrying to change rooms
#define BOUNCE_HEIGHT (FINENESS >> 5)         // Amplitude of player's bouncing motion

extern player_info player;
extern room_type current_room;
extern int sector_depths[];
extern DWORD latency;
extern float gravityAdjust;

// Time when we last tried to move off room
static DWORD move_off_room_time = 0;
/* Offset from player's average height to give appearance of bouncing */
static long bounce_height;
/* Offset from player's average height to allow user to look up or down */
static long height_offset;
static DWORD server_time = 0;           // Last time we informed server of our position
static DWORD last_splash = 0;           // Time of the last play of the splash wading sound
static DWORD last_go_time = 0;
static Bool pos_valid = FALSE;          // True when server_pos is valid
static V2 last_server_pos;              // Last position we've told server we are, in FINENESS units
static int last_server_angle = 0;       // Last angle we've told server we have, in server angle units
static float last_server_speed = 0.0f;  // Last speed used to move.
static int movespeed_pct  = 100;        // slow down of base speed for spell/item reasons
static int min_distance = 48;           // Minimum distance player is allowed to get to wall
static int min_distance2 = 48*48;       // Minimum distance squared
WallData *lastBlockingWall = NULL;      // Reference to last blocking wall for map drawing
Intersections intersects;               // stores potential intersections during recursive CanMoveInRoom3D query

/* local function prototypes */
static bool IsInRoom(int x, int y, room_type *room);
static void BounceUser(int dt);
static void VerifyMove(V3* Start, V2* End, V2* NewEnd, float Speed);
static void SlideAlongWall(WallData* wall, V2* Start, V2* End);

/************************************************************************/
void ResetPlayerPosition(void)
{
   lastBlockingWall = NULL;
   min_distance = player.width / 2;
   min_distance2 = min_distance * min_distance;
}
/************************************************************************/
void UserTryGo()
{
   // Record the time of the last go.
   last_go_time = timeGetTime();
   RequestGo();
}
/************************************************************************/
__forceinline void IntersectionsSwap(Intersection **a, Intersection **b)
{
   Intersection* t = *a; *a = *b; *b = t;
}
/************************************************************************/
void IntersectionsSort(Intersection* arr[], unsigned int beg, unsigned int end)
{
   // Recursive QuickSort implementation
   // sorting (potential) intersections by squared distance from start

   if (end > beg + 1)
   {
      Intersection* piv = arr[beg];
      unsigned int l = beg + 1, r = end;
      while (l < r)
      {
         const Intersection* itm = arr[l];
         if (itm->Distance2 < piv->Distance2 || (itm->Distance2 == piv->Distance2 && itm->FloorHeight < piv->FloorHeight))
            l++;
         else
            IntersectionsSwap(&arr[l], &arr[--r]);
      }
      IntersectionsSwap(&arr[--l], &arr[beg]);
      IntersectionsSort(arr, beg, l);
      IntersectionsSort(arr, r, end);
   }
}
/************************************************************************/
__forceinline bool IntersectLineSplitter(const BSPnode* Node, const V2* S, const V2* E, V2* P)
{
   // get 2d line equation coefficients for infinite line through S and E
   double a1, b1, c1;
   a1 = E->Y - S->Y;
   b1 = S->X - E->X;
   c1 = a1 * S->X + b1 * S->Y;

   // get 2d line equation coefficients of splitter
   double a2, b2, c2;
   a2 = -Node->u.internal.separator.a;
   b2 = -Node->u.internal.separator.b;
   c2 = Node->u.internal.separator.c;

   const double det = a1 * b2 - a2 * b1;

   // if zero, they're parallel and never intersect
   if (ISZERO(det))
      return false;

   // intersection point of infinite lines
   P->X = (float)((b2*c1 - b1 * c2) / det);
   P->Y = (float)((a1*c2 - a2 * c1) / det);

   // must be in boundingbox of finite SE
   return ISINBOX(S, E, P);
}
/************************************************************************/
void SetMovementSpeedPct(int speed)
{
   movespeed_pct = speed;
}
/************************************************************************/
__forceinline bool IsRemoteViewMoveAllowed()
{
   if (player.viewID
      && ((player.viewFlags & REMOTE_VIEW_CONTROL)
         || !(player.viewFlags & REMOTE_VIEW_MOVE)))
      return false;
   return true;
}
/************************************************************************/
__forceinline bool IsRunning(int action)
{
   return (action == A_FORWARDFAST || action == A_BACKWARDFAST
      || action == A_SLIDELEFTFAST || action == A_SLIDERIGHTFAST
      || action == A_SLIDELEFTFORWARDFAST || action == A_SLIDERIGHTFORWARDFAST
      || action == A_SLIDELEFTBACKWARDFAST || action == A_SLIDERIGHTBACKWARDFAST);
}
/************************************************************************/
__forceinline float UserMoveGetAngle(int action)
{
   switch (action)
   {
   case A_FORWARD:
   case A_FORWARDFAST:
      return player.angle;
   case A_BACKWARD:
   case A_BACKWARDFAST:
      return (player.angle + NUMDEGREES / 2) % NUMDEGREES;  /* angle + pi */
   case A_SLIDELEFT:
   case A_SLIDELEFTFAST:
      return (player.angle + 3 * NUMDEGREES / 4) % NUMDEGREES;  /* angle + 3pi/2 */
   case A_SLIDERIGHT:
   case A_SLIDERIGHTFAST:
      return (player.angle + NUMDEGREES / 4) % NUMDEGREES;  /* angle + pi/2 */
   case A_SLIDELEFTFORWARD:
   case A_SLIDELEFTFORWARDFAST:
      return (player.angle + 7 * NUMDEGREES / 8) % NUMDEGREES;
   case A_SLIDELEFTBACKWARD:
   case A_SLIDELEFTBACKWARDFAST:
      return (player.angle + 5 * NUMDEGREES / 8) % NUMDEGREES;
   case A_SLIDERIGHTFORWARD:
   case A_SLIDERIGHTFORWARDFAST:
      return (player.angle + 1 * NUMDEGREES / 8) % NUMDEGREES;
   case A_SLIDERIGHTBACKWARD:
   case A_SLIDERIGHTBACKWARDFAST:
      return (player.angle + 3 * NUMDEGREES / 8) % NUMDEGREES;
   default:
      debug(("Bad action type in UserMoveGetAngle\n"));
      return player.angle;
   }
   return player.angle;
}
/************************************************************************/
void UserMovePlayer(int action)
{
   int depthMask[4] = {0,1,2,4};
   int dx, dy, z, move_distance, dt, depth;
   float angle, speed;
   V3 start;
   V2 start2D, step, end;
   DWORD now;
   room_contents_node *player_obj;
   static DWORD last_move_time = 0;
   Bool bounce = True;

   if (effects.paralyzed)
      return;

   player_obj = GetRoomObjectById(player.id);
   if (player_obj == NULL)
   {
      debug(("UserMovePlayer got NULL player object!\n"));
      return;
   }

   if (!GetFrameDrawn())
      return;

   if (!IsRemoteViewMoveAllowed())
      return;

   now = timeGetTime();
   dt = now - last_move_time;
   // Watch for int wraparound in timeGetTime()
   if (dt <= 0)
      dt = 1;
   last_move_time = now;

   // Find out how far to move based on time elapsed:  always move at a rate of 
   // constant rate per MOVE_DELAY milliseconds.
   speed = IsRunning(action) ? USER_RUNNING_SPEED : USER_WALKING_SPEED;
   speed = speed * movespeed_pct / 100;

   // Wading slows player movement down.
   depth = GetPointDepth(player_obj->motion.x, player_obj->motion.y);
   switch (depth)
   {
   case SF_DEPTH1: speed *= 0.75f; break;
   case SF_DEPTH2: speed *= 0.5f; break;
   case SF_DEPTH3: speed *= 0.25f; break;
   }

   move_distance = MOVEUNITS * (speed / 25.0f);
   last_server_speed = speed;

   if (dt < MOVE_DELAY && config.animate)
   {
      gravityAdjust = 1.0f;
      move_distance = move_distance * dt / MOVE_DELAY;
   }
   else
   {
      gravityAdjust = (float)MOVE_DELAY / (float)dt;
   }

   angle = UserMoveGetAngle(action);
   FindOffsets(move_distance, angle, &dx, &dy);

   V2SET(&step, dx, dy);
   V3SET(&start, player_obj->motion.x, player_obj->motion.y, player_obj->motion.z);
   V2SET(&start2D, start.X, start.Y);
   V2SET(&end, start2D.X + step.X, start2D.Y + step.Y);

   // Closest player can get to wall is half his width.  Avoid divide by zero errors.
   min_distance = max(1, player.width / 2);
   min_distance2 = min_distance * min_distance;

   // Check for any object collisions.
   for (list_type l = current_room.contents; l != NULL; l = l->next)
   {
      room_contents_node *r = (room_contents_node *)(l->data);

      if (r->obj.id == player.id)
         continue;

      dx = abs(r->motion.x - (int)end.X);
      dy = abs(r->motion.y - (int)end.Y);

      if (r->obj.moveontype == MOVEON_NO)
      {
         if (dx > MIN_NOMOVEON || dy > MIN_NOMOVEON ||
            (dx * dx + dy * dy) > MIN_NOMOVEON * MIN_NOMOVEON)
            continue;

         // Allowed to move away from object
         int new_distance = dx * dx + dy * dy;

         dx = abs(r->motion.x - (int)start2D.X);
         dy = abs(r->motion.y - (int)start2D.Y);
         int old_distance = dx * dx + dy * dy;
         if (new_distance > old_distance)
            continue;

         // Try to slide along object -- object represented by a square of side MIN_NOMOVEON
         dx = abs(r->motion.x - (int)end.X);
         dy = abs(r->motion.y - (int)end.Y);

         if (dx < MIN_NOMOVEON)
         {
            if (r->motion.x > end.X) end.X = r->motion.x - MIN_NOMOVEON;
            else end.X = r->motion.x + MIN_NOMOVEON;
         }
         else if (dy < MIN_NOMOVEON)
         {
            if (r->motion.y > end.Y) end.Y = r->motion.y - MIN_NOMOVEON;
            else end.Y = r->motion.y + MIN_NOMOVEON;
         }
         // Mirroring Ogre - only handle one collision.
         // TODO: could attempt at least several moves here.
         // The upside over blocking the move outright (old classic move code)
         // is that it's probably harder to 'trap' a player if we allow the move.
         break;
      }
   }

   VerifyMove(&start, &end, &step, speed);

   V2ADD(&end, &start2D, &step);

   // See if trying to move off room.
   if (!IsInRoom(end.X, end.Y, &current_room))
   {
      // Delay between consecutive attempts to move off room
      if (now - move_off_room_time >= MOVE_OFF_ROOM_INTERVAL) // current time 
      {
         // Need to send walking speed for room change, otherwise room
         // changes by players with vigor < 10 will be blocked.
         RequestMove((int)end.Y, (int)end.X, USER_WALKING_SPEED, player.room_id, ANGLE_CTOS(player.angle));
         move_off_room_time = now;
      }
      // Don't actually move player off room
      V2SET(&end, start2D.X, start2D.Y);
   }
   else
   {
      // Reset moving-off-room timer, since this move doesn't go off the room
      move_off_room_time = 0;
   }

   // Set new player position
   player_obj->motion.x = end.X;
   player_obj->motion.y = end.Y;
   //player_obj->motion.z = z;

   // XXX Don't really need to store position in player, since he is also a room object.
   //     The way things are currently done, we now need to update player.
   player.x = end.X;
   player.y = end.Y;

   MoveUpdateServer();

   // Set up vertical motion, if necessary
   z = GetFloorBase(end.X, end.Y);
   if (z == -1)
      z = player_obj->motion.z;

   if (config.animate)
   {
      player_obj->motion.dest_z = z;

      // Only set motion if not already moving that direction
      if (z > player_obj->motion.z && player_obj->motion.v_z <= 0)
      {
         player_obj->motion.v_z = CLIMB_VELOCITY_0;
      }
      else if (z < player_obj->motion.z && player_obj->motion.v_z >= 0)
      {
         player_obj->motion.v_z = FALL_VELOCITY_0;
      }
   }
   else player_obj->motion.z = z;

   if (bounce)
      BounceUser(dt);

   // Play the wading sound if they persist to wade,
   // but sounds get spaced out more if they're deeper.
   z = GetFloorBase(start2D.X, start2D.Y);
   if (depth != SF_DEPTH0 &&
      !(depthMask[depth] & GetRoomFlags()) &&
       config.play_sound && effects.wadingsound &&
       player_obj->motion.z <= z &&
       ((last_splash == 0xFFFFFFFF) || ((now - last_splash) > (DWORD)(500*depth))))
   {
       SoundPlayResource(effects.wadingsound, 0);
       last_splash = now;
   }
   if (depth == SF_DEPTH0)
   {
       last_splash = 0xFFFFFFFF;
   }

   // Update looping sounds to reflect the player's new position
//   debug(("Player now at: (%i,%i)\n",player.x >> LOG_FINENESS,player.y >> LOG_FINENESS));
   SoundSetListenerPosition(player.x, player.y, player.angle);

   RedrawAll();
}
__forceinline float GetSectorHeightFloorWithDepth(const room_type* Room, Sector* Sector, const V2* P)
{
   const float height = GetFloorHeight(P->X, P->Y, Sector);
   const unsigned int depthtype = Sector->flags & SF_MASK_DEPTH;
   const unsigned int depthoverride = Room->flags & ROOM_OVERRIDE_MASK;

   if (depthtype == SF_DEPTH0)
      return (height - sector_depths[SF_DEPTH0]);

   if (depthtype == SF_DEPTH1)
      return (depthoverride != ROOM_OVERRIDE_DEPTH1) ? (height - sector_depths[SF_DEPTH1]) : Room->overrideDepth[SF_DEPTH1];

   if (depthtype == SF_DEPTH2)
      return (depthoverride != ROOM_OVERRIDE_DEPTH2) ? (height - sector_depths[SF_DEPTH2]) : Room->overrideDepth[SF_DEPTH2];

   if (depthtype == SF_DEPTH3)
      return (depthoverride != ROOM_OVERRIDE_DEPTH3) ? (height - sector_depths[SF_DEPTH3]) : Room->overrideDepth[SF_DEPTH3];

   return height;
}
__forceinline bool CanMoveInRoomTree3DInternal(const room_type* Room, Sector* SectorS, Sector* SectorE, Sidedef* SideS, Sidedef* SideE, WallData* Wall, const V2* Q)
{
   // block moves with end outside
   if (!SectorE || !SideE)
      return false;

   // sides which have no passable flag set always block
   if (SideS && !((SideS->flags & WF_PASSABLE) == WF_PASSABLE))
      return false;

   // endsector must not be marked SF_NOMOVE
   if ((SectorE->flags & SF_NOMOVE) == SF_NOMOVE)
      return false;

   // get floor and ceiling height at end
   const float hFloorE = GetSectorHeightFloorWithDepth(Room, SectorE, Q);
   const float hCeilingE = GetCeilingHeight(Q->X, Q->Y, SectorE);

   // check endsector height
   if (hCeilingE - hFloorE < player.height)
      return false;

   // must evaluate heights at transition later
   unsigned int size = intersects.Size;
   if (size < MAXINTERSECTIONS)
   {
      intersects.Data[size].SectorS = SectorS;
      intersects.Data[size].SectorE = SectorE;
      intersects.Data[size].SideS = SideS;
      intersects.Data[size].SideE = SideE;
      intersects.Data[size].Wall = Wall;
      intersects.Data[size].Q.X = Q->X;
      intersects.Data[size].Q.Y = Q->Y;
      intersects.Data[size].FloorHeight = (SectorS) ? GetSectorHeightFloorWithDepth(Room, SectorS, Q) : 0.0f;
      intersects.Ptrs[size] = &intersects.Data[size];
      size++;
      intersects.Size = size;
   }
   else
      debug(("CanMoveInRoomTree3DInternal: Maximum trackable intersections reached!\n"));

   return true;
}
bool CanMoveInRoomTree(const room_type* Room, const BSPnode* Node, const V2* S, const V2* E, WallData** BlockWall)
{
   // reached a leaf or nullchild, movements not blocked by leafs
   if (!Node || Node->type != BSPinternaltype)
   {
      return true;
   }

   /****************************************************************/

   // get signed distances from splitter to both endpoints of move
   const float distS = DISTANCETOSPLITTERSIGNED(Node->u.internal.separator, S);
   const float distE = DISTANCETOSPLITTERSIGNED(Node->u.internal.separator, E);

   /****************************************************************/

   // both endpoints far away enough on positive (right) side
   // --> climb down only right subtree
   if ((distS > (float)min_distance) & (distE > (float)min_distance))
      return CanMoveInRoomTree(Room, Node->u.internal.pos_side, S, E, BlockWall);

   // both endpoints far away enough on negative (left) side
   // --> climb down only left subtree
   else if ((distS < (float)-min_distance) & (distE < (float)-min_distance))
      return CanMoveInRoomTree(Room, Node->u.internal.neg_side, S, E, BlockWall);

   // endpoints are on different sides, or one/both on infinite line or potentially too close
   // --> check walls of splitter first and then possibly climb down both subtrees
   else
   {
      Sidedef* sideS;
      Sector* sectorS;
      Sidedef* sideE;
      Sector* sectorE;

      // CASE 1) The move line actually crosses this infinite splitter.
      // This case handles long movelines where S and E can be far away from each other and
      // just checking the distance of E to the line would fail.
      // q contains the intersection point
      if (((distS > 0.0f) && (distE < 0.0f)) ||
         ((distS < 0.0f) && (distE > 0.0f)))
      {
         // intersect finite move-line SE with infinite splitter line
         // q stores possible intersection point
         V2 q;
         if (IntersectLineSplitter(Node, S, E, &q))
         {
            // iterate finite segments (walls) in this splitter
            WallData* wall = Node->u.internal.walls_in_plane;
            while (wall)
            {
               // OLD: infinite intersection point must also be in bbox of wall
               // otherwise no intersect
               //if (!ISINBOXINT(wall->x0, wall->y0, wall->x1, wall->y1, &q))
               // NEW: Check if the line of the wall intersects a circle consisting
               // of player x, y and radius of min distance allowed to walls. Intersection
               // includes the wall being totally inside the circle.
               V2 P1, P2;
               V2SET(&P1, wall->x0, wall->y0);
               V2SET(&P2, wall->x1, wall->y1);
               if (!IntersectOrInsideLineCircle(&q, (float)min_distance, &P1, &P2))
               {
                  wall = wall->next;
                  continue;
               }

               // set from and to sector / side
               if (distS > 0.0f)
               {
                  sideS = wall->pos_sidedef;
                  sectorS = wall->pos_sector;
               }
               else
               {
                  sideS = wall->neg_sidedef;
                  sectorS = wall->neg_sector;
               }

               if (distE > 0.0f)
               {
                  sideE = wall->pos_sidedef;
                  sectorE = wall->pos_sector;
               }
               else
               {
                  sideE = wall->neg_sidedef;
                  sectorE = wall->neg_sector;
               }

               // check the transition data for this wall, use intersection point q
               bool ok = CanMoveInRoomTree3DInternal(Room, sectorS, sectorE, sideS, sideE, wall, &q);

               if (!ok)
               {
                  *BlockWall = wall;
                  return false;
               }

               wall = wall->next;
            }
         }
      }

      // CASE 2) The move line does not cross the infinite splitter, both move endpoints are on the same side.
      // This handles short moves where walls are not intersected, but the endpoint may be too close
      else
      {
         // check only getting closer
         if (fabsf(distE) <= fabsf(distS))
         {
            // iterate finite segments (walls) in this splitter
            WallData* wall = Node->u.internal.walls_in_plane;
            while (wall)
            {
               int useCase;

               // get min. squared distance from move endpoint to line segment
               V2 P1, P2;
               V2SET(&P1, wall->x0, wall->y0);
               V2SET(&P2, wall->x1, wall->y1);
               const float dist2 = MinSquaredDistanceToLineSegment(E, &P1, &P2, &useCase);

               // skip if far enough away
               if (dist2 > (float)min_distance2)
               {
                  wall = wall->next;
                  continue;
               }

               // q stores closest point on line
               V2 q;
               if (useCase == 1)
               {
                  // p0 is closest
                  V2SET(&q, wall->x0, wall->y0);
               }
               else if (useCase == 2)
               {
                  // p1 is closest
                  V2SET(&q, wall->x1, wall->y1);
               }
               else
               {
                  // line normal (90° vertical to line)
                  V2 normal;
                  V2SET(&normal, Node->u.internal.separator.a, Node->u.internal.separator.b);

                  // flip normal if necessary (pick correct one of two)
                  if (distE > 0.0f)
                  {
                     V2SCALE(&normal, -1.0f);
                  }

                  V2SCALE(&normal, fabsf(distE)); // set length of normal to distance to line
                  V2ADD(&q, E, &normal);         // q=E moved along the normal onto the line
               }

               // set from and to sector / side
               // for case 2 (too close) these are based on (S),
               // and (E) is assumed to be on the other side.
               if (distS >= 0.0f)
               {
                  sideS = wall->pos_sidedef;
                  sectorS = wall->pos_sector;
                  sideE = wall->neg_sidedef;
                  sectorE = wall->neg_sector;
               }
               else
               {
                  sideS = wall->neg_sidedef;
                  sectorS = wall->neg_sector;
                  sideE = wall->pos_sidedef;
                  sectorE = wall->pos_sector;
               }

               // check the transition data for this wall, use E for intersectpoint
               bool ok = CanMoveInRoomTree3DInternal(Room, sectorS, sectorE, sideS, sideE, wall, &q);

               if (!ok)
               {
                  *BlockWall = wall;
                  return false;
               }

               wall = wall->next;
            }
         }
      }

      /****************************************************************/

      // try right subtree first
      bool retval = CanMoveInRoomTree(Room, Node->u.internal.pos_side, S, E, BlockWall);

      // found a collision there? return it
      if (!retval)
         return retval;

      // otherwise try left subtree
      return CanMoveInRoomTree(Room, Node->u.internal.neg_side, S, E, BlockWall);
   }
}

static bool CanMoveInRoom(V2 *S, V2 *E, float Height, float Speed, WallData **BlockWall)
{
   // clear old intersections
   intersects.Size = 0;

   if (!CanMoveInRoomTree(&current_room, current_room.nodes, S, E, BlockWall))
      return false;

   // but still got to validate height transitions list
   // calculate the squared distances
   for (unsigned int i = 0; i < intersects.Size; ++i)
   {
      V2 v;
      V2SUB(&v, &intersects.Data[i].Q, S);
      intersects.Data[i].Distance2 = V2LEN2(&v);
   }

   // sort the potential intersections by squared distance from start
   IntersectionsSort(intersects.Ptrs, 0, intersects.Size);

   // iterate from intersection to intersection, starting and start and ending at end
   // check the transition data for each one, use intersection point q
   float distanceDone = 0.0f;
   float heightModified = Height;
   V2* p = S;
   for (unsigned int i = 0; i < intersects.Size; i++)
   {
      Intersection* transit = intersects.Ptrs[i];

      // deal with null start sector/side
      if (!transit->SideS || !transit->SectorS)
      {
         if (i == 0) break; // allow if first segment (start outside)
         else
         {
            // deny otherwise (segment on the line outside)
            *BlockWall = transit->Wall;
            return false;
         }
      }

      // get floor heights
      float hFloorSP = GetSectorHeightFloorWithDepth(&current_room, transit->SectorS, p);
      float hFloorSQ = GetSectorHeightFloorWithDepth(&current_room, transit->SectorS, &transit->Q);
      float hFloorEQ = GetSectorHeightFloorWithDepth(&current_room, transit->SectorE, &transit->Q);

      // squared length of this segment
      float stepLen2 = transit->Distance2 - distanceDone;

      // reduce height by what we lose across the SectorS from p to q
      if (heightModified > hFloorSP)
      {
         // workaround div by 0, todo? these are teleports
         if (Speed < 0.001f && Speed > -0.001f) Speed = 9999999;

         // S = 1/2*a*t² where t=dist/speed
         // S = 1/2*a*(dist²/speed²)
         // t² = (dist/speed)² = dist²/speed²
         float stepDt2 = stepLen2 / (Speed*Speed);
         float stepFall = 0.5f * GRAVITYACCELERATION * stepDt2;

         // apply fallheight
         heightModified += stepFall;
      }

      // too far below sector
      else if (heightModified < (hFloorSP - MAXSTEPHEIGHT))
      {
         *BlockWall = transit->Wall;
         return false;
      }

      // make sure we're at least at startsector's groundheight at Q when we reach Q from P
      // in case we stepped up or fell below it
      heightModified = max(hFloorSQ, heightModified);

      // check stepheight (this also requires a lower texture set)
      //if (transit->SideS->TextureLower > 0 && (hFloorE - hFloorQ > MAXSTEPHEIGHT))
      if (transit->SideS->below_bmap != NULL && (hFloorEQ - heightModified > MAXSTEPHEIGHT))
      {
         *BlockWall = transit->Wall;
         return false;
      }

      // get ceiling height
      const float hCeilingEQ = GetCeilingHeight(transit->Q.X, transit->Q.Y, transit->SectorE);

      // check ceilingheight (this also requires an upper texture set)
      if (transit->SideS->above_bmap != NULL && (hCeilingEQ - hFloorSQ < player.height))
      {
         *BlockWall = transit->Wall;
         return false;
      }

      // we actually made it across that intersection
      heightModified = max(hFloorEQ, heightModified); // keep our height or set it at least to sector
      distanceDone += stepLen2;                       // add squared length of processed segment
      p = &transit->Q;                                // set end to next start
   }

   return true;
}

static void VerifyMove(V3* Start, V2* End, V2* Move, float Speed)
{
   V2 Start2D, rot;
   lastBlockingWall = NULL;
   const int MAXATTEMPTS = 8;
   const float ANGLESTEP = QUARTERPERIOD / 8;

   V2SET(&Start2D, Start->X, Start->Y);
   V2SET(Move, End->X, End->Y);

   /**************************************************************/

   // no collision with untouched move
   if (CanMoveInRoom(&Start2D, End, Start->Z, Speed, &lastBlockingWall))
   {
      V2SUB(Move, End, &Start2D);
      return;
   }

   /**************************************************************/

   // try to slide along collision wall
   SlideAlongWall(lastBlockingWall, &Start2D, Move);

   // no collision with 'slide along' move
   if (CanMoveInRoom(&Start2D, Move, Start->Z, Speed, &lastBlockingWall))
   {
      V2SUB(Move, Move, &Start2D);
      return;
   }

   /**************************************************************/

   // sliding on collision walls does not work, try rotate a bit
   for (int i = 0; i < MAXATTEMPTS; i++)
   {
      V2SUB(&rot, End, &Start2D);
      V2ROTATE(&rot, -ANGLESTEP * i);
      V2ROUND(&rot);
      V2ADD(Move, &Start2D, &rot);

      // no collision
      if (CanMoveInRoom(&Start2D, Move, Start->Z, Speed, &lastBlockingWall))
      {
         V2SUB(Move, Move, &Start2D);
         return;
      }

      V2SUB(&rot, End, &Start2D);
      V2ROTATE(&rot, ANGLESTEP * i);
      V2ROUND(&rot);
      V2ADD(Move, &Start2D, &rot);

      // no collision
      if (CanMoveInRoom(&Start2D, Move, Start->Z, Speed, &lastBlockingWall))
      {
         V2SUB(Move, Move, &Start2D);
         return;
      }
   }

   // Couldn't move, set to 0.
   V2SET(Move, 0.0f, 0.0f);
}

bool IsInRoom(int x, int y, room_type *room)
{
   if (!room)
      return False;

   if (x <= room->ThingsBox.Min.X || x >= room->ThingsBox.Max.X
      || y <= room->ThingsBox.Min.Y || y >= room->ThingsBox.Max.Y)
      return False;

   return True;
}

// Try to "slide" along in direction of wall
static void SlideAlongWall(WallData *wall, V2 *Start, V2 *End)
{
   V2 delta, wall_delta;
   V2SUB(&delta, End, Start);
   V2SET(&wall_delta, wall->x1 - wall->x0, wall->y1 - wall->y0);
   float num = V2DOT(&delta, &wall_delta);
   float denom = V2LEN2(&wall_delta);

   if (denom > 0.0f)
   {
      V2SCALE(&wall_delta, num / denom);
      V2ROUND(&wall_delta);
      V2ADD(End, Start, &wall_delta);
   }
}

/************************************************************************/
/*
 * ServerMovedPlayer:  Called whenever the player is relocated by the server.
 *   Stop restricting user motion for teleporter nomoveons.
 */
void ServerMovedPlayer(void)
{
   room_contents_node *player_obj;

   // Put player on floor
   player_obj = GetRoomObjectById(player.id);
   if (!player_obj)
      return;

   RoomObjectSetHeight(player_obj);
   V2SET(&last_server_pos, player_obj->motion.x, player_obj->motion.y);
   //player_obj->motion.z = GetFloorBase(server_x,server_y);

   // Update looping sounds to reflect the player's new position
//   debug(("Player now at: (%i,%i)\n",player.x >> LOG_FINENESS,player.y >> LOG_FINENESS));
   SoundSetListenerPosition(player.x, player.y, player.angle);
}
/************************************************************************/
/*
 * MoveUpdateServer:  Update the server's knowledge of our position and angle, if necessary.
 */ 
void MoveUpdateServer(void)
{
   DWORD now = timeGetTime();
   DWORD latency_check = latency * 1.1f;
   int angle;

   if (latency_check < 100)
      latency_check = 100;
   if (latency_check > 500)
      latency_check = 500;

   // Inform server if necessary
   if (now - server_time < MOVE_INTERVAL
      || !pos_valid
      || last_go_time + latency_check > now)
      return;

   // if position wasn't updated
   // see if angle should be updated independently
   if (!MoveUpdatePosition())
   {
      angle = ANGLE_CTOS(player.angle);
      if (last_server_angle != angle)
      {
         RequestTurn(player.id, angle);
         last_server_angle = angle;
         server_time = now;
      }
   }
}
/************************************************************************/
/*
 * MoveUpdatePosition:  Update the server's knowledge of our position.
 *   This procedure does not care about elapsed interval, but
 *   sends the update only when the position has changed at least a bit.
 */ 
Bool MoveUpdatePosition(void)
{
   int x, y;

   x = player.x;
   y = player.y;

   // don't send update if we didn't move
   if ((last_server_pos.X - x) * (last_server_pos.X - x)
      + (last_server_pos.Y - y) * (last_server_pos.Y - y) > MOVE_THRESHOLD)
   {
      // debug output
      debug(("MoveUpdatePosition: x (%.2f -> %d), y (%.2f -> %d), speed (%.2f)\n",
         last_server_pos.X, x, last_server_pos.Y, y, last_server_speed));

      // send update
      RequestMove(y, x, (int)last_server_speed, player.room_id, ANGLE_CTOS(player.angle));
   
      // save last sent position and tick
      V2SET(&last_server_pos, x, y);
      server_time = timeGetTime();

      return true;
   }

   return false;
}
/************************************************************************/
/*
 * MoveSetValidity:  valid tells whether our knowledge of the player's position
 *   is correct.  This should be False during initialization.
 */ 
void MoveSetValidity(Bool valid)
{
   room_contents_node *player_obj;

   pos_valid = valid;

   if (pos_valid)
   {
      server_time = timeGetTime();

      player_obj = GetRoomObjectById(player.id);

      if (player_obj == NULL)
      {
         debug(("MoveSetValidity got NULL player object\n"));
         return;
      }

      player.x = player_obj->motion.x;
      player.y = player_obj->motion.y;
      V2SET(&last_server_pos, player.x, player.y);
   }
}
/************************************************************************/
void UserTurnPlayer(int action)
{
   int dt, delta;  /* # of degrees to turn */
   DWORD now;
   static DWORD last_turn_time = 0;

   if (effects.paralyzed)
      return;

   switch(action)
   {
   case A_TURNLEFT:
      delta = - TURNDEGREES;
      break;
   case A_TURNRIGHT:
      delta = TURNDEGREES;
      break;
   case A_TURNFASTLEFT:
      delta = - 3 * TURNDEGREES;
      break;
   case A_TURNFASTRIGHT:
      delta = 3 * TURNDEGREES;
      break;
   default:
      debug(("Bad action type in UserTurnPlayer\n"));
      return;
   }

   // Find out how far to turn based on time elapsed:  always turn at a rate of
   // TURNDEGREES per TURN_DELAY milliseconds.
   now = timeGetTime();
   dt = now - last_turn_time;
   if (last_turn_time == 0 || dt <= 0)
      dt = 1;
   if (dt < TURN_DELAY && config.animate)
   {
      delta = delta * dt / TURN_DELAY;
   }
   else if (dt > (4*TURN_DELAY) && config.animate)
   {
      delta = (delta * (int)(GetFrameTime()) / TURN_DELAY) / 2;
   }
   last_turn_time = now;

   if (player.viewID)
   {
      if (!(player.viewFlags & REMOTE_VIEW_TURN))
         return;
      if (player.viewFlags & REMOTE_VIEW_CONTROL)
      {
         room_contents_node *viewObject = GetRoomObjectById(player.viewID);
         if (viewObject)
         {
            viewObject->angle += delta;
            if (viewObject->angle < 0)
               viewObject->angle += NUMDEGREES;
            viewObject->angle = viewObject->angle % NUMDEGREES;
            RedrawAll();
            return;
         }
      }
   }

   player.angle += delta;
   if (player.angle < 0)
      player.angle += NUMDEGREES;
   player.angle = player.angle % NUMDEGREES;

   // Inform server of turn if necessary
   MoveUpdateServer();

   // update sound listener orientation
   SoundSetListenerPosition(player.x, player.y, player.angle);

   RedrawAll();
}

/************************************************************************/
void UserTurnPlayerMouse(int delta)
{
   if (effects.paralyzed)
      return;

   if (player.viewID)
   {
      if (!(player.viewFlags & REMOTE_VIEW_TURN))
         return;
      if (player.viewFlags & REMOTE_VIEW_CONTROL)
      {
         room_contents_node *viewObject = GetRoomObjectById(player.viewID);
         if (viewObject)
         {
            viewObject->angle += delta;
            if (viewObject->angle < 0)
               viewObject->angle += NUMDEGREES;
            viewObject->angle = viewObject->angle % NUMDEGREES;
            RedrawAll();
            return;
         }
      }
   }

   player.angle += delta;
   if (player.angle < 0)
      player.angle += NUMDEGREES;
   player.angle = player.angle % NUMDEGREES;

   // Inform server of turn if necessary
   MoveUpdateServer();

   RedrawAll();
}
/************************************************************************/
void UserFlipPlayer(void)
{
   if (effects.paralyzed)
      return;

   if (player.viewID)
   {
      if (!(player.viewFlags & REMOTE_VIEW_TURN))
         return;
      if (player.viewFlags & REMOTE_VIEW_CONTROL)
      {
         room_contents_node *viewObject = GetRoomObjectById(player.viewID);
         if (viewObject)
         {
            viewObject->angle += NUMDEGREES / 2;
            viewObject->angle = viewObject->angle % NUMDEGREES;
            RedrawAll();
            return;
         }
      }
   }

   // Turn 180 degrees around
   player.angle += NUMDEGREES / 2;
   player.angle = player.angle % NUMDEGREES;
   MoveUpdateServer();
   RedrawAll();
}

// Move at most HEIGHT_INCREMENT per HEIGHT_DELAY milliseconds
#define HEIGHT_INCREMENT (MAXY / 4)    
#define HEIGHT_DELAY     100
#define HEIGHT_MAX_OFFSET (3 * MAXY / 2)    // Farthest you can look up or down
/************************************************************************/
/* 
 * BounceUser:  Modify user's height a little to give appearance that 
 *   he's bouncing a little as he walks.
 *   dt is # of milliseconds since last user motion
 */
void BounceUser(int dt)
{
   static float bounce_time = 0.0;

   if (config.bounce)
   {
     dt = min(dt, MOVE_DELAY);
     bounce_time += ((float) dt) / MOVE_DELAY;  /* In radians */
     bounce_height = (long) (BOUNCE_HEIGHT * sin(bounce_time));
   }
   else
      bounce_height = 0;
}
/************************************************************************/
/*
 * PlayerChangeHeight:  If dz > 0, move player's eye level up.  If dz < 0,
 *   move eye level down.
 *   Also make sure that height stays within reasonable range.
 *   This is used for allowing the player to look up and down.
 */
void PlayerChangeHeight(int dz)
{
   DWORD now;
   int dt;
   static DWORD last_time = 0;

   dz = SGN(dz) * HEIGHT_INCREMENT;

   now = timeGetTime();
   dt = now - last_time;
   if (last_time == 0 || dt <= 0)
      dt = 1;
   if (dt < HEIGHT_DELAY && config.animate)
   {
      dz = dz * dt / HEIGHT_DELAY;
   }
   else if (dt > (4*HEIGHT_DELAY) && config.animate)
   {
      dz = (dz * (int)(GetFrameTime()) / HEIGHT_DELAY) / 2;
   }
   last_time = now;

   height_offset += dz;
   height_offset = min(height_offset, HEIGHT_MAX_OFFSET);
   height_offset = max(height_offset, - HEIGHT_MAX_OFFSET);
   RedrawAll();
}

void PlayerChangeHeightMouse(int dz)
{
   height_offset += dz;
   height_offset = min(height_offset, HEIGHT_MAX_OFFSET);
   height_offset = max(height_offset, - HEIGHT_MAX_OFFSET);
}
/************************************************************************/
/*
 * PlayerResetHeight:  Set player's eye height so that he's looking straight ahead.
 */
void PlayerResetHeight(void)
{
   height_offset = 0;
   RedrawAll();
}
/************************************************************************/
/*
 * PlayerGetHeight:  Return height of player's eye above the floor, in FINENESS units.
 */
int PlayerGetHeight(void)
{
   // Save last player height for when data becomes invalid (e.g. GC)
   // but we still want to redraw.
   static int height;
   room_contents_node *r;

   if (player.viewID)
   {
      return player.viewHeight;
   }
   r = GetRoomObjectById(player.id);
   if (r != NULL)
      height = r->motion.z + player.height + bounce_height;

   return height;
}
/************************************************************************/
/*
 * PlayerGetHeightOffset: 
 */
int PlayerGetHeightOffset(void)
{
   return height_offset;
}
