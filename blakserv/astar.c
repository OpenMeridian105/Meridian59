// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
* astar.c
*

Improvements over the old implementation:
  * Allocation of gridmemory is only done once when room is loaded
  * Node information is split up between persistent data and data
    which needs to be cleared for each algorithm and can be by single ZeroMemory
  * Whether a node is in the closed set is not saved and looked up by an additional list,
    but rather simply saved by a flag on that node.
  * Uses a binary heap to store the open set
*/

#include "blakserv.h"

#define LCHILD(x) (2 * x + 1)
#define RCHILD(x) (2 * x + 2)
#define PARENT(x) ((x-1) / 2)

#define ISOUTSIDEGRID(r, c) ((r < 0) | (c < 0) | (r >= AStar.Room->rowshighres) | (c >= AStar.Room->colshighres))

astar AStar;

#pragma region HEAP

#define HEAP(i)      (AStar.Heap[i])         // refers to the i-th element on the heap
#define FASTSTACK(i) (AStar.FastStack[i])    // refers to the i-th element on the faststack

bool AStarHeapCheck(room_type* Room, int i)
{
   int squares = Room->rowshighres * Room->colshighres;

   if (i >= squares)
      return true;

   if (!HEAP(i))
      return true;

   unsigned int our = HEAP(i)->combined;
   int lchild = LCHILD(i);
   int rchild = RCHILD(i);

   if (lchild < squares)
   {
      astar_node* node = HEAP(lchild);
      if (node)
      {
         if (node->combined < our)
            return false;
      }
   }
   if (rchild < squares)
   {
      astar_node* node = HEAP(rchild);
      if (node)
      {
         if (node->combined < our)
            return false;
      }
   }

   bool retval = AStarHeapCheck(Room, lchild);
   if (!retval)
      return false;

   return AStarHeapCheck(Room, rchild);
}

void AStarWriteHeapToFile(room_type* Room)
{
   int rows = Room->rowshighres;
   int cols = Room->colshighres;
   char* rowstring = (char *)AllocateMemory(MALLOC_ID_ROOM, 60000);
   FILE *fp = fopen("heapdebug.txt", "a");
   if (fp)
   {
      int maxlayer = 9;	
      int treesize = cols * rows;
      int rangestart = 0;
      int rangesize = 1;
      for (int i = 1; i < maxlayer; i++)
      {
         if (treesize < rangestart + rangesize)
            break;

         sprintf(rowstring, "L:%.2i", i);

         for (int k = 0; k < rangesize; k++)
         {
            astar_node* node = AStar.Heap[rangestart + k];

            if (node)
               sprintf(rowstring, "%s|%09i|", rowstring, node->combined);
            else
               sprintf(rowstring, "%s|XXXXXXXXX|", rowstring);
         }

         sprintf(rowstring, "%s\n", rowstring);
         fputs(rowstring, fp);

         rangestart = rangestart + rangesize; 
         rangesize *= 2;		
      }
      sprintf(rowstring, "%s\n", rowstring);
      fclose(fp);
   }
   FreeMemory(MALLOC_ID_ROOM, rowstring, 60000);
}

__forceinline void AStarHeapSwap(const unsigned int idx1, const unsigned int idx2)
{
   astar_node* node1 = HEAP(idx1);
   astar_node* node2 = HEAP(idx2);
   HEAP(idx1) = node2;
   HEAP(idx2) = node1;
   node1->heapindex = idx2;
   node2->heapindex = idx1;
}

__forceinline void AStarHeapMoveUp(unsigned int Index)
{
   while (Index > 0)
   {
      // idx of parent
      const unsigned int p = PARENT(Index);

      // stop moving up if parent is smaller
      if (HEAP(Index)->combined >= HEAP(p)->combined)
         return;

      // swap
      AStarHeapSwap(Index, p);
      Index = p;
   }
}

__forceinline void AStarHeapHeapify(unsigned int Index)
{
   // get current heapsize
   const unsigned int len = AStar.HeapSize;

   while (true)
   {
      // left and right child
      const unsigned int lc = LCHILD(Index);
      const unsigned int rc = RCHILD(Index);

      // min of this and both children
      unsigned int min = Index;

      // left is smaller
      if (lc < len && HEAP(lc)->combined < HEAP(min)->combined)
         min = lc;

      // right is (even) smaller
      if (rc < len && HEAP(rc)->combined < HEAP(min)->combined)
         min = rc;

      // none was smaller
      if (min == Index)
         return;

      // swap with smaller
      AStarHeapSwap(Index, min);
      Index = min;
   }
}

__forceinline void AStarHeapInsert(astar_node* Node)
{
   // next index to use
   unsigned int heapSize = AStar.HeapSize;

   // save index of this node
   Node->heapindex = heapSize;

   // add node at the end
   HEAP(Node->heapindex) = Node;

   // increment and update
   heapSize++;
   AStar.HeapSize = heapSize;

   // push node up until in place
   AStarHeapMoveUp(Node->heapindex);
}

__forceinline void AStarHeapRemoveFirst()
{
   // get current size
   unsigned int lastIdx = AStar.HeapSize;

   // decrement
   lastIdx--;

   // write back new size
   AStar.HeapSize = lastIdx;

   // more than 1
   if (lastIdx > 0)
   {
      // put last at root
      AStarHeapSwap(0, lastIdx);

      // reorder tree
      AStarHeapHeapify(0);
   }
}
#pragma endregion

#pragma region ASTAR
__forceinline void AStarZeroMemory1024(__m128i* ptr, const size_t size)
{
   //assert(size % 128 == 0);       // size must be multiple of 128
   //assert((size_t)ptr % 16 == 0); // ptr must have alignment of 16

   // stop when reaching this ptr
   const __m128i* end = (__m128i*)((size_t)ptr + size);

   // clears a chunk of 1024 bit / 128 bytes per iteration (8*16=128)
   // with non-cached moves (MOVNTDQ)
   const __m128i ZERO(_mm_setzero_si128());
   while (ptr < end)
   {
      _mm_stream_si128(ptr, ZERO); ptr++;
      _mm_stream_si128(ptr, ZERO); ptr++;
      _mm_stream_si128(ptr, ZERO); ptr++;
      _mm_stream_si128(ptr, ZERO); ptr++;
      _mm_stream_si128(ptr, ZERO); ptr++;
      _mm_stream_si128(ptr, ZERO); ptr++;
      _mm_stream_si128(ptr, ZERO); ptr++;
      _mm_stream_si128(ptr, ZERO); ptr++;
   }
}

__forceinline void AStarClearMemory()
{
   // clear nodesdata
   const size_t gridrowlen = MAXGRIDCOLS             * sizeof(astar_node_data);
   const size_t maprowlen  = AStar.Room->colshighres * sizeof(astar_node_data);
   const size_t rows       = AStar.Room->rowshighres;

   const size_t t1 = maprowlen / 128;    // how many full multiples of 128
   const size_t t2 = maprowlen % 128;    // one more if there is rest
   size_t size = (t2 > 0) ? t1 + 1 : t1; // if rest add one on full multiples
   size *= 128;                          // convert from multiples of 128 back into bytes

   __m128i* ptr = (__m128i*)&AStar.NodesData[0];
   for (size_t i = 0; i < rows; i++)
   {
      AStarZeroMemory1024(ptr, size);             // clear map row
      ptr = (__m128i*)((size_t)ptr + gridrowlen); // increment by grid row
   }
}

void AStarAddBlockers()
{
   float f1, f2;
   BspLeaf* leaf;

   // 3d start position
   V3 posS;
   posS.X = AStar.StartNode->Location.X;
   posS.Y = AStar.StartNode->Location.Y;
   
   // get height of start
   if (!BSPGetHeight(AStar.Room, &AStar.StartNode->Location, &f1, &posS.Z, &f2, &leaf))
      return;

   // add object height
   posS.Z += OBJECTHEIGHTROO;

   //////////////////////////////////////////////////////////////////////

   BlockerNode* b = AStar.Room->Blocker;
   while (b)
   {
      // don't add self.
      if (b->ObjectID == AStar.ObjectID)
      {
         b = b->Next;
         continue;
      }

      // Get blocker square coords
      const int row = (int)roundf(b->Position.Y * ROOTOGRIDFACT);
      const int col = (int)roundf(b->Position.X * ROOTOGRIDFACT);

      // Don't add blockers at the target coords.
      if (abs(row - AStar.EndNode->Row) < DESTBLOCKIGNORE &&
          abs(col - AStar.EndNode->Col) < DESTBLOCKIGNORE)
      {
         b = b->Next;
         continue;
      }

      // 3d blocker position
      V3 posB;
      posB.X = b->Position.X;
      posB.Y = b->Position.Y;

      // get height of blocker
      if (!BSPGetHeight(AStar.Room, &b->Position, &f1, &posB.Z, &f2, &leaf))
      {
         b = b->Next;
         continue;
      }

      // add object height
      posB.Z += OBJECTHEIGHTROO;

      // skip not visible blockers
      if (!BSPLineOfSightTree(&AStar.Room->TreeNodes[0], &posS, &posB))
      {
         b = b->Next;
         continue;
      }

      // Mark these nodes in A* grid blocked (our coord and +1 highres each dir)
      // Beware: This must be in sync with OBJMINDISTANCE in roofile.h
      for (int rowoffset = -1; rowoffset < 2; rowoffset++)
      {
         const int r = row + rowoffset;

         for (int coloffset = -1; coloffset < 2; coloffset++)
         {
            const int c = col + coloffset;

            // outside
            if (ISOUTSIDEGRID(r, c))
               continue;

            astar_node* node = &AStar.Grid[r][c];
            node->Data->isBlocked = true;
         }
      }
      b = b->Next;
   }
}

__forceinline unsigned int AStarCanWalkEdge(astar_node* Node, astar_node* Neighbour, const unsigned short KnowsVal, const unsigned short CanVal)
{
   // calculate index in edgecache
   const unsigned int idx = Node->Row * AStar.Room->colshighres + Node->Col;

   // get edgecache value
   const unsigned short cval = AStar.Room->EdgesCache[idx];

   // edge is cached
   if (cval & KnowsVal)
      return (cval & CanVal);

   // edge is not yet cached
   Wall* blockWall;

   // bsp query, don't check objects, their squares are already marked blocked
   // and an edge blocked by objects must not be added to the cache
   const bool can = BSPCanMoveInRoom(
      AStar.Room, &Node->Location, &Neighbour->Location, 
      AStar.ObjectID, false, false, true, &blockWall);

   // save answer to cache
   AStar.Room->EdgesCache[idx] |= (can) ? (CanVal | KnowsVal) : KnowsVal;

   // return it
   return (unsigned int)can;
}

__forceinline void AStarProcessNeighbour(astar_node* Node, astar_node* Neighbour, const unsigned int StepCost, const unsigned short KnowsVal, const unsigned short CanVal)
{
   // data of neighbour
   astar_node_data* nd = Neighbour->Data;

   // skip outside of sector, already processed or blocked
   if (nd->isInClosedList | nd->isBlocked)
      return;

   // can't walk edge from node to neighbour
   if (!AStarCanWalkEdge(Node, Neighbour, KnowsVal, CanVal))
      return;

   // CASE 1)
   // we already got a path to this candidate
   if (nd->parent)
   {
      // our cost to the candidate
      const unsigned int newcost = Node->cost + StepCost;

      // we're cheaper, so update the candidate
      // the real cost matters here, not including the heuristic
      if (newcost < Neighbour->cost)
      {
         nd->parent          = Node;
         Neighbour->cost     = newcost;
         Neighbour->combined = newcost + Neighbour->heuristic;

         // reorder it upwards in the heap tree, don't care about downordering
         // since costs are lower and heuristic is always the same,
         // it's guaranteed to be moved up
         AStarHeapMoveUp(Neighbour->heapindex);
      }
   }

   // CASE 2)
   // if this candidate has no parent yet, it was never visited before
   else
   {
      // set node as parent of neighbour
      nd->parent = Node;

      // calculate heuristic for node (~ estimated distance from node to end)
      // need to do this only once for a node
      const unsigned int dx = abs(Neighbour->Col - AStar.EndNode->Col);
      const unsigned int dy = abs(Neighbour->Row - AStar.EndNode->Row);

      // octile-distance
      const unsigned int heuristic = 
         COST * (dx + dy) + (COST_DIAG - 2 * COST) * MIN(dx, dy);

      // cost to candidate is cost to Node + one step
      const unsigned int cost     = Node->cost + StepCost;
      const unsigned int combined = cost + heuristic;

      // update values on neighbour
      Neighbour->heuristic = heuristic;
      Neighbour->cost      = cost;
      Neighbour->combined  = combined;

      // if cost of parent (=heaproot,lowest!) is equal to cost of this neighbour
      // then put the neighbour on the so called faststack instead of openlist. 
      if (combined == Node->combined)
      {
         unsigned int stackSize = AStar.FastStackSize;
         FASTSTACK(stackSize) = Neighbour;
         
         stackSize++;
         AStar.FastStackSize = stackSize;
      }

      // add it sorted to the open list
      else
         AStarHeapInsert(Neighbour);
   }
}

bool AStarProcessFirst()
{
   // All elements on faststack have equal cost that equals lowest costnode at heap root
   // FastStack elements must be processed before openlist elements

   /****************************************************************/

   // next node with lowest combined cost
   // either from faststack or if faststack empty from openlist
   astar_node* node;

   // get and remove from faststack
   if (AStar.FastStackSize > 0)
   {
      AStar.FastStackSize--;
      node = FASTSTACK(AStar.FastStackSize);
      node->Data->isInClosedList = true;
   }

   // get and remove from heap
   else if (AStar.HeapSize > 0)
   {
      node = HEAP(0);
      node->Data->isInClosedList = true;
      AStarHeapRemoveFirst();
   }

   // both empty -> unreachable, no path
   else
      return false;

   /****************************************************************/

   // destination
   const astar_node* endNode = AStar.EndNode;

   // we're close enough, path found
   if (abs(node->Col - endNode->Col) < CLOSEENOUGHDIST &&
       abs(node->Row - endNode->Row) < CLOSEENOUGHDIST)
   {
      // save node as last step
      AStar.LastNode = node;
      return false;
   }

   /****************************************************************/

   int r, c;

   // straight neighbours

   // n
   r = node->Row - 1;
   c = node->Col;
   if (r >= 0)
      AStarProcessNeighbour(node, &AStar.Grid[r][c], COST, EDGECACHE_KNOWS_N, EDGECACHE_CAN_N);

   // e
   r = node->Row;
   c = node->Col + 1;
   if (c < AStar.Room->colshighres)
      AStarProcessNeighbour(node, &AStar.Grid[r][c], COST, EDGECACHE_KNOWS_E, EDGECACHE_CAN_E);
   
   // s
   r = node->Row + 1;
   c = node->Col;
   if (r < AStar.Room->rowshighres)
      AStarProcessNeighbour(node, &AStar.Grid[r][c], COST, EDGECACHE_KNOWS_S, EDGECACHE_CAN_S);

   // w
   r = node->Row;
   c = node->Col - 1;
   if (c >= 0)
      AStarProcessNeighbour(node, &AStar.Grid[r][c], COST, EDGECACHE_KNOWS_W, EDGECACHE_CAN_W);

   // diagonal neighbours

   // ne
   r = node->Row - 1;
   c = node->Col + 1;
   if (!((r < 0) | (c >= AStar.Room->colshighres)))
      AStarProcessNeighbour(node, &AStar.Grid[r][c], COST_DIAG, EDGECACHE_KNOWS_NE, EDGECACHE_CAN_NE);

   // se
   r = node->Row + 1;
   c = node->Col + 1;
   if (!((r >= AStar.Room->rowshighres) | (c >= AStar.Room->colshighres)))
      AStarProcessNeighbour(node, &AStar.Grid[r][c], COST_DIAG, EDGECACHE_KNOWS_SE, EDGECACHE_CAN_SE);

   // sw
   r = node->Row + 1;
   c = node->Col - 1;
   if (!((r >= AStar.Room->rowshighres) | (c < 0)))
      AStarProcessNeighbour(node, &AStar.Grid[r][c], COST_DIAG, EDGECACHE_KNOWS_SW, EDGECACHE_CAN_SW);

   // nw
   r = node->Row - 1;
   c = node->Col - 1;
   if (!((r < 0) | (c < 0)))
      AStarProcessNeighbour(node, &AStar.Grid[r][c], COST_DIAG, EDGECACHE_KNOWS_NW, EDGECACHE_CAN_NW);

   /****************************************************************/

   return true;
}

void AStarWriteGridToFile(room_type* Room)
{
   int rows = Room->rowshighres;
   int cols = Room->colshighres;

   char *rowstring = (char *)AllocateMemory(MALLOC_ID_ROOM, 50000);

   FILE *fp = fopen("griddebug.txt", "w");
   if (fp)
   {
      for (int row = 0; row < rows; row++)
      {
         sprintf(rowstring, "Row %3i- ", row);
         for (int col = 0; col < cols; col++)
         {
            if (&AStar.Grid[row][col] == AStar.StartNode)
            {
               sprintf(rowstring, "%s|SSSSSSSSS|", rowstring);
            }
            else if (&AStar.Grid[row][col] == AStar.EndNode)
            {
               sprintf(rowstring, "%s|EEEEEEEEE|", rowstring);
            }
            else if (AStar.Grid[row][col].Data->isBlocked)
            {
               sprintf(rowstring, "%s|BBBBBBBBB|", rowstring);
            }
            else
            {
               sprintf(rowstring, "%s|%09i|", rowstring, AStar.Grid[row][col].Data->parent ? AStar.Grid[row][col].combined : 0);
            }
         }
         sprintf(rowstring, "%s \n", rowstring);
         fputs(rowstring, fp);
      }
      fclose(fp);
   }

   FreeMemory(MALLOC_ID_ROOM, rowstring, 50000);
}

void AStarClearEdgesCache(room_type* Room)
{
   ZeroMemory(Room->EdgesCache, Room->EdgesCacheSize);
}

void AStarClearPathCaches(room_type* Room)
{
   // clear path-cache
   for (unsigned int i = 0; i < PATHCACHESIZE; i++)
   {
      // get path entry
      astar_path* path = Room->Paths[i];

      // set head to 0 and clear first two nodes
      // care: GetStepFromCache() accesses head+1
      path->head     = 0;
      path->nodes[0] = NULL;
      path->nodes[1] = NULL;
   }

   // clear nopath-cache
   for (unsigned int i = 0; i < NOPATHCACHESIZE; i++)
      Room->NoPaths[i]->tick = 0;

   // reset current cache indices
   Room->NextPathIdx   = 0;
   Room->NextNoPathIdx = 0;
}

void AStarBuildEdgesCache(room_type* Room)
{
   const int rows = Room->rowshighres;
   const int cols = Room->colshighres;

   // iterate all squares
   ::concurrency::parallel_for(size_t(0), (size_t)rows, [&](size_t r)
   {
      for (int c = 0; c < cols; c++)
      {
         // calculate index of node in edgecache
         const unsigned int idx = r * cols + c;

         // node in astar grid
         astar_node* node = &AStar.Grid[r][c];

         Wall* blockWall;
         int nr, nc;
         unsigned int flags = 0;

         // straight neighbours

         // n
         nr = r - 1;
         nc = c;
         if (nr >= 0)
         {
            const bool can = BSPCanMoveInRoom(
               Room, &node->Location, &AStar.Grid[nr][nc].Location,
               0, false, false, true, &blockWall);

            flags |= (can) ? (EDGECACHE_CAN_N | EDGECACHE_KNOWS_N) : EDGECACHE_KNOWS_N;
         }
         // s
         nr = r + 1;
         nc = c;
         if (nr < rows)
         {
            const bool can = BSPCanMoveInRoom(
               Room, &node->Location, &AStar.Grid[nr][nc].Location,
               0, false, false, true, &blockWall);

            flags |= (can) ? (EDGECACHE_CAN_S | EDGECACHE_KNOWS_S) : EDGECACHE_KNOWS_S;
         }
         // e
         nr = r;
         nc = c + 1;
         if (nc < cols)
         {
            const bool can = BSPCanMoveInRoom(
               Room, &node->Location, &AStar.Grid[nr][nc].Location,
               0, false, false, true, &blockWall);

            flags |= (can) ? (EDGECACHE_CAN_E | EDGECACHE_KNOWS_E) : EDGECACHE_KNOWS_E;
         }
         // w
         nr = r;
         nc = c - 1;
         if (nc >= 0)
         {
            const bool can = BSPCanMoveInRoom(
               Room, &node->Location, &AStar.Grid[nr][nc].Location,
               0, false, false, true, &blockWall);

            flags |= (can) ? (EDGECACHE_CAN_W | EDGECACHE_KNOWS_W) : EDGECACHE_KNOWS_W;
         }

         // diagonal

         // ne
         nr = r - 1;
         nc = c + 1;
         if (!((nr < 0) | (nc >= cols)))
         {
            const bool can = BSPCanMoveInRoom(
               Room, &node->Location, &AStar.Grid[nr][nc].Location,
               0, false, false, true, &blockWall);

            flags |= (can) ? (EDGECACHE_CAN_NE | EDGECACHE_KNOWS_NE) : EDGECACHE_KNOWS_NE;
         }
         // se
         nr = r + 1;
         nc = c + 1;
         if (!((nr >= rows) | (nc >= cols)))
         {
            const bool can = BSPCanMoveInRoom(
               Room, &node->Location, &AStar.Grid[nr][nc].Location,
               0, false, false, true, &blockWall);

            flags |= (can) ? (EDGECACHE_CAN_SE | EDGECACHE_KNOWS_SE) : EDGECACHE_KNOWS_SE;
         }
         // sw
         nr = r + 1;
         nc = c - 1;
         if (!((nr >= rows) | (nc < 0)))
         {
            const bool can = BSPCanMoveInRoom(
               Room, &node->Location, &AStar.Grid[nr][nc].Location,
               0, false, false, true, &blockWall);

            flags |= (can) ? (EDGECACHE_CAN_SW | EDGECACHE_KNOWS_SW) : EDGECACHE_KNOWS_SW;
         }
         // nw
         nr = r - 1;
         nc = c - 1;
         if (!((nr < 0) | (nc < 0)))
         {
            const bool can = BSPCanMoveInRoom(
               Room, &node->Location, &AStar.Grid[nr][nc].Location,
               0, false, false, true, &blockWall);

            flags |= (can) ? (EDGECACHE_CAN_NW | EDGECACHE_KNOWS_NW) : EDGECACHE_KNOWS_NW;
         }

         // save flags/edges
         Room->EdgesCache[idx] = (unsigned short)flags;
      }
   });
}

bool AStarGetStepFromCache(room_type* Room, astar_node* S, astar_node* E, V2* P, unsigned int* Flags, int ObjectID)
{
   for (int i = 0; i < PATHCACHESIZE; i++)
   {
      // get path and end-square of cached path
      astar_path* path    = Room->Paths[i];
      astar_node* pathEnd = path->nodes[0];

      // no pathend set
      if (!pathEnd)
         continue;

      // check for match with our requested end (has small tolerance)
      if (abs(pathEnd->Row - E->Row) > PATHCACHETOLERANCE || 
          abs(pathEnd->Col - E->Col) > PATHCACHETOLERANCE)
          continue;

      // get current head (start/laststep)
      unsigned int head = path->head;

      // get the previous, the current, the next and the next-next steps on this path
      astar_node* prev  = (head < MAXPATHLENGTH - 1) ? path->nodes[head + 1] : NULL;
      astar_node* cur   = (head < MAXPATHLENGTH)     ? path->nodes[head]     : NULL;
      astar_node* next  = (head > 0)                 ? path->nodes[head - 1] : NULL;
      astar_node* nnext = (head > 1)                 ? path->nodes[head - 2] : NULL;

      astar_node* stepCurrent = NULL; // might be set with the next step location from candidates above
      astar_node* stepNext    = NULL; // might be set with current-location from candidates above

      // match on cur
      if (cur && cur->Row == S->Row && cur->Col == S->Col)
      {
         // if current matches, next step is next
         stepCurrent = cur;
         stepNext    = next;
      }

      // match on next
      else if (next && next->Row == S->Row && next->Col == S->Col)
      {
         // if next matches, next step is nnext
         stepCurrent = next;
         stepNext    = nnext;
      }

      // match on prev
      else if (prev && prev->Row == S->Row && prev->Col == S->Col)
      {
         // if previous matches, next step is current
         stepCurrent = prev;
         stepNext    = cur;
      }

      // no match
      if (!stepCurrent || !stepNext)
         continue;

      /////////////////////////////////////////////////////////////////////////////////////////////////////////

      // unused
      Wall* blockWall;

      // make sure we can still move to this next endpoint (cares for moved objects!)
      // note: if objects block a cached path, the path will still be walked until the block occurs!
      // to improve this revalidate the whole path from the first to the last node here
      if (!BSPCanMoveInRoom(Room, &stepCurrent->Location, &stepNext->Location, ObjectID, false, true, false, &blockWall))
         continue;

      // adjust head to match on next step
      if (head > 0)
         head--;

      // save new head
      path->head = head;

      // for diagonal moves mark to be long step (required for timer elapse)
      if (abs(stepCurrent->Col - stepNext->Col) &&
          abs(stepCurrent->Row - stepNext->Row))
      {
         *Flags |= ESTATE_LONG_STEP;
      }
      else
         *Flags &= ~ESTATE_LONG_STEP;

      // set step endpoint
      *P = stepNext->Location;

      // cache hit
      return true;
   }
   
   // no cache hit
   return false;
}

bool AStarCheckNoPathCache(const room_type* Room, const astar_node* S, const astar_node* E)
{
   // get current tick in seconds
   const int tick = GetTime();

   // loop nopath cache entries
   for (unsigned int i = 0; i < NOPATHCACHESIZE; i++)
   {
      // path to check
      const astar_nopath* path = Room->NoPaths[i];

      // outdated or invalid entry
      if (tick - path->tick > NOPATHCACHETTL)
         continue;

      // start/end node
      const astar_node* startnode = path->startnode;
      const astar_node* endnode   = path->endnode;

      // invalid entry
      if ((startnode == NULL) | (endnode == NULL))
         continue;

      // check
      if (abs(startnode->Row - S->Row) > NOPATHCACHETOLERANCE || 
          abs(startnode->Col - S->Col) > NOPATHCACHETOLERANCE ||
          abs(endnode->Row - E->Row) > NOPATHCACHETOLERANCE || 
          abs(endnode->Col - E->Col) > NOPATHCACHETOLERANCE)
      {
         continue;
      }
      else
         return true;
   }

   return false;
}

bool AStarGetStepTowards(room_type* Room, V2* S, V2* E, V2* P, unsigned int* Flags, int ObjectID)
{
   // scale to astar grid scale
   V2 sScaled = *S;
   V2SCALE(&sScaled, ROOTOGRIDFACT);

   // round to square
   V2 sScaledRound = sScaled;
   V2ROUND(&sScaledRound);

   // not on astar square location
   // we must step to closest square location first before algorithm can work
   V2 sDelta;
   V2SUB(&sDelta, &sScaledRound, &sScaled);
   if (V2LEN2(&sDelta) > 0.0f)
   {
      V2 sRoo = sScaledRound;
      V2SCALE(&sRoo, GRIDTOROOFACT);

      // check if can step from location to grid square location
      Wall* blockWall;
      if (BSPCanMoveInRoom(Room, S, &sRoo, ObjectID, false, true, false, &blockWall))
      {
         *P = sRoo;
         return true;
      }
      else
         return false;
   }

   /**********************************************************************/

   // scale and round end
   V2 eg = *E;
   V2SCALE(&eg, ROOTOGRIDFACT);
   V2ROUND(&eg);

   // convert coordinates from ROO floatingpoint to
   // highres scale in integers
   const int startrow = (int)sScaledRound.Y;
   const int startcol = (int)sScaledRound.X;
   const int endrow = (int)eg.Y;
   const int endcol = (int)eg.X;

   // all 4 values must be within grid array bounds!
   if ((startrow < 0) | (startrow >= Room->rowshighres) |
       (startcol < 0) | (startcol >= Room->colshighres) |
       (endrow < 0)   | (endrow >= Room->rowshighres)   |
       (endcol < 0)   | (endcol >= Room->colshighres))
         return false;

   /**********************************************************************/

   // set data on astar
   AStar.StartNode     = &AStar.Grid[startrow][startcol];
   AStar.EndNode       = &AStar.Grid[endrow][endcol];
   AStar.Room          = Room;
   AStar.LastNode      = NULL;
   AStar.ObjectID      = ObjectID;
   AStar.FastStackSize = 0;
   AStar.HeapSize      = 0;

   // must also init cost of startnode
   AStar.StartNode->cost      = 0;
   AStar.StartNode->heuristic = 0;
   AStar.StartNode->combined  = 0;

   /**********************************************************************/
#if ASTARDEBUG
   const resource_node* RES = GetResourceByID(AStar.Room->resource_id);
   const char* RESNAME      = RES->resource_name;
   double ts, te, sp1, sp2, sp3;
#endif

   /**********************************************************************/

   // first check nopath cache to avoid expansive unreachable queries
   if (AStarCheckNoPathCache(Room, AStar.StartNode, AStar.EndNode))
      return false;

   // then try path cache lookup
   if (AStarGetStepFromCache(Room, AStar.StartNode, AStar.EndNode, P, Flags, ObjectID))
      return true;

   /**********************************************************************/
#if ASTARDEBUG
   ts = GetMicroCountDouble();
#endif

   // prepare non-persistent astar grid data memory
   AStarClearMemory();

#if ASTARDEBUG
   te = GetMicroCountDouble();
   sp1 = te - ts;
#endif

   /**********************************************************************/
#if ASTARDEBUG
   ts = GetMicroCountDouble();
#endif

   // mark nodes blocked by objects 
   AStarAddBlockers();

#if ASTARDEBUG
   te = GetMicroCountDouble();
   sp2 = te - ts;
#endif

   /**********************************************************************/

   // push startnode on faststack
   FASTSTACK(0) = AStar.StartNode;
   AStar.FastStackSize = 1;

   // count executions on heap root (next node to process)
   unsigned int counter = 0;

#if ASTARDEBUG
   ts = GetMicroCountDouble();
#endif

   // the algorithm finishes if we either hit a node close enough to endnode
   // or if there is no more entries in the open list (unreachable)
   while (AStarProcessFirst())
   {
      counter++;

      // maximum iterations or heapsize reached, abort and assume no path
      // it's +8 because may add up to 8 neighbours in next loop
      if ((counter >= MAXITERATIONS) | (AStar.HeapSize + 8 >= MAXHEAPSIZE))
         break;
   }

#if ASTARDEBUG
   te = GetMicroCountDouble();
   sp3 = te - ts;
#endif

   //if (strcmp(RESNAME, "room_tos") == 0)
   //   AStarWriteGridToFile(Room);
   //AStarWriteHeapToFile(Room);

   /**********************************************************************/

   // unreachable or path too expansive
   if (!AStar.LastNode)
   {
     #if ASTARDEBUG
      dprintf("A*ER O:%05i C:%04.0f B:%04.0f A:%05.0f I:%05i (%s)", 
         AStar.ObjectID, sp1, sp2, sp3, counter, RESNAME);
     #endif

      // check for resetting nextpathidx
      if (Room->NextNoPathIdx >= NOPATHCACHESIZE)
         Room->NextNoPathIdx = 0;

      // get nopath entry from cache and clear it
      astar_nopath* nopath = Room->NoPaths[Room->NextNoPathIdx];
      Room->NextNoPathIdx++;

      // set nopath data
      nopath->startnode = AStar.StartNode;
      nopath->endnode   = AStar.EndNode;
      nopath->tick      = GetTime();
      return false;
   }

   /**********************************************************************/

   // reachable, check for resetting nextpathidx
   if (Room->NextPathIdx >= PATHCACHESIZE)
      Room->NextPathIdx = 0;

   // get path from cache
   astar_path* path = Room->Paths[Room->NextPathIdx];
   Room->NextPathIdx++;

   // reset path head and clear head and head+1
   // care: GetStepFromCache() accesses head+1
   path->head     = 0;
   path->nodes[0] = NULL;
   path->nodes[1] = NULL;

   // walk back parent pointers from lastnode (=path)
   astar_node*  node = AStar.LastNode;
   unsigned int head = 0;

   while (true)
   {
      // above maximum pathlength
      if (head >= MAXPATHLENGTH)
         return false;

      // save step/node
      path->nodes[head] = node;

      // get parent (next step on path)
      node = node->Data->parent;

      // loop again with incremented head or exit loop
      if (node) head++;
      else break;
   }

   // zero out the next unused path-entry
   // care: GetStepFromCache() accesses head+1
   if (head < MAXPATHLENGTH - 1)
      path->nodes[head + 1] = NULL;

   // ignore startnode (will be at head)
   if (head > 0)
      head--;

   // get next step node
   node = path->nodes[head];

   // remove next step node
   if (head > 0)
      head--;

   // save new head
   path->head = head;

   /**********************************************************************/

   // invalid node or node not neighbour of startnode
   if (!node || node->Data->parent != AStar.StartNode)
   {
     #if ASTARDEBUG
      dprintf("A*ER O:%05i NODE NULL OR NOT NEIGHBOUR OF START (%s)",
         AStar.ObjectID, RESNAME);
     #endif
      return false;
   }

   // for diagonal moves mark to be long step (required for timer elapse)
   if ((node->Col - AStar.StartNode->Col) != 0 &&
       (node->Row - AStar.StartNode->Row) != 0)
   {
      *Flags |= ESTATE_LONG_STEP;
   }
   else
      *Flags &= ~ESTATE_LONG_STEP;

   // set step endpoint
   *P = node->Location;

#if ASTARDEBUG
   dprintf("A*OK O:%05i C:%04.0f B:%04.0f A:%05.0f I:%05i S:%04i (%s)", 
      AStar.ObjectID, sp1, sp2, sp3, counter, head, RESNAME);
#endif

   return true;
}

void AStarInit()
{
   AStar.StartNode = NULL;
   AStar.EndNode   = NULL;
   AStar.LastNode  = NULL;
   AStar.ObjectID  = 0;
   AStar.HeapSize  = 0;
   AStar.Room      = NULL;

   // setup all squares
   for (int i = 0; i < MAXGRIDROWS; i++)
   {
      for (int j = 0; j < MAXGRIDCOLS; j++)
      {
         astar_node* node = &AStar.Grid[i][j];

         // set row/col
         node->Row = i;
         node->Col = j;

         // set ptr to data-node
         const int idx = i*MAXGRIDCOLS + j;
         node->Data = &AStar.NodesData[idx];

         // floatingpoint coordinates of the center of the square in ROO fineness (for queries)
         node->Location.X = j * GRIDTOROOFACT;// +128.0f;
         node->Location.Y = i * GRIDTOROOFACT;// +128.0f;
      }
   }
}

void AStarShutdown()
{
}
#pragma endregion
