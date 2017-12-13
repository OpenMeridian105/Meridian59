// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
* async.c
*

  This module contains functions to handle asynchronous socket events
  (i.e. new connections, reading/writing, and closing.
  
	Every function here is called from the interface thread!
	
*/

#include "blakserv.h"

static HANDLE name_lookup_handle;

static char   udpbuf[BUFFER_SIZE + HEADERBYTES]; // same size as bufpool buffers!
static SOCKET udpsock = INVALID_SOCKET;
static int last_udp_read_time = 0; // track this for errors.

/* local function prototypes */
void AcceptSocketConnections(int socket_port,int connection_type);
void AcceptUDP(int socket_port);
void AsyncEachSessionNameLookup(session_node *s);

void ResetUDP(void)
{
   // Potential threading issues?
   closesocket(udpsock);
   // WSACleanup(); Not sure if we need to do this
   AcceptUDP(ConfigInt(SOCKET_PORT));
}

int GetLastUDPReadTime(void)
{
   return last_udp_read_time;
}

void AsyncSocketStart(void)
{
	AcceptSocketConnections(ConfigInt(SOCKET_PORT),SOCKET_PORT);
	AcceptSocketConnections(ConfigInt(SOCKET_MAINTENANCE_PORT),SOCKET_MAINTENANCE_PORT);

   // accept udp datagrams on same port
   AcceptUDP(ConfigInt(SOCKET_PORT));
}

/* connection_type is either SOCKET_PORT or SOCKET_MAINTENANCE_PORT, so we
keep track of what state to send clients into. */
void AcceptSocketConnections(int socket_port,int connection_type)
{
	SOCKET sock;
	SOCKADDR_IN6 sin;
	struct linger xlinger;
	int xxx;
	
	sock = socket(AF_INET6,SOCK_STREAM,0);
	if (sock == INVALID_SOCKET) 
	{
		eprintf("AcceptSocketConnections socket() failed WinSock code %i\n",
			GetLastError());
		closesocket(sock);
		return;
	}
	
	/* Make sure this is a IPv4/IPv6 dual stack enabled socket */
	
	xxx = 0;
	if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&xxx, sizeof(xxx)) < 0)
	{
		eprintf("AcceptSocketConnections error setting sock opts: IPV6_V6ONLY\n");
		return;
	}

	/* Set a couple socket options for niceness */
	
	xlinger.l_onoff=0;
	if (setsockopt(sock,SOL_SOCKET,SO_LINGER,(char *)&xlinger,sizeof(xlinger)) < 0)
	{
		eprintf("AcceptSocketConnections error setting sock opts: SO_LINGER\n");
		return;
	}
	
	xxx=1;
	if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(char *)&xxx,sizeof xxx) < 0)
	{
		eprintf("AcceptSocketConnections error setting sock opts: SO_REUSEADDR\n");
		return;
	}
	
	if (!ConfigBool(SOCKET_NAGLE))
	{
		/* turn off Nagle algorithm--improve latency? */
		xxx = true;
		if (setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,(char *)&xxx,sizeof xxx))
		{
			eprintf("AcceptSocketConnections error setting sock opts: TCP_NODELAY\n");
			return;
		}
	}
	
	memset(&sin, 0, sizeof(sin));
	sin.sin6_family = AF_INET6;
	sin.sin6_addr = in6addr_any;
	sin.sin6_flowinfo = 0;
	sin.sin6_scope_id = 0;
	sin.sin6_port = htons((short)socket_port);
	
	if (bind(sock,(struct sockaddr *) &sin,sizeof(sin)) == SOCKET_ERROR) 
	{
		eprintf("AcceptSocketConnections bind failed, WinSock error %i\n",
			GetLastError());
		closesocket(sock);
		return;
	}	  
	
	if (listen(sock,5) < 0) /* backlog of 5 connects by OS */
	{
		eprintf("AcceptSocketConnections listen failed, WinSock error %i\n",
			GetLastError());
		closesocket(sock);
		return;
	}
	
	StartAsyncSocketAccept(sock,connection_type);
	/* when we get a connection, it'll call AsyncSocketAccept */
}

void AcceptUDP(int socket_port)
{
   SOCKADDR_IN6 addr;

   // listen on all interfaces
   memset(&addr, 0, sizeof(addr));
   addr.sin6_family = AF_INET6;
   addr.sin6_addr = in6addr_any;
   addr.sin6_flowinfo = 0;
   addr.sin6_scope_id = 0;
   addr.sin6_port = htons((short)socket_port);

   // try to create IPV6 UDP socket
   udpsock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
   if (udpsock == INVALID_SOCKET)
   {
      int error = WSAGetLastError();
      eprintf("AcceptUDP error creating udp socket - %i \n", error);
      return;
   }

   // set IPv6 DualStack to also support IPv4
   int yesno = 0;
   if (setsockopt(udpsock, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&yesno, sizeof(yesno)) < 0)
   {
      int error = WSAGetLastError();
      eprintf("AcceptUDP error setting sock opts: IPV6_V6ONLY - %i \n", error);
      return;
   }

   // bind socket
   int rc = bind(udpsock, (SOCKADDR*)&addr, sizeof(addr));

   if (rc == SOCKET_ERROR)
   {
      int error = WSAGetLastError();
      eprintf("AcceptUDP error binding socket - %i \n", error);
      return;
   }

   StartAsyncSocketUDPRead(udpsock);
   /* when we get a udp datagram, it'll call AsyncSocketReadUDP */
}

void AsyncNameLookup(HANDLE hLookup,int error)
{
   if (error != 0)
   {
      /* eprintf("AsyncSocketNameLookup got error %i\n",error); */
      return;
   }
   
   name_lookup_handle = hLookup;
   
   EnterServerLock();
   ForEachSession(AsyncEachSessionNameLookup);
   LeaveServerLock();
   
}

void AsyncEachSessionNameLookup(session_node *s)
{
	if (s->conn.type != CONN_SOCKET)
		return;
	
	if (s->conn.hLookup == name_lookup_handle)
	{
		sprintf(s->conn.name,"%s",((struct hostent *)&(s->conn.peer_data))->h_name);
		InterfaceUpdateSession(s);
	}      
}

void AsyncSocketClose(SOCKET sock)
{
	session_node *s;
	
	s = GetSessionBySocket(sock);
	if (s == NULL)
		return;
	
	/* dprintf("async socket close %i\n",s->session_id); */
	HangupSession(s);
	
}

void AsyncSocketWrite(SOCKET sock)
{
   int bytes;  
   session_node *s;
   buffer_node *bn;

   s = GetSessionBySocket(sock);
   if (s == NULL)
      return;

   if (s->hangup)
      return;

   /* dprintf("got async write session %i\n",s->session_id); */
   if (!MutexAcquireWithTimeout(s->muxSend,10000))
   {
      eprintf("AsyncSocketWrite couldn't get session %i muxSend\n",s->session_id);
      return;
	}

   while (s->send_list != NULL)
   {
      bn = s->send_list;
      /* dprintf("async writing %i\n",bn->len_buf); */
      bytes = send(s->conn.socket,bn->buf,bn->len_buf,0);
      if (bytes == SOCKET_ERROR)
      {
         if (GetLastError() != WSAEWOULDBLOCK)
         {
            /* eprintf("AsyncSocketWrite got send error %i\n",GetLastError()); */
            if (!MutexRelease(s->muxSend))
               eprintf("File %s line %i release of non-owned mutex\n",__FILE__,__LINE__);

            HangupSession(s);
            return;
         }
			
         /* dprintf("got write event, but send would block\n"); */
         break;
      }
      else
      {
         if (bytes != bn->len_buf)
            dprintf("async write wrote %i/%i bytes\n",bytes,bn->len_buf);
			
         transmitted_bytes += bn->len_buf;
			
         s->send_list = bn->next;
         DeleteBuffer(bn);
      }
   }
   if (!MutexRelease(s->muxSend))
      eprintf("File %s line %i release of non-owned mutex\n",__FILE__,__LINE__);
}

void AsyncSocketRead(SOCKET sock)
{
   int bytes;
   session_node *s;
   buffer_node *bn;

   s = GetSessionBySocket(sock);
   if (s == NULL)
      return;

   if (s->hangup)
      return;

   if (!MutexAcquireWithTimeout(s->muxReceive,10000))
   {
      eprintf("AsyncSocketRead couldn't get session %i muxReceive",s->session_id);
      return;
   }

   if (s->receive_list == NULL)
   {
      s->receive_list = GetBuffer();
      /* dprintf("Read0x%08x\n",s->receive_list); */
   }
	
   // find the last buffer in the receive list
   bn = s->receive_list;
   while (bn->next != NULL)
      bn = bn->next;
	
   // if that buffer is filled to capacity already, get another and append it
   if (bn->len_buf >= BUFFER_SIZE_TCP_NOHEADER)
   {
      bn->next = GetBuffer();
      /* dprintf("ReadM0x%08x\n",bn->next); */
      bn = bn->next;
   }
	
   // read from the socket, up to the remaining capacity of this buffer
   bytes = recv(s->conn.socket,bn->buf + bn->len_buf, BUFFER_SIZE_TCP_NOHEADER - bn->len_buf,0);
   if (bytes == SOCKET_ERROR)
   {
      if (GetLastError() != WSAEWOULDBLOCK)
      {
         /* eprintf("AsyncSocketRead got read error %i\n",GetLastError()); */
         if (!MutexRelease(s->muxReceive))
            eprintf("File %s line %i release of non-owned mutex\n",__FILE__,__LINE__);

         HangupSession(s);
         return;
      }
      if (!MutexRelease(s->muxReceive))
         eprintf("File %s line %i release of non-owned mutex\n",__FILE__,__LINE__);
   }

   if (bytes == 0)
   {
      if (!MutexRelease(s->muxReceive))
         eprintf("File %s line %i release of non-owned mutex\n",__FILE__,__LINE__);

      HangupSession(s);

      return;
   } 

   if (bytes < 0 || bytes > BUFFER_SIZE_TCP_NOHEADER - bn->len_buf)
   {
      eprintf("AsyncSocketRead got %i bytes from recv() when asked to stop at %i\n",bytes, BUFFER_SIZE_TCP_NOHEADER - bn->len_buf);
      FlushDefaultChannels();
      bytes = 0;
   }

   bn->len_buf += bytes;
	
   if (!MutexRelease(s->muxReceive))
      eprintf("File %s line %i release of non-owned mutex\n",__FILE__,__LINE__);  
	
	SignalSession(s->session_id);
}

void AsyncSocketReadUDP(SOCKET sock)
{
   /************* THIS IS EXECUTED BY THE UI/NETWORK THREAD *************/

   SOCKADDR_IN6 senderaddr;
   int          bytesReceivd = 0;
   int          flags = 0;
   int          lplen = sizeof(senderaddr);

   // Record time.
   last_udp_read_time = GetTime();

   ///////////////////////////////////////////////////////////////////////
   // try to receive the new udp datagram from socket
   ///////////////////////////////////////////////////////////////////////
   
   bytesReceivd = recvfrom(sock, (char*)&udpbuf, sizeof(udpbuf), flags,
      (SOCKADDR*)&senderaddr, &lplen);

   if (bytesReceivd == SOCKET_ERROR)
   {
      eprintf("AsyncSocketReadUDP error receiving UDP from socket - %i \n", 
         WSAGetLastError());
      return;
   }

   ///////////////////////////////////////////////////////////////////////
   // checks #1
   ///////////////////////////////////////////////////////////////////////

   // 1) not at least full header with byte, discard it
   if (bytesReceivd < SIZE_HEADER_UDP + 1)
   {
      eprintf("AsyncSocketReadUDP error udp with size below headersize \n");
      return;
   }

   // 2) todo?: check blacklisted ip by comparing senderaddr

   ///////////////////////////////////////////////////////////////////////
   // parse header and type
   ///////////////////////////////////////////////////////////////////////

   int sessionid      = *((int*)&udpbuf[0]);
   unsigned int seqno = *((unsigned int*)&udpbuf[4]);
   short crc          = *((short*)&udpbuf[8]);
   char epoch         = udpbuf[10];
   char type          = udpbuf[11];
   
   // try to get that session
   session_node* session = GetSessionByID(sessionid);

   ///////////////////////////////////////////////////////////////////////
   // checks #2
   ///////////////////////////////////////////////////////////////////////

   // Print errors/info?
   bool debug_udp = ConfigBool(DEBUG_UDP);

   // 1) invalid session or hangup
   if (!session || session->hangup)
   {
      if (debug_udp)
         dprintf("AsyncSocketReadUDP error unknown session-Id or hangup session \n");
      return;
   }

   // 2) important: udp sender ip must match tcp session ip to prevent attacks!
   for (unsigned int i = 0; i < 8; i++)
   {
      if (session->conn.addr.u.Word[i] != senderaddr.sin6_addr.u.Word[i])
      {
         if (debug_udp)
            eprintf("AsyncSocketReadUDP warning received session-Id from different IP\n");
         return;
      }
   }

   // 3) validate crc (calculated over type + data)
   short validcrc = (short)GetCRC16(udpbuf + SIZE_HEADER_UDP, bytesReceivd - SIZE_HEADER_UDP);
   if (crc != validcrc)
   {
      eprintf("AsyncSocketReadUDP error crc mismatch \n");
      return;
   }

   // 4) out of sequence order or duplicated UDP datagram
   if (seqno <= session->receive_seqno_udp)
   {
      if (debug_udp)
         dprintf("AsyncSocketReadUDP discarding out of sequence UDP on Session %i \n", session->session_id);
      return;
   }
   else
   {
      // Don't print, happens a bit too often and as a result of normal packet loss.
      // log missed or malformed UDP
      // if (seqno > session->receive_seqno_udp + 1)
      //    dprintf("AsyncSocketReadUDP detected lost or malformed UDP on Session %i", session->session_id);

      // update seqno
      session->receive_seqno_udp = seqno;
   }

   // 4) epoch: see GameProcessSessionBufferUDP()

   ///////////////////////////////////////////////////////////////////////
   // debug
   ///////////////////////////////////////////////////////////////////////

   //dprintf("Received valid UDP Session: %i Type: %i \n",
   //   sessionid, type);

   ///////////////////////////////////////////////////////////////////////
   // save data on session struct
   ///////////////////////////////////////////////////////////////////////

   // lock
   if (!MutexAcquireWithTimeout(session->muxReceive, 10000))
   {
      eprintf("AsyncSocketReadUDP couldn't get session %i muxReceive", 
         session->session_id);
      return;
   }

   // create first udp datagram buffer if none yet
   if (session->receive_list_udp == NULL)
      session->receive_list_udp = GetBuffer();

   // find the last buffer in the udp receive list
   buffer_node* bn = session->receive_list_udp;
   while (bn->next != NULL)
      bn = bn->next;

   // copy udp datagram into udp buffer list of session
   // note: this is simply 1 UDP datagram per buffer !
   bn->len_buf = bytesReceivd;
   memcpy(bn->prebuf, udpbuf, bytesReceivd);

   // unlock
   if (!MutexRelease(session->muxReceive))
      eprintf("File %s line %i release of non-owned mutex\n", __FILE__, __LINE__);

   ///////////////////////////////////////////////////////////////////////
   // signal mainthread (blakserv) to process data (thread transition here)
   ///////////////////////////////////////////////////////////////////////

   if (debug_udp)
      dprintf("Signalling main thread with UDP read for session %i\n", session->session_id);

   SignalSession(session->session_id);
}
