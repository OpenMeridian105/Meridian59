// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
* memory.c
*

  This module keeps track of memory usage by most of the system.
  
*/

#include "blakserv.h"

memory_statistics memory_stat;

const char *memory_stat_names[] = 
{
	"Timer", "String", "Kodbase", "Resource", 
		"Session", "Account", "User", "Motd",
		"Dllist", "LoadBof",
		"Systimer", "Nameid",
		"Class", "Message", "Object",
		"List", "Object properties",
		"Configuration", "Rooms", "Astar",
		"Admin constants", "Buffers", "Game loading",
		"Tables", "Socket blocks", "Game saving", "SQL",
		
		NULL
};

/* local function prototypes */


void InitMemory(void)
{
	int i;
	
	/* verify that memory_stat_names has a name for every malloc_id */
	i = 0;
	while (memory_stat_names[i] != NULL)
		i++;
	
	if (i != MALLOC_ID_NUM)
		StartupPrintf("InitMemory FATAL there aren't names for every malloc id\n");
	
	for (i=0;i<MALLOC_ID_NUM;i++)
		memory_stat.allocated[i] = 0;
}

memory_statistics * GetMemoryStats(void)
{
	return &memory_stat;
}

int GetMemoryTotal(void)
{
	int total = 0;
	
	for (int i = 0; i < MALLOC_ID_NUM; ++i)
		total += memory_stat.allocated[i];
	
	return total;
}

int GetNumMemoryStats(void)
{
	return MALLOC_ID_NUM;
}

const char * GetMemoryStatName(int malloc_id)
{
	return memory_stat_names[malloc_id];
}

void * AllocateMemoryDebug(int malloc_id,int size,const char *filename,int linenumber)
{
	void *ptr;
	
	if (size == 0)
	{
		eprintf("AllocateMemoryDebug zero byte memory block from %s at %d\n",filename,linenumber);
	}

	if (malloc_id < 0 || malloc_id >= MALLOC_ID_NUM)
		eprintf("AllocateMemory allocating memory of unknown type %i\n",malloc_id);
	else
		memory_stat.allocated[malloc_id] += size;

	ptr = malloc( size );

	if (ptr == NULL)
	{
	/* assume channels started up if allocation error, which might not be true,
		but if so, then there are more serious problems! */
		eprintf("AllocateMemory couldn't allocate %i bytes (id %i)\n",size,malloc_id);
		FatalError("Memory allocation failure");
	}
	/*if (InMainLoop())
	{
		dprintf("M0x%08x %i %i\n",ptr,malloc_id,size);
	}*/
	return ptr;
}

// Same as AllocateMemoryDebug, except calls calloc() for use in arrays. Faster than calling
// malloc and setting the array to NULL.
void * AllocateMemoryCallocDebug(int malloc_id, int count, int size, const char *filename, int linenumber)
{
   void *ptr;

   if (size == 0 || count == 0)
   {
      eprintf("AllocateMemoryCallocDebug zero byte memory block from %s at %d\n",
         filename, linenumber);
   }

   if (malloc_id < 0 || malloc_id >= MALLOC_ID_NUM)
      eprintf("AllocateMemoryCallocDebug allocating memory of unknown type %i\n", malloc_id);
   else
      memory_stat.allocated[malloc_id] += (count * size);

   ptr = calloc(count, size);

   if (ptr == NULL)
   {
      /* assume channels started up if allocation error, which might not be true,
      but if so, then there are more serious problems! */
      eprintf("AllocateMemoryCallocDebug couldn't allocate %i bytes (id %i)\n",
         size, malloc_id);
      FatalError("Memory allocation failure");
   }

   return ptr;
}

void FreeMemoryX(int malloc_id,void **ptr,int size)
{
	/*if (InMainLoop())
	{
		dprintf("F0x%08x %i %i\n",ptr,malloc_id,size);
	}*/

	if (malloc_id < 0 || malloc_id >= MALLOC_ID_NUM)
		eprintf("FreeMemory freeing memory of unknown type %i\n",malloc_id);
	else
		memory_stat.allocated[malloc_id] -= size;
	
	free( *ptr );
	
	/* we want to catch any references to this, after the free()  */
	*ptr = (void*)0xDEADC0DE ;
	
}

void * ResizeMemory(int malloc_id,void *ptr,int old_size,int new_size)
{
	void* new_mem;

	/*if (InMainLoop())
	{
		dprintf("R0x%08x %i %i %i\n",ptr,malloc_id,old_size,new_size);
	}*/
	if (malloc_id < 0 || malloc_id >= MALLOC_ID_NUM)
		eprintf("ResizeMemory resizing memory of unknown type %i\n",malloc_id);
	else
		memory_stat.allocated[malloc_id] += new_size-old_size;

	new_mem = realloc(ptr,new_size);

	if (new_mem == NULL)
	{		
		eprintf("ResizeMemory couldn't reallocate from %i to %i bytes (id %i)\n",old_size,new_size,malloc_id);
		FatalError("Memory reallocation failure");
	}

	return new_mem;
}

void * AllocateMemorySIMD(int malloc_id, int size)
{
#if defined(SSE2) || defined(SSE4)
   void *ptr;

   if (size == 0)
      eprintf("AllocateMemorySIMD zero byte memory block from %s at %d\n", __FILE__, __LINE__);

   if (malloc_id < 0 || malloc_id >= MALLOC_ID_NUM)
      eprintf("AllocateMemorySIMD allocating memory of unknown type %i\n", malloc_id);
   
   else
      memory_stat.allocated[malloc_id] += size;

   ptr = _aligned_malloc(size, 16);

   if (ptr == NULL)
   {
      /* assume channels started up if allocation error, which might not be true,
      but if so, then there are more serious problems! */
      eprintf("AllocateMemorySIMD couldn't allocate %i bytes (id %i)\n", size, malloc_id);
      FatalError("Memory allocation failure");
   }

   return ptr;
#else
   // use default if SSE is disabled
   return AllocateMemory(malloc_id, size);
#endif
}

void FreeMemorySIMD(int malloc_id, void *ptr, int size)
{
#if defined(SSE2) || defined(SSE4)
   if (malloc_id < 0 || malloc_id >= MALLOC_ID_NUM)
      eprintf("FreeMemorySIMD freeing memory of unknown type %i\n", malloc_id);
   else
      memory_stat.allocated[malloc_id] -= size;

   _aligned_free(ptr);
#else
   // use default if SSE is disabled
   FreeMemory(malloc_id, ptr, size);
#endif
}
