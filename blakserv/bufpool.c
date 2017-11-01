// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * bufpool.c
 *

 This file provides a pool of memory buffers to be used for reading data from
 clients and sending data to them.

 The main thread typically calls GetBuffer() and the interface/socket thread
 calls DeleteBuffer(), so we need a cs.

 */

#include "blakserv.h"

// stores free/available buffers
buffer_node *buffers;
int next_buffer_id;

CRITICAL_SECTION csBuffers; /* protects our buffers list */

void InitBufferPool(void)
{
   buffers = NULL;
   next_buffer_id = 1;
   InitializeCriticalSection(&csBuffers);
}

/* this frees buffers we have sitting around, but ones in action are still
   out there */
void ResetBufferPool(void)
{
   buffer_node *bn,*temp;

   EnterCriticalSection(&csBuffers);
   /* dprintf("ResetBufferPool begin\n"); */

   /* test out debug junk: buffers->buf[BUFFER_SIZE] = 12; */
   DebugCheckHeap();

   bn = buffers;
   while (bn != NULL)
   {
      temp = bn->next;
      FreeMemory(MALLOC_ID_BUFFER,bn,sizeof(buffer_node));
      bn = temp;
   }
   buffers = NULL;
   /* dprintf("ResetBufferPool end\n"); */
   LeaveCriticalSection(&csBuffers);
}

buffer_node * GetBuffer(void)
{
   buffer_node *bn;

   EnterCriticalSection(&csBuffers);
   if (buffers == NULL)
   {
      bn = (buffer_node *) AllocateMemory(MALLOC_ID_BUFFER,sizeof(buffer_node));
      bn->len_buf = 0;
      bn->buf = bn->prebuf + HEADERBYTES;
      bn->buffer_id = next_buffer_id++;
      bn->next = NULL;
   }
   else
   {
      bn = buffers;
      buffers = buffers->next;
      bn->next = NULL;
      bn->len_buf = 0;
      bn->buf = bn->prebuf + HEADERBYTES;
      bn->buffer_id = next_buffer_id++;
   }
   LeaveCriticalSection(&csBuffers);

   return bn;
}

void DeleteBuffer(buffer_node *bn)
{
   EnterCriticalSection(&csBuffers);

   // add the buffer to the beginning
   // of our free/unused buffers list
   bn->next = buffers;
   buffers = bn;

   LeaveCriticalSection(&csBuffers);
}

void DeleteBufferList(buffer_node *blist)
{
   EnterCriticalSection(&csBuffers);

   // add all buffers from blist to the
   // free/unused buffers list
   while (blist != NULL)
   {
      buffer_node* bn = blist->next;

      // add the buffer to the beginning
      // of our free/unused buffers list
      blist->next = buffers;
      buffers = blist;
      
      blist = bn;
   }
   LeaveCriticalSection(&csBuffers);
}

/* adds a block of bytes to a buffer list, potentially adding more buffers to
   the end of the list as need be */
buffer_node * AddToBufferList(buffer_node *blist,void *buf,int len_buf)
{
   buffer_node *bn;
   int copy_bytes,index; /* index into buf we were sent */

   if (len_buf == 0)
      return blist;

   if (blist == NULL)
      blist = GetBuffer();

   bn = blist;

   /* get to last node on the list, and add there */
   while (bn->next != NULL)
      bn = bn->next;

   index = 0;
   for(;;)
   {
     copy_bytes = std::min(BUFFER_SIZE_TCP_NOHEADER - bn->len_buf, len_buf - index);
      memcpy(bn->buf + bn->len_buf, (char *)buf + index, copy_bytes);
      index += copy_bytes;
      bn->len_buf += copy_bytes;

      if (index == len_buf)
			break;

		//dprintf("AddToBufferList had to create a second buffer");
      bn->next = GetBuffer();
      bn = bn->next;
   }

   return blist;
}

buffer_node * AddByteToBufferList(buffer_node *blist,char ch)
{
   char b;
   b = ch;
   return AddToBufferList(blist,&b,1);
}

buffer_node * CopyBufferList(buffer_node *blist)
{
   buffer_node *new_list,*bn;

   if (blist == NULL)
      return NULL;

   new_list = GetBuffer();
   bn = new_list;
   while (blist != NULL)
   {
      memcpy(bn->buf,blist->buf,blist->len_buf);
      bn->len_buf = blist->len_buf;

      blist = blist->next;
      if (blist != NULL)
      {
	 bn->next = GetBuffer();
	 bn = bn->next;
      }
   }
   return new_list;   
}
