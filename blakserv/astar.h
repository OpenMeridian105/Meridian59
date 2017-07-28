// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
* astar.h:
*/

#include <list>

#ifndef _ASTAR_H
#define _ASTAR_H

#define ASTARENABLED          1   // turn astar on and off
#define ASTARDEBUG            0   // enables debug logs
#define PREBUILDEDGECACHE     1   // populates edgecache fully on roomload (~ 1-150ms)
#define PATHCACHETOLERANCE    3   // offset that still creates a match on cached path
#define PATHCACHESIZE        32   // cached paths per room
#define NOPATHCACHESIZE      32   // cached unreachable paths per room
#define NOPATHCACHETTL        3   // lifetime of unreachable cache entry
#define NOPATHCACHETOLERANCE  3   // offset that still creates a match on nopath cache
#define CLOSEENOUGHDIST       3   // offset that still creates a match with requested end location
#define DESTBLOCKIGNORE       3   // squares around end that are never marked blocked by objects
#define NUMNEIGHBOURS         8   // FIXED: a square has 8 neighbours (4 straight, 4 diagonal)

#define MAXGRIDROWS        1024   // rows of astar workgrid
#define MAXGRIDCOLS        1024   // cols of astar workgrid
#define MAXHEAPSIZE       16384   // maximum size of heap storing the open-list
#define MAXITERATIONS      8192   // maximum iterations before aborting algorithm and assume nopath
#define MAXPATHLENGTH      1024   // maximum length of a path
#define MAXFASTSTACKSIZE   8192   // maximum size of equal cost elements on the faststack

// Astar work-grid has 1.048.576 squares (1024x1024)
// biggest known grid so far is desertshore3.roo having 391.680 squares.
#define NODESDATASQUARES   MAXGRIDROWS * MAXGRIDCOLS

#define ROOTOGRIDFACT      (1.0f/256.0f)     // converts from ROO to astar grid scale
#define GRIDTOROOFACT      256.0f            // converts from astar grid scale to ROO
#define COST               16384                                     // edge cost straight (pow2)
#define COST_DIAG          ((unsigned int)(M_SQRT2 * (double)COST))  // edge cost diagonal

// edge flags for edge cache
#define EDGECACHE_KNOWS_N  0x0001
#define EDGECACHE_KNOWS_NE 0x0002
#define EDGECACHE_KNOWS_E  0x0004
#define EDGECACHE_KNOWS_SE 0x0008
#define EDGECACHE_KNOWS_S  0x0010
#define EDGECACHE_KNOWS_SW 0x0020
#define EDGECACHE_KNOWS_W  0x0040
#define EDGECACHE_KNOWS_NW 0x0080
#define EDGECACHE_CAN_N    0x0100
#define EDGECACHE_CAN_NE   0x0200
#define EDGECACHE_CAN_E    0x0400
#define EDGECACHE_CAN_SE   0x0800
#define EDGECACHE_CAN_S    0x1000
#define EDGECACHE_CAN_SW   0x2000
#define EDGECACHE_CAN_W    0x4000
#define EDGECACHE_CAN_NW   0x8000

// forward declarations
typedef struct room_type       room_type;
typedef struct astar_node_data astar_node_data;
typedef struct astar_node      astar_node;

// square data that needs zero initialization
typedef struct astar_node_data
{
   astar_node* parent;
   bool        isInClosedList;
   bool        isBlocked;
} astar_node_data;

// persistent square data or at least not requiring zero initialization
typedef struct astar_node
{
   V2               Location;
   int              Row;
   int              Col;
   unsigned int     cost;
   unsigned int     heuristic;
   unsigned int     combined;
   unsigned int     heapindex;
   astar_node_data* Data;
} astar_node;

// stores a found path
typedef struct astar_path
{
   astar_node*  nodes[MAXPATHLENGTH];
   unsigned int head;
} astar_path;

// stores a found unreachable path
typedef struct astar_nopath
{
   astar_node* startnode;
   astar_node* endnode;
   int         tick;
} astar_nopath;

// astar main data
typedef struct astar
{
   _MM_ALIGN16 astar_node*     Heap[MAXHEAPSIZE];              // 4  * 16384       = 64 KB
   _MM_ALIGN16 astar_node*     FastStack[MAXFASTSTACKSIZE];    // 4  * 8192        = 32 KB 
   _MM_ALIGN16 astar_node_data NodesData[NODESDATASQUARES];    // 8  * 1024 * 1024 =  8 MB
   _MM_ALIGN16 astar_node      Grid[MAXGRIDROWS][MAXGRIDCOLS]; // 36 * 1024 * 1024 = 36 MB (SSE: 48 MB)
   astar_node*       StartNode;
   astar_node*       EndNode;
   astar_node*       LastNode;
   int               ObjectID;
   unsigned int      HeapSize;
   unsigned int      FastStackSize;
   room_type*        Room;
} astar;

void AStarInit();
void AStarShutdown();
bool AStarGetStepTowards(room_type* Room, V2* S, V2* E, V2* P, unsigned int* Flags, int ObjectID);
void AStarClearEdgesCache(room_type* Room);
void AStarClearPathCaches(room_type* Room);
void AStarBuildEdgesCache(room_type* Room);

#endif /*#ifndef _ASTAR_H */