// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * bspload.c:  Load a BSP room file.
 *
 * A room file contains a BSP tree which is reconstructed as we load it in.  In the file,
 * references to other nodes, walls and sectors are in the form of an indices.  We convert these
 * directly into pointers in RoomSwizzle.
 *
 * The file also contains a security checksum that we use as a unique ID of the room, and which 
 * also prevents tampering.  We recalculate the checksum as the room is loaded, and compare
 * the result with the checksum in the file.  In addition, the server sends us the checksum of
 * the current room; we compare the two elsewhere to make sure that the client uses the correct
 * room file (in case someone tries to copy one room file over another).
 */

#include "client.h"
#include "d3dcache.h"

static const int ROO_VERSION = 14;

static BYTE room_magic[] = { 0x52, 0x4F, 0x4F, 0xB1 };

static int room_version;     // Version of room file we're reading

static int security;         // Room security value, calculated as room is loaded

static Bool LoadNodes(file_node *f, room_type *room, int num_nodes);
static Bool LoadWalls(file_node *f, room_type *room, int num_walls);
static Bool LoadSectors(file_node *f, room_type *room, int num_sectors);
static Bool LoadThings(file_node *f, room_type *room, int num_things);
static Bool LoadSidedefs(file_node *f, room_type *room, int num_sidedefs);
static Bool RoomSwizzle(room_type *room, BSPTree tree, int num_nodes, int num_walls, int num_sidedefs, int num_sectors);
void PrecalcLeafData(BSPleaf *leaf);
void PrecalcInternalNodeData(BSPinternal *iNode);
/*****************************************************************************************/
/*
 * BSPRooFileLoad:  Load room description from given file, and put result in room.
 *   Returns True on success.
 */
extern Bool                  gD3DRedrawAll;
extern d3d_render_cache_system   gWorldCacheSystemStatic;
extern d3d_render_cache_system   gLMapCacheSystemStatic;
extern custom_xyz            playerOldPos;

Bool BSPRooFileLoad(char *fname, room_type *room)
{
   int i, temp;
   BYTE byte;
   WORD num_nodes, num_walls, num_sectors, num_sidedefs, num_things;
   int node_pos, wall_pos, sidedef_pos, sector_pos, thing_pos, offset_adjust;
   file_node f;

   if (!MappedFileOpenCopy(fname, &f))
      return False;

   // Check magic number and version
   for (i = 0; i < 4; i++)
      if (CliMappedFileRead(&f, &byte, 1) != 1 || byte != room_magic[i])
      { MappedFileClose(&f); debug(("%s is not a roo file\n", fname)); return False; }

   security = 0;

   if (CliMappedFileRead(&f, &room_version, 4) != 4 || room_version < ROO_VERSION)
   { 
      MappedFileClose(&f); 
      debug(("Bad roo version %d; expecting %d\n", room_version, ROO_VERSION)); 
      return False; 
   }
   
   security += room_version;

   if (CliMappedFileRead(&f, &room->security, 4) != 4)
   { MappedFileClose(&f); return False; }   

   // Read pointer to main info in file, and go there
   if (CliMappedFileRead(&f, &temp, 4) != 4)
   { MappedFileClose(&f); return False; }
   MappedFileGoto(&f, temp);

   // Read size of room
   if (CliMappedFileRead(&f, &room->width, 4) != 4)
   { MappedFileClose(&f); return False; }

   offset_adjust = 0;
   room->cols = room->width >> LOG_FINENESS;

   if (CliMappedFileRead(&f, &room->height, 4) != 4)
   { MappedFileClose(&f); return False; }
   room->rows = room->height >> LOG_FINENESS;

   // Read pointers to file sections
   if (CliMappedFileRead(&f, &node_pos, 4) != 4) { MappedFileClose(&f); return False; }
   if (CliMappedFileRead(&f, &wall_pos, 4) != 4) { MappedFileClose(&f); return False; }
   if (CliMappedFileRead(&f, &temp, 4) != 4) { MappedFileClose(&f); return False; }
   if (CliMappedFileRead(&f, &sidedef_pos, 4) != 4) { MappedFileClose(&f); return False; }
   if (CliMappedFileRead(&f, &sector_pos, 4) != 4) { MappedFileClose(&f); return False; }
   if (CliMappedFileRead(&f, &thing_pos, 4) != 4) { MappedFileClose(&f); return False; }
   node_pos += offset_adjust;
   wall_pos += offset_adjust;
   sidedef_pos += offset_adjust;
   sector_pos += offset_adjust;
   thing_pos += offset_adjust;

   // Read nodes
   MappedFileGoto(&f, node_pos);
   if (CliMappedFileRead(&f, &num_nodes, 2) != 2) { MappedFileClose(&f); return False; }
   if (LoadNodes(&f, room, num_nodes) == False) 
   { 
      debug(("Failure loading %d nodes\n", num_nodes));
      MappedFileClose(&f); 
      return False; 
   }

   // Read walls
   MappedFileGoto(&f, wall_pos);
   if (CliMappedFileRead(&f, &num_walls, 2) != 2) { MappedFileClose(&f); return False; }
   if (LoadWalls(&f, room, num_walls) == False) 
   { 
      debug(("Failure loading %d walls\n", num_walls));
      MappedFileClose(&f); 
      return False; 
   }

   // Read sidedefs
   MappedFileGoto(&f, sidedef_pos);
   if (CliMappedFileRead(&f, &num_sidedefs, 2) != 2) { MappedFileClose(&f); return False; }
   if (LoadSidedefs(&f, room, num_sidedefs) == False) 
   { 
      debug(("Failure loading %d sidedefs\n", num_sidedefs));
      MappedFileClose(&f); 
      return False; 
   }

   // Read sectors
   MappedFileGoto(&f, sector_pos);
   if (CliMappedFileRead(&f, &num_sectors, 2) != 2) { MappedFileClose(&f); return False; }
   if (LoadSectors(&f, room, num_sectors) == False)
   { 
      debug(("Failure loading %d sectors\n", num_sectors));
      MappedFileClose(&f); 
      return False; 
   }

   // Read things (room bbox)
   MappedFileGoto(&f, thing_pos);
   if (CliMappedFileRead(&f, &num_things, 2) != 2) { MappedFileClose(&f); return False; }
   if (num_things != 2)
   {
      debug(("Num things != 2! Got %d things\n", num_things));
      MappedFileClose(&f);
      return False;
   }

   if (LoadThings(&f, room, num_things) == False)
   {
      debug(("Failure loading %d things\n", num_things));
      MappedFileClose(&f);
      return False;
   }

   MappedFileClose(&f);

   if (num_nodes == 0)
   {
      debug(("LoadRoom found room with no nodes!\n"));
      room->tree = NULL;
   }
   else room->tree = &room->nodes[0];

   if (RoomSwizzle(room, room->tree, num_nodes, num_walls, num_sidedefs, num_sectors) ==  False)
   {
      debug(("RoomSwizzle failed\n")); 
      BSPRoomFree(room);
      return False; 
   }

   security ^= 0x89ab786c;
   if (config.security && security != room->security)
   {
      debug(("Room security mismatch (got %d, expecting %d)!\n", security, room->security));
      BSPRoomFree(room);
      return False;
   }

   room->num_nodes    = num_nodes;
   room->num_walls    = num_walls;
   room->num_sectors  = num_sectors;
   room->num_sidedefs = num_sidedefs;

   for (int i = 0; i < num_nodes; ++i)
   {
      if (room->nodes[i].type == BSPleaftype)
         PrecalcLeafData(&room->nodes[i].u.leaf);
      else if (room->nodes[i].type == BSPinternaltype)
         PrecalcInternalNodeData(&room->nodes[i].u.internal);
   }

//   BSPDumpTree(room->tree, 0);
//   D3DGeometryBuild(room);
   gD3DRedrawAll |= D3DRENDER_REDRAW_ALL;
   D3DParticlesInit(false);

   playerOldPos.x = 0;
   playerOldPos.y = 0;
   playerOldPos.z = 0;

   CacheReport();

   return True;
}
/*****************************************************************************************/
/*
 * BSPRoomFree:  Free memory allocated for a room, and set room's BSP tree to NULL.
 */
void BSPRoomFree(room_type *room)
{
   int i;

   if (room->tree == NULL || room->nodes == NULL)
   {
      debug(("BSPRoomFree tried to free an empty room\n"));
      return;
   }

   // Free animation structures
   for (i=0; i < room->num_sidedefs; i++)
      if (room->sidedefs[i].animate != NULL)
    SafeFree(room->sidedefs[i].animate);

   // free animation and slope structures for each sector
   for (i=0; i < room->num_sectors; i++) {
      if (room->sectors[i].animate != NULL) 
    SafeFree(room->sectors[i].animate);
      
      if (room->sectors[i].sloped_floor != NULL) 
    SafeFree(room->sectors[i].sloped_floor);
      if (room->sectors[i].sloped_ceiling != NULL) 
    SafeFree(room->sectors[i].sloped_ceiling);
   }

   SafeFree(room->nodes);
   room->nodes = NULL;
   if (room->walls != NULL)
   {
      SafeFree(room->walls);
      room->walls = NULL;
   }

   if (room->sidedefs != NULL)
   {
      SafeFree(room->sidedefs);
      room->sidedefs = NULL;
   }
   if (room->sectors != NULL)
   {
      SafeFree(room->sectors);
      room->sectors = NULL;
   }
   room->tree = NULL;
   room->num_nodes = room->num_walls = room->num_sectors = room->num_sidedefs = 0;
}
/*****************************************************************************************/
/*
 * LoadNodes:  Load nodes of BSP tree, and set tree field of room structure to the root.
 *   Return True on success.
 */
Bool LoadNodes(file_node *f, room_type *room, int num_nodes)
{
   int i, j, size;
   BYTE type;
   WORD num_points;

   size = num_nodes * sizeof(BSPnode);
   room->nodes = (BSPnode *) SafeMalloc(size);
   memset(room->nodes, 0, size);

   for (i=0; i < num_nodes; i++)
   {
      BSPnode *node;
      BSPleaf *leaf;
      BSPinternal *inode;

      node = &room->nodes[i];

      if (CliMappedFileRead(f, &type, 1) != 1) return False;
      node->type = (BSPnodetype) type;

      // These are now loaded as floats.
      if (CliMappedFileRead(f, &node->bbox.x0, 4) != 4) return False;
      if (CliMappedFileRead(f, &node->bbox.y0, 4) != 4) return False;
      if (CliMappedFileRead(f, &node->bbox.x1, 4) != 4) return False;
      if (CliMappedFileRead(f, &node->bbox.y1, 4) != 4) return False;

//      debug(("Loading node %d ", i));

      switch (type)
      {
      case BSPinternaltype:
    inode = &node->u.internal;
    
    // Loaded as floats.
    if (CliMappedFileRead(f, &inode->separator.a, 4) != 4) return False;
    if (CliMappedFileRead(f, &inode->separator.b, 4) != 4) return False;
    if (CliMappedFileRead(f, &inode->separator.c, 4) != 4) return False;

    // Don't add these to room security (as of roo version 14).
    //security += inode->separator.a + inode->separator.b + inode->separator.c;

    if (CliMappedFileRead(f, &inode->pos_num, 2) != 2) return False;
    if (CliMappedFileRead(f, &inode->neg_num, 2) != 2) return False;
    if (CliMappedFileRead(f, &inode->wall_num, 2) != 2) return False;
    security += inode->wall_num;

    break;

      case BSPleaftype:
    leaf = &node->u.leaf;
    
    if (CliMappedFileRead(f, &leaf->sector_num, 2) != 2) return False;

    // Get points of polygon
    if (CliMappedFileRead(f, &num_points, 2) != 2) return False;
    leaf->poly.npts = num_points;
    for (j=0; j < num_points; j++)
    {
       // Loaded as floats.
      if (CliMappedFileRead(f, &leaf->poly.p[j].x, 4) != 4) return False;
      if (CliMappedFileRead(f, &leaf->poly.p[j].y, 4) != 4) return False;
      // Don't add these to room security (as of roo version 14).
      //security += leaf->poly.p[j].x + leaf->poly.p[j].y;
    }
    leaf->poly.p[num_points] = leaf->poly.p[0];    
    break;

      default:
    debug(("LoadNodes got unknown node type %d!\n", (int) type));
    return False;
      }      
   }
   return True;
}
/*****************************************************************************************/
/*
 * LoadWalls:  Load walls, and set global walls variable to array of walls.
 *   Return True on success.
 */
Bool LoadWalls(file_node *f, room_type *room, int num_walls)
{
   int i, size;
   WORD word;

   size = num_walls * sizeof(WallData);
   room->walls = (WallData *) SafeMalloc(size);
   memset(room->walls, 0, size);

   for (i=0; i < num_walls; i++)
   {
      WallData *wall = &room->walls[i];
      
//      debug(("Loading wall %d of %d\n", i, num_walls));

      if (CliMappedFileRead(f, &word, 2) != 2) return False;
      wall->next_num = word;

      // Get sidedef numbers
      if (CliMappedFileRead(f, &wall->pos_sidedef_num, 2) != 2) return False;
      if (CliMappedFileRead(f, &wall->neg_sidedef_num, 2) != 2) return False;
      security += wall->pos_sidedef_num + wall->neg_sidedef_num;

      // Loaded as floats.
      if (CliMappedFileRead(f, &wall->x0, 4) != 4) return False;
      if (CliMappedFileRead(f, &wall->y0, 4) != 4) return False;
      if (CliMappedFileRead(f, &wall->x1, 4) != 4) return False;
      if (CliMappedFileRead(f, &wall->y1, 4) != 4) return False;
      // Don't add these to room security (as of roo version 14).
      //security += wall->x0 + wall->y0 + wall->x1 + wall->y1;
      
      // Get wall length, loaded as float.
      if (CliMappedFileRead(f, &wall->length, 4) != 4) return False;
      
      // Get texture offsets
      if (CliMappedFileRead(f, &wall->pos_xoffset, 2) != 2) return False;
      if (CliMappedFileRead(f, &wall->neg_xoffset, 2) != 2) return False;
      if (CliMappedFileRead(f, &wall->pos_yoffset, 2) != 2) return False;
      if (CliMappedFileRead(f, &wall->neg_yoffset, 2) != 2) return False;

      // Get sector numbers
      if (CliMappedFileRead(f, &wall->pos_sector_num, 2) != 2) return False;
      if (CliMappedFileRead(f, &wall->neg_sector_num, 2) != 2) return False;
      security += wall->pos_sector_num + wall->neg_sector_num;
   }
   return True;
}
/*****************************************************************************************/
/*
 * LoadSidedefs:  Load sidedefs, and set global sidedefs variable to array of sidedefs.
 *   Return True on success.
 */
Bool LoadSidedefs(file_node *f, room_type *room, int num_sidedefs)
{
   int i, size, period;
   BYTE speed;

   size = num_sidedefs * sizeof(Sidedef);
   room->sidedefs = (Sidedef *) SafeMalloc(size);
   memset(room->sidedefs, 0, size);

   for (i=0; i < num_sidedefs; i++)
   {
      Sidedef *s = &room->sidedefs[i];
      
      if (CliMappedFileRead(f, &s->server_id, 2) != 2) return False;
      security += s->server_id;
      
      // Get wall bitmaps
      if (CliMappedFileRead(f, &s->normal_type, 2) != 2) return False;
      s->normal_bmap = GetGridPdib(s->normal_type);
      if (CliMappedFileRead(f, &s->above_type, 2) != 2) return False;
      s->above_bmap  = GetGridPdib(s->above_type);
      if (CliMappedFileRead(f, &s->below_type, 2) != 2) return False;
      s->below_bmap  = GetGridPdib(s->below_type);
      security += s->above_type + s->below_type + s->normal_type;

      if (CliMappedFileRead(f, &s->flags, 4) != 4) return False;
      security += s->flags;
      if (CliMappedFileRead(f, &speed, 1) != 1) return False;

      // Set up animation, if any
      if (speed != 0)
      {
         s->animate = (RoomAnimate *) SafeMalloc(sizeof(RoomAnimate));
         RoomSetupCycleAnimation(s->animate, speed);
      }
      else if (WallScrollSpeed(s->flags) != SCROLL_NONE)
      {
         // Slow down wall scrolling speed; this is because we notice subjectively
         // that walls are scrolling too fast.
         speed = WallScrollSpeed(s->flags);
         switch (speed)
         {
         case SCROLL_MEDIUM: period = SCROLL_WALL_MEDIUM_PERIOD;   break;
         case SCROLL_FAST:   period = SCROLL_WALL_FAST_PERIOD;     break;
         default: period = SCROLL_WALL_SLOW_PERIOD; // case SCROLL_SLOW
         }
         s->animate = (RoomAnimate *) SafeMalloc(sizeof(RoomAnimate));
         RoomSetupScrollAnimation(s->animate, period, WallScrollDirection(s->flags));
      }
      else s->animate = NULL;
   }
   return True;
}

// These vector math functions are just a little bit of pork to make
//  setting up the slope info records easier.
   
/* returns squared length of input vector */   
FixedPoint V3SquaredLength(Vector3D *a) {
    FixedPoint s2;

    s2 = fpSquare(a->x) + fpSquare(a->y) + fpSquare(a->z);
    
    return(s2);
}

/* returns length of input vector */
FixedPoint V3Length(Vector3D *a) 
{
   FixedPoint s2 = V3SquaredLength(a);
   FixedPoint s = fpSqrt(s2);
   return(s);
}

/* return vector sum c = a+b */
Vector3D *V3Add(Vector3D *a, Vector3D *b, Vector3D *c) {
    c->x = a->x + b->x;
    c->y = a->y + b->y;
    c->z = a->z + b->z;

    return(c);
}

/* return the cross product c = a cross b */
Vector3D *V3Cross(Vector3D *a, Vector3D *b, Vector3D *c) {
    c->x = fpMul(a->y, b->z) - fpMul(a->z, b->y);
    c->y = fpMul(a->z, b->x) - fpMul(a->x, b->z);
    c->z = fpMul(a->x, b->y) - fpMul(a->y, b->x);
    return(c);
}

/* scales the input vector to the new length and returns it */
Vector3D *V3Scale(Vector3D *v, FixedPoint newlen) {
    FixedPoint len;
    
    len = V3Length(v);
    
    if (len != 0.0) {
   v->x = mulDiv(v->x,newlen,len);
   v->y = mulDiv(v->y,newlen,len);
   v->z = mulDiv(v->z,newlen,len);
    }
    return(v);
}

// from drawbsp.c
extern long shade_amount;

/*****************************************************************************************/
/*
 * LoadSlopeInfo:  Load data for sloped surface descriptions and generate
 *   runtime representation of the data describing the texture coordinates.
 *   Return pointer to new SlopeData record on success, NULL on error
 */
SlopeData *LoadSlopeInfo(file_node *f) {
    int       size;
    long      txt_angle;
    long      index;
    SlopeData *new_slope;
    Vector3D  texture_orientation;
    Vector3D  v1,v2;
    char      junk[6];

    // create a new slope record
    size = sizeof(SlopeData);
    new_slope = (SlopeData *) SafeMalloc(size);
    memset(new_slope, 0, size);
    // Load coefficients of plane equation as floats.
    if (CliMappedFileRead(f, &new_slope->plane.a, 4) != 4) return (SlopeData *)NULL;
    if (CliMappedFileRead(f, &new_slope->plane.b, 4) != 4) return (SlopeData *)NULL;
    if (CliMappedFileRead(f, &new_slope->plane.c, 4) != 4) return (SlopeData *)NULL;
    if (CliMappedFileRead(f, &new_slope->plane.d, 4) != 4) return (SlopeData *)NULL;

    //dprintf("loaded equation a = %6.4f, b = %6.4f, c = %6.4f, d = %6.4f\n",
    //  new_slope->plane.a, new_slope->plane.b, new_slope->plane.c, new_slope->plane.d);
    
    if (new_slope->plane.c == 0) {
   debug(("Error: loaded plane equation equal to a vertical slope\n"));
   // punt on error and stick in non crashing values (use assert instead?)
   new_slope->plane.a = 0;
   new_slope->plane.b = 0;
   new_slope->plane.c = 1024;
   new_slope->plane.d = 0;
    }
    
    // Load x & y of texture origin. These are saved as integers, so load them
    // into a temp variable first and them save them.
    int payload;
    if (CliMappedFileRead(f, &payload, 4) != 4) return (SlopeData *)NULL;
    new_slope->p0.x = payload;
    if (CliMappedFileRead(f, &payload, 4) != 4) return (SlopeData *)NULL;
    new_slope->p0.y = payload;
    
    // calculate z of texture origin from x, y, and plane equation
    new_slope->p0.z = (-new_slope->plane.a*new_slope->p0.x - new_slope->plane.b*new_slope->p0.y - new_slope->plane.d)/new_slope->plane.c;

    // load in texture angle - this is planar angle between x axis of texture & x axis of world
    if (CliMappedFileRead(f, &txt_angle, 4) != 4) return (SlopeData *)NULL;

   new_slope->texRot = txt_angle;
    
    // convert angle to vector
    texture_orientation.x = COS(txt_angle) >> 6;
    texture_orientation.y = SIN(txt_angle) >> 6;
    texture_orientation.z = 0;

    // generate other endpoints from plane normal, texture origin, and texture
    //  orientation which determine the orientation of the texture's u v space
    //  in the 3d world's x, y, z space
    
    // cross normal with texture orientation to get vector perpendicular to texture
    //  orientation and normal = v axis direction
    V3Cross((Vector3D *)&(new_slope->plane), &texture_orientation, &v2);
    // scale to size of texture in world space
    V3Scale(&v2, FINENESS);
    
    // cross normal with v axis direction vector to get vector perpendicular to v axis
    //  and normal = u axis direction vector
    V3Cross(&v2, (Vector3D *)&(new_slope->plane), &v1);
    // scale to size of texture in world space
    V3Scale(&v1, FINENESS);

    // add vectors to origin to get endpoints
    V3Add(&new_slope->p0, &v1, &new_slope->p1);
    V3Add(&new_slope->p0, &v2, &new_slope->p2);

    // set flags indicating properties of the slope
    new_slope->flags = 0;

    if (ABS(new_slope->plane.c) < DIRECTIONAL_THRESHOLD)
   new_slope->flags |= SLF_DIRECTIONAL;
    else if (new_slope->plane.c < 0) // ceiling, apply same lighting hack as regular ceilings (see doDrawLeaf)
   new_slope->lightscale = FINENESS-(shade_amount>>1);
    else
   new_slope->lightscale = FINENESS;
    
    // it's not clear that anyone will actually use this flag, but we talked about using it once
    if ((new_slope->plane.c >= 0) && (new_slope->plane.c < TOO_STEEP_THRESHOLD))
        new_slope->flags |= SLF_TOO_STEEP;

    for (index=0; index < 3; index++)
       // Skip unused bytes
       if (CliMappedFileRead(f, &junk, 6) != 6) return (SlopeData *)NULL;

    return new_slope;
}

/*****************************************************************************************/
/*
 * LoadSectors:  Load sectors, and set global sectors variable to array of sectors.
 *   Return True on success.
 */
Bool LoadSectors(file_node *f, room_type *room, int num_sectors)
{
   int i, size, period;
   WORD word;
   BYTE speed;

   size = num_sectors * sizeof(Sector);
   room->sectors = (Sector *) SafeMalloc(size);
   memset(room->sectors, 0, size);

   for (i=0; i < num_sectors; i++)
   {
      Sector *s = &room->sectors[i];
      
      if (CliMappedFileRead(f, &s->server_id, 2) != 2) return False;
      security += s->server_id;
      
      // Get floor and ceiling bitmaps
      if (CliMappedFileRead(f, &s->floor_type, 2) != 2) return False;
      s->floor = GetGridPdib(s->floor_type);
      if (CliMappedFileRead(f, &s->ceiling_type, 2) != 2) return False;
      s->ceiling = GetGridPdib(s->ceiling_type);
      security += s->floor_type + s->ceiling_type;

      // Get texture origin
      if (CliMappedFileRead(f, &word, 2) != 2) return False;
      s->tx = FinenessKodToClient(word);
      if (CliMappedFileRead(f, &word, 2) != 2) return False;
      s->ty = FinenessKodToClient(word);

      // Get floor and ceiling heights
      if (CliMappedFileRead(f, &word, 2) != 2) return False;
      security += word;
      s->floor_height = HeightKodToClient(word);
      if (CliMappedFileRead(f, &word, 2) != 2) return False;
      security += word;
      s->ceiling_height = HeightKodToClient(word);
 
      if (CliMappedFileRead(f, &s->light, 1) != 1) return False;
      security += s->light;

      if (CliMappedFileRead(f, &s->flags, 4) != 4) return False;
      security += s->flags;

      // XXX Old version
      if (room_version >= 10)
      {
         if (CliMappedFileRead(f, &speed, 1) != 1) return False;
      }
      else
         speed = 0;

      // Set up animation, if any
      if (speed != 0)
      {
         s->animate = (RoomAnimate *) SafeMalloc(sizeof(RoomAnimate));
         RoomSetupCycleAnimation(s->animate, speed);
      }
      else if (SectorScrollSpeed(s->flags) != SCROLL_NONE)
      {
         speed = SectorScrollSpeed(s->flags);
         switch (speed)
         {
         case SCROLL_MEDIUM: period = SCROLL_MEDIUM_PERIOD; break;
         case SCROLL_FAST:   period = SCROLL_FAST_PERIOD;   break;
         default: period = SCROLL_SLOW_PERIOD; // case SCROLL_SLOW
         }
         s->animate = (RoomAnimate *) SafeMalloc(sizeof(RoomAnimate));
         RoomSetupScrollAnimation(s->animate, period, SectorScrollDirection(s->flags));
      }
      else if (s->flags & SF_FLICKER)
      {
         s->animate = (RoomAnimate *) SafeMalloc(sizeof(RoomAnimate));
         RoomSetupFlickerAnimation(s->animate, s->light, s->server_id);
      }
      else
         s->animate = NULL;

      if (s->flags & SF_SLOPED_FLOOR)
      {
         s->sloped_floor = LoadSlopeInfo(f);
         if (s->sloped_floor == (SlopeData *)NULL)
            return False;
      }

      if (s->flags & SF_SLOPED_CEILING)
      {
         s->sloped_ceiling = LoadSlopeInfo(f);
         if (s->sloped_ceiling == (SlopeData *)NULL)
            return False;
      }
   }
   return True;
}

Bool LoadThings(file_node *f, room_type *room, int num_things)
{
   int temp;

   // note: Things vertices are stored as INT in (1:64) fineness, based on the
   // coordinate-system origin AS SHOWN IN ROOMEDIT (Y-UP).
   // Also these can be ANY variant of the 2 possible sets describing
   // a diagonal in a rectangle, so not guaranteed to be ordered like min/or max first.
   float x0, x1, y0, y1;

   if (CliMappedFileRead(f, &temp, 4) != 4) return False;
   x0 = (float)temp;
   if (CliMappedFileRead(f, &temp, 4) != 4) return False;
   y0 = (float)temp;
   if (CliMappedFileRead(f, &temp, 4) != 4) return False;
   x1 = (float)temp;
   if (CliMappedFileRead(f, &temp, 4) != 4) return False;
   y1 = (float)temp;

   // from the 4 bbox points shown in roomedit (defined by 2 vertices)
   // 1) Pick the left-bottom one as minimum (and scale to ROO fineness)
   // 2) Pick the right-up one as maximum (and scale to ROO fineness)
   room->ThingsBox.Min.X = FINENESSKODTOROO(fmin(x0, x1));
   room->ThingsBox.Min.Y = FINENESSKODTOROO(fmin(y0, y1));
   room->ThingsBox.Max.X = FINENESSKODTOROO(fmax(x0, x1));
   room->ThingsBox.Max.Y = FINENESSKODTOROO(fmax(y0, y1));

   // translate box so minimum is at (0/0)
   room->ThingsBox.Max.X = room->ThingsBox.Max.X - room->ThingsBox.Min.X;
   room->ThingsBox.Max.Y = room->ThingsBox.Max.Y - room->ThingsBox.Min.Y;
   room->ThingsBox.Min.X = 0.0f;
   room->ThingsBox.Min.Y = 0.0f;

   // calculate the old cols/rows values rather than loading them
   room->cols = (WORD)(room->ThingsBox.Max.X / 1024.0f);
   room->rows = (WORD)(room->ThingsBox.Max.Y / 1024.0f);

   return True;
}
/*****************************************************************************************/
/*
 * RoomSwizzle:  Convert node and wall id numbers loaded from the file into pointers 
 *   directly to these nodes and walls.
 *   Return True if all id numbers are legal, and so conversion was successful.
 */
#define MAXLONG 0x7fffffff
#define OVERFLOWAMOUNT (MAXLONG>>(LOG_FINENESS*2))

Bool RoomSwizzle(room_type *room, BSPTree tree, 
      int num_nodes, int num_walls, int num_sidedefs, int num_sectors)
{
   BSPinternal *inode;
   BSPleaf *leaf;
   WallData *wall;
   float len = 0.0f;

   if (tree == NULL)
      return True;

   switch (tree->type)
   {
   case BSPinternaltype:
      inode = &tree->u.internal;

      // normalize plane separator equation
      len = sqrtf(
         inode->separator.a * inode->separator.a +
         inode->separator.b * inode->separator.b);

      // normalize
      if (!ISZERO(len))
      {
         inode->separator.a /= len;
         inode->separator.b /= len;
         inode->separator.c /= len;
      }

      if (inode->pos_num == 0)
         inode->pos_side = NULL;
      else if (inode->pos_num < 0 || inode->pos_num > num_nodes)
      {
         debug(("RoomSwizzle got node #%d; max is %d\n", inode->pos_num, num_nodes));
         inode->pos_side = NULL;
      }
      else
         inode->pos_side = &room->nodes[inode->pos_num - 1];

      if (inode->neg_num == 0)
         inode->neg_side = NULL;
      else if (inode->neg_num < 0 || inode->neg_num > num_nodes)
      {
         debug(("RoomSwizzle got node #%d; max is %d", inode->neg_num, num_nodes));
         inode->neg_side = NULL;
      }
      else
         inode->neg_side = &room->nodes[inode->neg_num - 1];

      if (inode->wall_num == 0)
         inode->walls_in_plane = NULL;
      else if (inode->wall_num < 0 || inode->wall_num > num_walls)
      {
         debug(("RoomSwizzle got wall #%d; max is %d\n", inode->wall_num, num_walls));
         inode->walls_in_plane = NULL;
      }
      else
         inode->walls_in_plane = &room->walls[inode->wall_num - 1];

      // Swizzle wall list
      if (inode->walls_in_plane != NULL)
      {
         // Set next pointers to build up list
         for (wall = inode->walls_in_plane; wall->next_num != 0; wall = wall->next)
         {
            if (wall->next_num < 0 || wall->next_num > num_walls)
            {
               debug(("RoomSwizzle got wall #%d; max is %d\n", wall->next_num, num_walls));
               return False;
            }
            else
               wall->next = &room->walls[wall->next_num - 1];
         }
         wall->next = NULL;

         // Set sidedef and sector pointers
         for (wall = inode->walls_in_plane; wall != NULL; wall = wall->next)
         {
            if (wall->pos_sidedef_num > num_sidedefs)
            {
               debug(("RoomSwizzle found wall referencing sidedef %d; max is %d\n", 
                  wall->pos_sidedef_num, num_sidedefs));
               return False;
            }
            if (wall->pos_sidedef_num == 0)
               wall->pos_sidedef = NULL;
            else
               wall->pos_sidedef = &room->sidedefs[wall->pos_sidedef_num - 1];

            if (wall->neg_sidedef_num > num_sidedefs)
            {
               debug(("RoomSwizzle found wall referencing sidedef %d; max is %d\n", 
                  wall->neg_sidedef_num, num_sidedefs));
               return False;
            }
            if (wall->neg_sidedef_num == 0)
               wall->neg_sidedef = NULL;
            else
               wall->neg_sidedef = &room->sidedefs[wall->neg_sidedef_num - 1];

            if (wall->pos_sector_num > num_sectors)
            {
               debug(("RoomSwizzle found wall referencing sector %d; max is %d\n", 
                  wall->pos_sector_num, num_sectors));
               return False;
            }

            if (wall->pos_sector_num == 0)
               wall->pos_sector = NULL;
            else
               wall->pos_sector = &room->sectors[wall->pos_sector_num - 1];

            if (wall->neg_sector_num > num_sectors)
            {
               debug(("RoomSwizzle found wall referencing sector %d; max is %d\n", 
                  wall->neg_sector_num, num_sectors));
               return False;
            }

            if (wall->neg_sector_num == 0)
               wall->neg_sector = NULL;
            else
               wall->neg_sector = &room->sectors[wall->neg_sector_num - 1];

            SetWallHeights(wall);
         }
      }

      if (RoomSwizzle(room, inode->pos_side, num_nodes, num_walls, num_sidedefs, num_sectors) == False)
         return False;
      if (RoomSwizzle(room, inode->neg_side, num_nodes, num_walls, num_sidedefs, num_sectors) == False)
         return False;
      break;
      
   case BSPleaftype:
      leaf = &tree->u.leaf;

      if (leaf->sector_num > num_sectors)
      {
         debug(("RoomSwizzle found leaf referencing sector %d; max is %d\n", 
            leaf->sector, num_sectors));
         return False;
      }
      if (leaf->sector_num == 0)
      {
         debug(("RoomSwizzle found leaf without sector reference\n"));
         return False;
      }
      leaf->sector = &room->sectors[leaf->sector_num - 1];

      break;
   }

   return True;
}

void PrecalcWallData(WallData *pWall, unsigned int type, int side)
{
   int drawTopDown;
   Sidedef *pSideDef = NULL;
   PDIB pDib;
   custom_xyz *pXYZ, *pNormal;
   custom_st *pST;
   unsigned int *flags;
   int xOffset, yOffset, top, bottom;


   if (side > 0)
   {
      pSideDef = pWall->pos_sidedef;
      if (!pSideDef)
         return;
      xOffset = pWall->pos_xoffset;
      yOffset = pWall->pos_yoffset;

      if (type == D3DRENDER_WALL_NORMAL)
      {
         pDib = pWall->pos_sidedef->normal_bmap;
         if (!pDib)
            return;

         pST = pWall->pos_normal_stBase;
         pXYZ = pWall->pos_normal_xyz;
         pNormal = &pWall->pos_normal_normal;
         flags = &pWall->pos_normal_d3dFlags;

         pXYZ[0].z = (float)pWall->z2;
         pXYZ[3].z = (float)pWall->zz2;
         pXYZ[1].z = (float)pWall->z1;
         pXYZ[2].z = (float)pWall->zz1;
         drawTopDown = ((pSideDef->flags & WF_NORMAL_TOPDOWN) == WF_NORMAL_TOPDOWN);
      }
      else if (type == D3DRENDER_WALL_BELOW)
      {
         pDib = pWall->pos_sidedef->below_bmap;
         if (!pDib)
            return;

         pXYZ = pWall->pos_below_xyz;
         pNormal = &pWall->pos_below_normal;
         pST = pWall->pos_below_stBase;
         flags = &pWall->pos_below_d3dFlags;

         if ((pWall->bowtie_bits & BT_BELOW_POS)
            || (pWall->bowtie_bits & BT_BELOW_NEG))
         {
            pXYZ[0].z = (float)pWall->z1Neg;
            pXYZ[3].z = (float)pWall->zz1Neg;
         }
         else
         {
            pXYZ[0].z = (float)pWall->z1;
            pXYZ[3].z = (float)pWall->zz1;
         }

         pXYZ[1].z = (float)pWall->z0;
         pXYZ[2].z = (float)pWall->zz0;
         drawTopDown = ((pSideDef->flags & WF_BELOW_TOPDOWN) == WF_BELOW_TOPDOWN);
      }
      else if (type == D3DRENDER_WALL_ABOVE)
      {
         pDib = pWall->pos_sidedef->above_bmap;
         if (!pDib)
            return;

         pXYZ = pWall->pos_above_xyz;
         pNormal = &pWall->pos_above_normal;
         pST = pWall->pos_above_stBase;
         flags = &pWall->pos_above_d3dFlags;

         pXYZ[0].z = (float)pWall->z3;
         pXYZ[3].z = (float)pWall->zz3;
         pXYZ[1].z = (float)pWall->z2;
         pXYZ[2].z = (float)pWall->zz2;
         drawTopDown = !((pSideDef->flags & WF_ABOVE_BOTTOMUP) == WF_ABOVE_BOTTOMUP);
      }
      else
      {
         debug(("Invalid wall type %u in PrecalcWallData, not saving wall!.\n", type));

         return;
      }

      pXYZ[0].x = pWall->x0;
      pXYZ[3].x = pWall->x1;
      pXYZ[1].x = pWall->x0;
      pXYZ[2].x = pWall->x1;

      pXYZ[0].y = pWall->y0;
      pXYZ[3].y = pWall->y1;
      pXYZ[1].y = pWall->y0;
      pXYZ[2].y = pWall->y1;
   }
   else if (side < 0)
   {
      pSideDef = pWall->neg_sidedef;
      if (!pSideDef)
         return;

      xOffset = pWall->neg_xoffset;
      yOffset = pWall->neg_yoffset;

      if (type == D3DRENDER_WALL_NORMAL)
      {
         pDib = pWall->neg_sidedef->normal_bmap;
         if (!pDib)
            return;

         pXYZ = pWall->neg_normal_xyz;
         pNormal = &pWall->neg_normal_normal;
         pST = pWall->neg_normal_stBase;
         flags = &pWall->neg_normal_d3dFlags;

         pXYZ[0].z = (float)pWall->zz2;
         pXYZ[3].z = (float)pWall->z2;
         pXYZ[1].z = (float)pWall->zz1;
         pXYZ[2].z = (float)pWall->z1;
         drawTopDown = ((pSideDef->flags & WF_NORMAL_TOPDOWN) == WF_NORMAL_TOPDOWN);
      }
      else if (type == D3DRENDER_WALL_BELOW)
      {
         pDib = pWall->neg_sidedef->below_bmap;
         if (!pDib)
            return;
         pXYZ = pWall->neg_below_xyz;
         pNormal = &pWall->neg_below_normal;
         pST = pWall->neg_below_stBase;
         flags = &pWall->neg_below_d3dFlags;

         pXYZ[0].z = (float)pWall->zz1;
         pXYZ[3].z = (float)pWall->z1;
         pXYZ[1].z = (float)pWall->zz0;
         pXYZ[2].z = (float)pWall->z0;
         drawTopDown = ((pSideDef->flags & WF_BELOW_TOPDOWN) == WF_BELOW_TOPDOWN);
      }
      else if (type == D3DRENDER_WALL_ABOVE)
      {
         pDib = pWall->neg_sidedef->above_bmap;
         if (!pDib)
            return;
         pXYZ = pWall->neg_above_xyz;
         pNormal = &pWall->neg_above_normal;
         pST = pWall->neg_above_stBase;
         flags = &pWall->neg_above_d3dFlags;

         pXYZ[0].z = (float)pWall->zz3;
         pXYZ[3].z = (float)pWall->z3;
         pXYZ[1].z = (float)pWall->zz2;
         pXYZ[2].z = (float)pWall->z2;
         drawTopDown = !((pSideDef->flags & WF_ABOVE_BOTTOMUP) == WF_ABOVE_BOTTOMUP);
      }
      else
      {
         debug(("Invalid wall type %u in PrecalcWallData, not saving wall!.\n", type));

         return;
      }

      pXYZ[0].x = pWall->x1;
      pXYZ[3].x = pWall->x0;
      pXYZ[1].x = pWall->x1;
      pXYZ[2].x = pWall->x0;

      pXYZ[0].y = pWall->y1;
      pXYZ[3].y = pWall->y0;
      pXYZ[1].y = pWall->y1;
      pXYZ[2].y = pWall->y0;
   }
   else
   {
      debug(("Got side == 0 in PrecalcWallData! Not saving wall data.\n"));

      return;
   }

   *flags = 0;

   if (pSideDef->flags & WF_TRANSPARENT)
      *flags |= D3DRENDER_TRANSPARENT;

   if (type == D3DRENDER_WALL_NORMAL && (pSideDef->flags & WF_NO_VTILE))
      *flags |= D3DRENDER_NO_VTILE;

   float invWidth, invHeight, invWidthFudge, invHeightFudge;
   // force a wraparound because many textures in the old client do this, grr
   yOffset = yOffset << 16;
   yOffset = yOffset >> 16;

   invWidth = 1.0f / (float)pDib->width;
   invHeight = 1.0f / (float)pDib->height;
   invWidthFudge = 1.0f / ((float)pDib->width * PETER_FUDGE);
   invHeightFudge = 1.0f / ((float)pDib->height * PETER_FUDGE);

   pST[0].t = (float)xOffset * (float)(pDib->shrink) * invHeight;
   pST[1].t = (float)xOffset * (float)(pDib->shrink) * invHeight;
   pST[3].t = (float)(pST[0].t + ((float)pWall->length * (float)pDib->shrink) * invHeight);
   pST[2].t = (float)(pST[1].t + ((float)pWall->length * (float)pDib->shrink) * invHeight);

   if (!drawTopDown)
   {
      if (pXYZ[1].z == pXYZ[2].z)
         bottom = pXYZ[1].z;
      else
      {
         bottom = min(pXYZ[1].z, pXYZ[2].z);
         bottom &= ~(FINENESS - 1);
      }

      if (pXYZ[0].z == pXYZ[3].z)
         top = pXYZ[0].z;
      else
      {
         top = max(pXYZ[0].z, pXYZ[3].z);
         top = (top + FINENESS - 1) & ~(FINENESS - 1);
      }

      if (pXYZ[1].z == pXYZ[2].z)
      {
         pST[1].s = 1.0f - (((float)yOffset * (float)pDib->shrink) * invWidth);
         pST[2].s = 1.0f - (((float)yOffset * (float)pDib->shrink) * invWidth);
      }
      else
      {
         pST[1].s = 1.0f - (((float)yOffset * (float)pDib->shrink) *invWidth);
         pST[2].s = 1.0f - (((float)yOffset * (float)pDib->shrink) * invWidth);
         pST[1].s -= (pXYZ[1].z - bottom) * (float)pDib->shrink * invWidthFudge;
         pST[2].s -= (pXYZ[2].z - bottom) * (float)pDib->shrink * invWidthFudge;
      }

      pST[0].s = pST[1].s - ((pXYZ[0].z - pXYZ[1].z) * (float)pDib->shrink * invWidthFudge);
      pST[3].s = pST[2].s - ((pXYZ[3].z - pXYZ[2].z) * (float)pDib->shrink * invWidthFudge);
   }
   else   // else, need to place tex origin at top left
   {
      if (pXYZ[0].z == pXYZ[3].z)
         top = pXYZ[0].z;
      else
      {
         top = max(pXYZ[0].z, pXYZ[3].z);
         top = (top + FINENESS - 1) & ~(FINENESS - 1);
      }

      if (pXYZ[1].z == pXYZ[2].z)
         bottom = pXYZ[1].z;
      else
      {
         bottom = min(pXYZ[1].z, pXYZ[2].z);
         bottom &= ~(FINENESS - 1);
      }

      if (pXYZ[0].z == pXYZ[3].z)
      {
         pST[0].s = 0.0f;
         pST[3].s = 0.0f;
      }
      else
      {
         pST[0].s = ((float)top - pXYZ[0].z) * (float)pDib->shrink * invWidthFudge;
         pST[3].s = ((float)top - pXYZ[3].z) * (float)pDib->shrink * invWidthFudge;
      }

      pST[0].s -= ((float)(yOffset * pDib->shrink) * invWidth);
      pST[3].s -= ((float)(yOffset * pDib->shrink) * invWidth);

      pST[1].s = pST[0].s + ((pXYZ[0].z - pXYZ[1].z) * (float)pDib->shrink * invWidthFudge);
      pST[2].s = pST[3].s + ((pXYZ[3].z - pXYZ[2].z) * (float)pDib->shrink * invWidthFudge);
   }

   if (pSideDef->flags & WF_BACKWARDS)
   {
      float temp;
      temp = pST[3].t;
      pST[3].t = pST[0].t;
      pST[0].t = temp;

      temp = pST[2].t;
      pST[2].t = pST[1].t;
      pST[1].t = temp;
   }
   /*if (pSideDef->flags & WF_NO_VTILE)
   {
   if (pST[1].t > 0.99f)
   pST[1].t = 0.99f;

   if (pST[2].t > 0.99f)
   pST[2].t = 0.99f;
   }*/

   if (*flags & D3DRENDER_NO_VTILE)
   {
      if (pST[0].s < 0.0f)
      {
         float   tex, wall, ratio, temp;

         tex = pST[1].s - pST[0].s;
         if (tex == 0)
            tex = 1.0f;
         temp = -pST[0].s;
         ratio = temp / tex;

         wall = pXYZ[0].z - pXYZ[1].z;
         temp = wall * ratio;
         pXYZ[0].z -= temp;
         pST[0].s = 0.0f;
      }
      if (pST[3].s < 0.0f)
      {
         float   tex, wall, ratio, temp;

         tex = pST[2].s - pST[3].s;
         if (tex == 0)
            tex = 1.0f;
         temp = -pST[3].s;
         ratio = temp / tex;

         wall = pXYZ[3].z - pXYZ[2].z;
         temp = wall * ratio;
         pXYZ[3].z -= temp;
         pST[3].s = 0.0f;
      }

      pXYZ[1].z -= 16.0f;
      pXYZ[2].z -= 16.0f;
   }

   pST[0].s += 1.0f / pDib->width;
   pST[3].s += 1.0f / pDib->width;
   pST[1].s -= 1.0f / pDib->width;
   pST[2].s -= 1.0f / pDib->width;

   // Normals
   custom_xyz vec0, vec1;

   // calc cross product, get normal and determine major axis
   vec0.x = pXYZ[1].x - pXYZ[0].x;
   vec0.y = pXYZ[1].y - pXYZ[0].y;
   vec0.z = pXYZ[1].z - pXYZ[0].z;

   vec1.x = pXYZ[3].x - pXYZ[0].x;
   vec1.y = pXYZ[3].y - pXYZ[0].y;
   vec1.z = pXYZ[3].z - pXYZ[0].z;

   pNormal->x = vec0.z * vec1.y - vec0.y * vec1.z;
   pNormal->z = vec0.y * vec1.x - vec0.x * vec1.y;
   pNormal->y = vec0.x * vec1.z - vec0.z * vec1.x;

   if (pNormal->x < 0)
      pNormal->x = -pNormal->x;

   if (pNormal->y < 0)
      pNormal->y = -pNormal->y;
}

void PrecalcWallDataAll(WallData *pWall)
{
   pWall->seen &= ~WF_CANDRAWMASK;

   if (pWall->pos_sidedef)
   {
      if (pWall->pos_sidedef->normal_bmap && (((short)pWall->z2 != (short)pWall->z1)
         || ((short)pWall->zz2 != (short)pWall->zz1)))
      {
         pWall->seen |= WF_CANDRAWNORMAL;
         PrecalcWallData(pWall, D3DRENDER_WALL_NORMAL, 1);
      }

      if (pWall->pos_sidedef->below_bmap && (((short)pWall->z1 != (short)pWall->z0)
         || ((short)pWall->zz1 != (short)pWall->zz0)))
      {
         pWall->seen |= WF_CANDRAWBELOW;
         PrecalcWallData(pWall, D3DRENDER_WALL_BELOW, 1);
      }

      if (pWall->pos_sidedef->above_bmap && (((short)pWall->z3 != (short)pWall->z2)
         || ((short)pWall->zz3 != (short)pWall->zz2)))
      {
         pWall->seen |= WF_CANDRAWABOVE;
         PrecalcWallData(pWall, D3DRENDER_WALL_ABOVE, 1);
      }
   }

   if (pWall->neg_sidedef)
   {
      if (pWall->neg_sidedef->normal_bmap && (((short)pWall->z2 != (short)pWall->z1)
         || ((short)pWall->zz2 != (short)pWall->zz1)))
      {
         pWall->seen |= WF_CANDRAWNORMAL;
         PrecalcWallData(pWall, D3DRENDER_WALL_NORMAL, -1);
      }
      if (pWall->neg_sidedef->below_bmap && (((short)pWall->z1 != (short)pWall->z0)
         || ((short)pWall->zz1 != (short)pWall->zz0)))
      {
         pWall->seen |= WF_CANDRAWBELOW;
         PrecalcWallData(pWall, D3DRENDER_WALL_BELOW, -1);
      }
      if (pWall->neg_sidedef->above_bmap && (((short)pWall->z3 != (short)pWall->z2)
         || ((short)pWall->zz3 != (short)pWall->zz2)))
      {
         pWall->seen |= WF_CANDRAWABOVE;
         PrecalcWallData(pWall, D3DRENDER_WALL_ABOVE, -1);
      }
   }
}

// Precalculate some internal node data (walls) so it doesn't have
// to be done during frame rendering.
void PrecalcInternalNodeData(BSPinternal *iNode)
{
   WallData *pWall;

   for (pWall = iNode->walls_in_plane; pWall != NULL; pWall = pWall->next)
   {
      pWall->separator.a = iNode->separator.a;
      pWall->separator.b = iNode->separator.b;
      pWall->separator.c = iNode->separator.c;

      PrecalcWallDataAll(pWall);
   }
}

// Precalculate some leaf data (floors/ceilings) so it doesn't have
// to be done during frame rendering. This is kept in a separate data
// structure than floor_xyz etc in BSPnode, because we add dynamic
// info to those alongside the original data from BSPleaf. Walls are
// handled differently due to the number of extra fields that would
// be needed.
void PrecalcLeafData(BSPleaf *leaf)
{
   float left = 0, top = 0;
   // Pre-calc some floor/ceiling stuff

   // Ceiling texture coords.
   for (int i = 0; i < leaf->poly.npts; ++i)
   {
      if (leaf->poly.p[i].x < left)
         left = leaf->poly.p[i].x;
      if (leaf->poly.p[i].y < top)
         top = leaf->poly.p[i].y;
   }

   // Floor texture coords.

   for (int i = 0; i < leaf->poly.npts; ++i)
   {
      if (leaf->sector->sloped_ceiling)
      {
         // Texture coords

         BSPleafdata *ceil = &leaf->ceil;
         SlopeData *slope = leaf->sector->sloped_ceiling;

         ceil->xyz[i].x = leaf->poly.p[i].x;
         ceil->xyz[i].y = leaf->poly.p[i].y;
         ceil->xyz[i].z = (-slope->plane.a * ceil->xyz[i].x
               - slope->plane.b * ceil->xyz[i].y
               - slope->plane.d) * (1.0f / slope->plane.c);

         // Ceiling ST
         custom_xyz   vectorU, vectorV, vector;
         custom_xyz   intersectTop, intersectLeft;
         float      U, temp, distance;

         // calc distance from top line (vector u)
         U = ((ceil->xyz[i].x - slope->p0.x) * (slope->p1.x - slope->p0.x))
            + ((ceil->xyz[i].z - slope->p0.z) * (slope->p1.z - slope->p0.z))
            + ((ceil->xyz[i].y - slope->p0.y) * (slope->p1.y - slope->p0.y));
         temp = ((slope->p1.x - slope->p0.x) * (slope->p1.x - slope->p0.x))
            + ((slope->p1.z - slope->p0.z) * (slope->p1.z - slope->p0.z))
            + ((slope->p1.y - slope->p0.y) * (slope->p1.y - slope->p0.y));

         if (temp == 0)
            temp = 1.0f;

         U /= temp;

         intersectTop.x = slope->p0.x + U * (slope->p1.x - slope->p0.x);
         intersectTop.z = slope->p0.z + U * (slope->p1.z - slope->p0.z);
         intersectTop.y = slope->p0.y + U * (slope->p1.y - slope->p0.y);

         ceil->st[i].t = sqrtf((ceil->xyz[i].x - intersectTop.x) *
            (ceil->xyz[i].x - intersectTop.x) +
            (ceil->xyz[i].z - intersectTop.z) *
            (ceil->xyz[i].z - intersectTop.z) +
            (ceil->xyz[i].y - intersectTop.y) *
            (ceil->xyz[i].y - intersectTop.y));

         // calc distance from left line (vector v)
         U = ((ceil->xyz[i].x - slope->p0.x) * (slope->p2.x - slope->p0.x))
            +  ((ceil->xyz[i].z - slope->p0.z) * (slope->p2.z - slope->p0.z))
            + ((ceil->xyz[i].y - slope->p0.y) * (slope->p2.y - slope->p0.y));
         temp = ((slope->p2.x - slope->p0.x) * (slope->p2.x - slope->p0.x))
            + ((slope->p2.z - slope->p0.z) *  (slope->p2.z - slope->p0.z))
            + ((slope->p2.y - slope->p0.y) * (slope->p2.y - slope->p0.y));

         if (temp == 0)
            temp = 1.0f;

         U /= temp;

         intersectLeft.x = slope->p0.x + U * (slope->p2.x - slope->p0.x);
         intersectLeft.z = slope->p0.z + U * (slope->p2.z - slope->p0.z);
         intersectLeft.y = slope->p0.y + U * (slope->p2.y - slope->p0.y);

         ceil->st[i].s = sqrtf((ceil->xyz[i].x - intersectLeft.x) *
            (ceil->xyz[i].x - intersectLeft.x) +
            (ceil->xyz[i].z - intersectLeft.z) *
            (ceil->xyz[i].z - intersectLeft.z) +
            (ceil->xyz[i].y - intersectLeft.y) *
            (ceil->xyz[i].y - intersectLeft.y));

         // Commented out in original D3D version.
         //ceil->st[i].t += leaf->sector->ty / 2.0f;
         //ceil->st[i].s += leaf->sector->tx / 2.0f;
         //ceil->st[i].s -= pSector->ty / 2.0f;
         //ceil->st[i].t -= pSector->tx / 2.0f;

         vectorU.x = slope->p1.x - slope->p0.x;
         vectorU.z = slope->p1.z - slope->p0.z;
         vectorU.y = slope->p1.y - slope->p0.y;

         distance = sqrtf((vectorU.x * vectorU.x) + (vectorU.y * vectorU.y));

         if (distance == 0)
            distance = 1.0f;

         vectorU.x /= distance;
         vectorU.z /= distance;
         vectorU.y /= distance;

         vectorV.x = slope->p2.x - slope->p0.x;
         vectorV.z = slope->p2.z - slope->p0.z;
         vectorV.y = slope->p2.y - slope->p0.y;

         distance = sqrtf((vectorV.x * vectorV.x) + (vectorV.y * vectorV.y));

         if (distance == 0)
            distance = 1.0f;

         vectorV.x /= distance;
         vectorV.z /= distance;
         vectorV.y /= distance;

         vector.x = ceil->xyz[i].x - slope->p0.x;
         vector.y = ceil->xyz[i].y - slope->p0.y;

         distance = sqrtf((vector.x * vector.x) + (vector.y * vector.y));

         if (distance == 0)
            distance = 1.0f;

         vector.x /= distance;
         vector.y /= distance;

         if (((vector.x * vectorU.x) + (vector.y * vectorU.y)) < 0)
            ceil->st[i].s = -ceil->st[i].s;

         if (((vector.x * vectorV.x) + (vector.y * vectorV.y)) > 0)
            ceil->st[i].t = -ceil->st[i].t;

         ceil->st[i].t -= leaf->sector->ty / 2.0f;
         ceil->st[i].s -= leaf->sector->tx / 2.0f;
      }
      else
      {
         BSPleafdata *ceil = &leaf->ceil;
         ceil->xyz[i].x = leaf->poly.p[i].x;
         ceil->xyz[i].y = leaf->poly.p[i].y;
         ceil->xyz[i].z = (float)leaf->sector->ceiling_height;
         ceil->st[i].t = fabsf(leaf->poly.p[i].y - top) - leaf->sector->ty;
         ceil->st[i].s = fabsf(leaf->poly.p[i].x - left) - leaf->sector->tx;
      }

      if (leaf->sector->sloped_floor)
      {
         BSPleafdata *floor = &leaf->floor;
         SlopeData *slope = leaf->sector->sloped_floor;

         floor->xyz[i].x = leaf->poly.p[i].x;
         floor->xyz[i].y = leaf->poly.p[i].y;
         floor->xyz[i].z = (-slope->plane.a * leaf->poly.p[i].x
               - slope->plane.b * leaf->poly.p[i].y
               - slope->plane.d) * (1.0f / slope->plane.c);

         // Floor ST
         custom_xyz   vectorU, vectorV, vector;
         custom_xyz   intersectTop, intersectLeft;
         float      U, temp, distance;

         // calc distance from top line (vector u)
         U = ((floor->xyz[i].x - slope->p0.x) * (slope->p1.x - slope->p0.x))
            + ((floor->xyz[i].z - slope->p0.z) * (slope->p1.z - slope->p0.z))
            + ((floor->xyz[i].y - slope->p0.y) * (slope->p1.y - slope->p0.y));
         temp = ((slope->p1.x - slope->p0.x) * (slope->p1.x - slope->p0.x))
            + ((slope->p1.z - slope->p0.z) * (slope->p1.z - slope->p0.z))
            + ((slope->p1.y - slope->p0.y) * (slope->p1.y - slope->p0.y));

         if (temp == 0)
            temp = 1.0f;

         U /= temp;

         intersectTop.x = slope->p0.x + U * (slope->p1.x - slope->p0.x);
         intersectTop.z = slope->p0.z + U * (slope->p1.z - slope->p0.z);
         intersectTop.y = slope->p0.y + U * (slope->p1.y - slope->p0.y);

         floor->st[i].t = sqrtf((floor->xyz[i].x - intersectTop.x) *
            (floor->xyz[i].x - intersectTop.x) +
            (floor->xyz[i].z - intersectTop.z) *
            (floor->xyz[i].z - intersectTop.z) +
            (floor->xyz[i].y - intersectTop.y) *
            (floor->xyz[i].y - intersectTop.y));

         // calc distance from left line (vector v)
         U = ((floor->xyz[i].x - slope->p0.x) * (slope->p2.x - slope->p0.x))
            + ((floor->xyz[i].z - slope->p0.z) * (slope->p2.z - slope->p0.z))
            + ((floor->xyz[i].y - slope->p0.y) * (slope->p2.y - slope->p0.y));
         temp = ((slope->p2.x - slope->p0.x) * (slope->p2.x - slope->p0.x))
            + ((slope->p2.z - slope->p0.z) * (slope->p2.z - slope->p0.z))
            + ((slope->p2.y - slope->p0.y) * (slope->p2.y - slope->p0.y));

         if (temp == 0)
            temp = 1.0f;

         U /= temp;

         intersectLeft.x = slope->p0.x + U * (slope->p2.x - slope->p0.x);
         intersectLeft.z = slope->p0.z + U * (slope->p2.z - slope->p0.z);
         intersectLeft.y = slope->p0.y + U * (slope->p2.y - slope->p0.y);

         floor->st[i].s = sqrtf((floor->xyz[i].x - intersectLeft.x) *
            (floor->xyz[i].x - intersectLeft.x) +
            (floor->xyz[i].z - intersectLeft.z) *
            (floor->xyz[i].z - intersectLeft.z) +
            (floor->xyz[i].y - intersectLeft.y) *
            (floor->xyz[i].y - intersectLeft.y));

         floor->st[i].t += leaf->sector->ty / 2.0f;
         floor->st[i].s += leaf->sector->tx / 2.0f;

         vectorU.x = slope->p1.x - slope->p0.x;
         vectorU.z = slope->p1.z - slope->p0.z;
         vectorU.y = slope->p1.y - slope->p0.y;

         distance = sqrtf((vectorU.x * vectorU.x) + (vectorU.y * vectorU.y));

         if (distance == 0)
            distance = 1.0f;

         vectorU.x /= distance;
         vectorU.z /= distance;
         vectorU.y /= distance;

         vectorV.x = slope->p2.x - slope->p0.x;
         vectorV.z = slope->p2.z - slope->p0.z;
         vectorV.y = slope->p2.y - slope->p0.y;

         distance = sqrtf((vectorV.x * vectorV.x) + (vectorV.y * vectorV.y));

         if (distance == 0)
            distance = 1.0f;

         vectorV.x /= distance;
         vectorV.z /= distance;
         vectorV.y /= distance;

         vector.x = floor->xyz[i].x - slope->p0.x;
         vector.y = floor->xyz[i].y - slope->p0.y;

         distance = sqrtf((vector.x * vector.x) + (vector.y * vector.y));

         if (distance == 0)
            distance = 1.0f;

         vector.x /= distance;
         vector.y /= distance;

         if (((vector.x * vectorU.x) + (vector.y * vectorU.y)) <= 0)
            floor->st[i].s = -(floor->st[i].s);

         if (((vector.x * vectorV.x) + (vector.y * vectorV.y)) > 0)
            floor->st[i].t = -(floor->st[i].t);
      }
      else
      {
         BSPleafdata *floor = &leaf->floor;
         floor->xyz[i].x = leaf->poly.p[i].x;
         floor->xyz[i].y = leaf->poly.p[i].y;
         floor->xyz[i].z = (float)leaf->sector->floor_height;
         floor->st[i].t = fabsf(leaf->poly.p[i].y - top) - leaf->sector->ty;// / pDib->shrink;
         floor->st[i].s = fabsf(leaf->poly.p[i].x - left) - leaf->sector->tx;// / pDib->shrink;
      }
   }
}
/*****************************************************************************/
/*
 * BSPDumpTree:  Print out BSP tree.  Pass in 0 for level to start.
 */
void BSPDumpTree(BSPnode *tree, int level)
{
   int i;
   char indent[100];
   BSPinternal *node;
   BSPleaf *leaf;
   WallData *wall;

   memset(indent, ' ', level * 3);
   indent[level * 3] = 0;

   if (tree == NULL)
   {
      debug(("%s<empty>\n", indent));
      return;
   }

   switch (tree->type)
   {
   case BSPleaftype:
      leaf = &tree->u.leaf;
//      debug(("%sFloor type = %d, ceiling type = %d\n", indent, 
//         leaf->floor_type, leaf->ceiling_type));
      debug(("%sPolygon has %d points: ", indent, leaf->poly.npts));
      for (i=0; i < leaf->poly.npts; i++)
      {
    debug(("(%d %d) ", leaf->poly.p[i].x, leaf->poly.p[i].y));
      }
      debug(("\n"));
      return;

   case BSPinternaltype:
      node = &tree->u.internal;
      
      debug(("%sBounding box = (%d %d) (%d %d)\n", indent, 
         tree->bbox.x0, tree->bbox.y0, 
         tree->bbox.x1, tree->bbox.y1));
      debug(("%sPlane (a, b, c) = (%d %d %d):\n", indent, node->separator.a,
         node->separator.b, node->separator.c));
      debug(("%sWalls:\n", indent));
      for (wall = node->walls_in_plane; wall != NULL; wall = wall->next)
      {
    debug(("%s   (%d %d)-(%d %d) ", indent, 
       wall->x0, wall->y0, 
       wall->x1, wall->y1));
    debug((" z = (%d %d %d %d), +x=%d, -x=%d\n", 
       wall->z0, wall->z1, wall->z2, wall->z3, wall->pos_xoffset, wall->neg_xoffset));
      }

      debug(("%s+ side:\n", indent));
      BSPDumpTree(node->pos_side, level + 1);
      debug(("%s- side:\n", indent));
      BSPDumpTree(node->neg_side, level + 1);
      return;

   default:
      debug(("Unknown node type!\n"));
   }
}
/***************************************************************************/
/*
 * RoomSetupCycleAnimation:  Set up the given RoomAnimate structure for a 
 *   cycling animation.
 */
void RoomSetupCycleAnimation(RoomAnimate *ra, BYTE speed)
{
   Animate *a;

   ra->animation = ANIMATE_ROOM_BITMAP;

   a = &ra->u.bitmap.a;
   a->animation = ANIMATE_CYCLE;
   a->group = 0;
   a->period = 10000 / speed;
   a->tick = a->period;
   a->group_low = a->group_high = 0;
}
/***************************************************************************/
/*
 * RoomSetupScrollAnimation:  Set up the given RoomAnimate structure for a 
 *   scrolling animation.  period and direction characterize the scrolling;
 *   the direction is a constant from bsp.h.
 */
void RoomSetupScrollAnimation(RoomAnimate *ra, int period, BYTE direction)
{
   RoomScroll *s;

   s = &ra->u.scroll;
   ra->animation = ANIMATE_SCROLL;
   s->direction = direction;
   s->period = period;
   s->tick = s->period;

   // Initial offsets don't matter much, since we're scrolling the whole texture
   s->xoffset = 0;
   s->yoffset = 0;
}
/***************************************************************************/
/*
 * RoomSetupFlickerAnimation:  Set up the given RoomAnimate structure for a 
 *   flickering light animation.
 *   original_light gives base light level from which to flicker.
 *   server_id gives the user id # of the sector.
 */
void RoomSetupFlickerAnimation(RoomAnimate *ra, BYTE original_light, WORD server_id)
{
   RoomFlicker *f;

   f = &ra->u.flicker;

   ra->animation = ANIMATE_FLICKER;
   f->period = FLICKER_PERIOD;
   f->tick = f->period;
   
   f->original_light = original_light;
   f->server_id = server_id;
}

/*****************************************************************************/
/*****************************************************************************/
/*  A WORD ABOUT BOWTIES - by Colin Andrews
 */

/*   Bowties are walls on the boundary between a sloped sector and another
 * sloped or normal sector. They have the property that at one endpoint the sector
 * in front of the wall is higher than the one behind, and at the other end of the
 * wall, the sector behind is higher than the one in front. So, it looks something
 * like this if your are looking at it from in front:
 *
 *      .
 *      |\     .
 *      | \   /|
 *      |  \ / |
 *      |   x  |
 *      |  / \ |
 *      | /   \|
 *      |/     '
 *      '
 *
 *   What the user should see when looking at one of these walls depends on which
 * side of the wall the user is standing. The sector on the side of the wall closer
 * to the user always covers everything from one of the diagonals down (for lower
 * walls, up for upper walls) so all that needs to be drawn is the part of the bowtie
 * that extends above the nearer sector:
 *
 *      .
 *       \     .
 *        \   /|
 *         \ / |
 *          x  |
 *           \ |
 *            \|
 *             \
 *              \
 *
 *   Which part of the bowtie shape gets drawn depends on which side of wall the user
 * is on. From the other side of the above example, the other point of the bowtie
 * would extend above the nearer sector. Which point of the bowtie gets drawn also
 * depends on which sector is higher at each endpoint of the wall. In the above example,
 * the closer sector is above at the left endpoint of the wall. If the sector behind
 * the wall is above at the left endpoint, then it would look like this:
 *
 *      .       
 *      |\     .
 *      | \   / 
 *      |  \ /  
 *      |   x   
 *      |  /    
 *      | /     
 *      |/      
 *      /
 *     /
 *
 *   There are four cases that need to be handled by the wall rendering code:
 *
 *                              |
 *                +             |             +
 *                |\     -      |      -     /|
 *                | \   /|      |      |\   /-|
 *                |  \ /+|      |      | \ /--|         Positive sector is above
 *                |   x++|      |      |  x---|          at endpoint 0
 *                |  / \+|      |      | / \--|
 *                | /   \|      |      |/   \-|
 *                |/     +      |      +     \|
 *                -             |             -
 *      endpoint: 0      1      |      1      0
 *                              |
 *  ----------------------------+----------------------------------
 *                              |
 *                -             |             -
 *                |\     +      |      +     /|
 *                |+\   /|      |      |\   / |
 *                |++\ / |      |      |-\ /  |         Negative sector is above
 *                |+++x  |      |      |--x   |          at endpoint 0
 *                |++/ \ |      |      |-/ \  |
 *                |+/   \|      |      |/   \ |
 *                |/     -      |      -     \|
 *                +             |             +
 *      endpoint: 0      1      |      1      0
 *                              |
 *                              |
 *      User is on Positive     |  User is on Negative side of wall
 *        side of wall          |
 *
 *
 *   In the table above, the top row shows how bowties should be drawn when
 * the positive sector is above at the first endpoint. The lower row shows
 * how bowties should be drawn when the negative sector is on top at the
 * first endpoint. The left coloumn shows the bowties when the user is on
 * the positive side of the wall and the right column shows the view from
 * the negative side. The part of the bowtie that is filled in with +'s or -'s
 * is the part that needs to get filled by the wall rendering code. A '+'
 * indicates the positive sidedef and a '-' indicates the negative sidedef.
 *
 *   Bowties in the upper or lower walls are identified by SetWallHeights.
 * It is assumed that bowtied normal walls cannot (or should not) exist. The
 * height values for the WallData struct are filled in as they be would for a
 * normal wall. That is: z0 <= z1 <= z2 <= z3 and zz0 <= zz1 <= zz2 <= zz3.
 * Flag bits are set in the wall struct indicating that the above and or below
 * wall is bowtied and whether each has the positive sector on top at the first
 * endpoint.
 *
 *   When WalkWall sees that a wall is flaged as bowtie, it modifies the z
 * values at either endpoint so that the wall has the proper shape for the side
 * facing the user.
 *
 *   For example, assume that the following is a lower wall bowtie with the
 * negative sector on top at the first endpoint:
 *
 *    z1 -       + zz1
 *       |\     /|
 *       | \   / |
 *       |  \ /  |
 *       |   x   |
 *       |  / \  |
 *       | /   \ |
 *       |/     \|
 *    z0 +       - zz0
 *
 *   When this wall is viewed from the positive side, WalkWall sets z1 = z0 so
 * the wall will look like this:
 *
 *               + zz1
 *              /|
 *             / |
 *            /  |
 *           /   |
 *          /    |
 *         /     |
 *        /      |
 *    z0 + ----- - zz0
 *
 *   The routine can then go on to handle this wall just like a normal wall. Because
 * everything closer than this has already been placed in the cone tree, everything
 * below the original line from z1 to zz0 will be hidden, and the wall will get
 * rendered as:
 *
 *               . 
 *        \     /|
 *         \   / |
 *          \ /  |
 *           \   |
 *            \  |
 *             \ |
 *              \|
 *               \ 
 *                \
 *
 *   This is exactly what needs to get drawn for this bowtie for this situation.
 *
 *****************************************************************************/

// Make sure wall height can be stored in two bytes. Used by SetWallHeights below.
#define WALL_HEIGHT_CHECK(z) if ((z) > 65535) {debug(("SetWallHeights: got wall taller than 65535. %li",z));\
    debug((" Shorten ceiling height or change\nshort to long in WallData struct in bsp.h\n"));\
    (z) = 65535;}

/***************************************************************************
 *
 * SetWallHeights:  Fill in wall heights from adjacent sector heights.
 *    understands about sloped floors and correctly tags bowtied walls
 */
void SetWallHeights(WallData *wall)
{
   Sector *S1, *S2;
   long     S1_height0,S2_height0,S1_height1,S2_height1;

   S1 = wall->pos_sector;
   S2 = wall->neg_sector;

   // If no sectors, give defaults
   if (S1 == NULL && S2 == NULL)
   {
      wall->z0 = wall->z1 = 0;
      wall->z2 = wall->z3 = FINENESS;
      wall->zz0 = wall->zz1 = 0;
      wall->zz2 = wall->zz3 = FINENESS;
      return;
   }

   if (S1 == NULL)
   {
      wall->z0 = wall->z1 = GetFloorHeight(wall->x0, wall->y0, S2);
      wall->z2 = wall->z3 = GetCeilingHeight(wall->x0, wall->y0, S2);
      wall->zz0 = wall->zz1 = GetFloorHeight(wall->x1, wall->y1, S2);
      wall->zz2 = wall->zz3 = GetCeilingHeight(wall->x1, wall->y1, S2);
      PrecalcWallDataAll(wall);
      return;
   }

   if (S2 == NULL)
   {
      wall->z0 = wall->z1 = GetFloorHeight(wall->x0, wall->y0, S1);
      wall->z2 = wall->z3 = GetCeilingHeight(wall->x0, wall->y0, S1);
      wall->zz0 = wall->zz1 = GetFloorHeight(wall->x1, wall->y1, S1);
      wall->zz2 = wall->zz3 = GetCeilingHeight(wall->x1, wall->y1, S1);
      PrecalcWallDataAll(wall);
      return;
   }
   
   S1_height0 = GetFloorHeight(wall->x0, wall->y0, S1);
   S2_height0 = GetFloorHeight(wall->x0, wall->y0, S2);
   S1_height1 = GetFloorHeight(wall->x1, wall->y1, S1);
   S2_height1 = GetFloorHeight(wall->x1, wall->y1, S2);

   // make sure heights fit in short int
   WALL_HEIGHT_CHECK(S1_height0);
   WALL_HEIGHT_CHECK(S2_height0);
   WALL_HEIGHT_CHECK(S1_height1);
   WALL_HEIGHT_CHECK(S2_height1);

   if (S1_height0 > S2_height0) { // S1 is above S2 at first endpoint
       if (S1_height1 >= S2_height1) {
      // normal wall - S1 higher at both ends
      wall->bowtie_bits = 0;

      wall->z1 = S1_height0;
      wall->zz1 = S1_height1;
      wall->z0 = S2_height0;
      wall->zz0 = S2_height1;
       }
       else {
         // bowtie - see notes on bowties above
         wall->bowtie_bits = (BYTE)BT_BELOW_POS; // positive sector is on top at endpoint 0

         if (gD3DEnabled)
         {
            wall->z1 = S1_height0;
            wall->zz1 = S1_height1;
            wall->z0 = S2_height0;
            wall->zz0 = S1_height1;

            wall->z1Neg = S2_height0;
            wall->zz1Neg = S2_height1;
            wall->z0Neg = S2_height0;
            wall->zz0Neg = S1_height1;
         }
         else
         {
            wall->z1 = S1_height0;
            wall->zz1 = S2_height1;
            wall->z0 = S2_height0;
            wall->zz0 = S1_height1;
         }
       }
   }
   else { // S2 above S1 at first endpoint
       if (S2_height1 >= S1_height1) {
      // normal wall - S2 higher at both ends
      wall->bowtie_bits = 0;

      wall->z1 = S2_height0;
      wall->zz1 = S2_height1;
      wall->z0 = S1_height0;
      wall->zz0 = S1_height1;
       }
       else {
         // bowtie - see notes on bowties above
         wall->bowtie_bits = (BYTE)BT_BELOW_NEG; // negative sector is on top at endpoint 0

         if (gD3DEnabled)
         {
            wall->z1 = S1_height0;
            wall->zz1 = S1_height1;
            wall->z0 = S1_height0;
            wall->zz0 = S2_height1;

            wall->z1Neg = S2_height0;
            wall->zz1Neg = S2_height1;
            wall->z0Neg = S1_height0;
            wall->zz0Neg = S2_height1;
         }
         else
         {
            wall->z1 = S2_height0;
            wall->zz1 = S1_height1;
            wall->z0 = S1_height0;
            wall->zz0 = S2_height1;
         }
       }
   }
   
   S1_height0 = GetCeilingHeight(wall->x0, wall->y0, S1);
   S2_height0 = GetCeilingHeight(wall->x0, wall->y0, S2);
   S1_height1 = GetCeilingHeight(wall->x1, wall->y1, S1);
   S2_height1 = GetCeilingHeight(wall->x1, wall->y1, S2);

   // make sure heights fit in short int
   WALL_HEIGHT_CHECK(S1_height0);
   WALL_HEIGHT_CHECK(S2_height0);
   WALL_HEIGHT_CHECK(S1_height1);
   WALL_HEIGHT_CHECK(S2_height1);
   
   if (S1_height0 > S2_height0) {
       if (S1_height1 >= S2_height1) {
      // normal wall - S1 higher at both ends
      wall->bowtie_bits &= (BYTE)~BT_ABOVE_BOWTIE; // Clear above bowtie bits

      wall->z3 = S1_height0;
      wall->zz3 = S1_height1;
      wall->z2 = S2_height0;
      wall->zz2 = S2_height1;
       }
       else {
      // bowtie - see notes on bowties above
      wall->bowtie_bits |= (BYTE)BT_ABOVE_POS; // positive sector is on top at endpoint 0

      wall->z3 = S1_height0;
      wall->zz3 = S2_height1;
      wall->z2 = S2_height0;
      wall->zz2 = S1_height1;
       }
   }
   else {
       if (S2_height1 >= S1_height1) {
      // normal wall - S2 higher at both ends
      wall->bowtie_bits &= (BYTE)~BT_ABOVE_BOWTIE;

      wall->z3 = S2_height0;
      wall->zz3 = S2_height1;
      wall->z2 = S1_height0;
      wall->zz2 = S1_height1;
       }
       else {
      // bowtie - see notes on bowties above
      wall->bowtie_bits |= (BYTE)BT_ABOVE_NEG; // negative sector is on top at endpoint 0

      wall->z3 = S2_height0;
      wall->zz3 = S1_height1;
      wall->z2 = S1_height0;
      wall->zz2 = S2_height1;
       }
   }
   PrecalcWallDataAll(wall);
}
