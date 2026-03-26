// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.
/*
 * osd_epoll.c
 *
 * Linux networking and main loop based on epoll().
 * Based on vanilla Meridian59, extended with UDP support.
 */

#include "blakserv.h"
#include <sys/epoll.h>
#include <sys/eventfd.h>

int fd_epoll;
static int fd_wakeup = -1;  // eventfd for timer wakeup

#define MAX_MAINTENANCE_MASKS 15
static char *maintenance_masks[MAX_MAINTENANCE_MASKS];
static int num_maintenance_masks = 0;
static char *maintenance_buffer = NULL;

Bool CheckMaintenanceMask(SOCKADDR_IN6 *addr, int len_addr);

static INT64 GetMainLoopWaitTime(void)
{
   INT64 ms;
   int numActive = GetNumActiveTimers();

   if (numActive == 0)
      ms = 500;
   else
   {
      ms = GetNextTimerTime() - GetMilliCount();
      if (ms <= 0)
         ms = 0;
      if (ms > 500)
         ms = 500;
   }
   return ms;
}

void RunMainLoop(void)
{
   INT64 ms;
   const uint32_t num_notify_events = 500;
   struct epoll_event notify_events[num_notify_events];
   int i;

   signal(SIGPIPE, SIG_IGN);
   signal(SIGTERM, [](int) { SetQuit(); });
   signal(SIGINT, [](int) { SetQuit(); });

   dprintf("RunMainLoop started on epoll fd %d\n", fd_epoll);

   while (!GetQuit())
   {
      ms = GetMainLoopWaitTime();

      int val = epoll_wait(fd_epoll, notify_events, num_notify_events, ms);
      if (val == -1)
      {
         if (errno != EINTR)
            eprintf("RunMainLoop error on epoll_wait %s\n", GetLastErrorStr());
         continue;
      }

      for (i = 0; i < val; i++)
      {
         if (notify_events[i].events == 0)
            continue;

         // Timer wakeup eventfd - just drain it and let TimerActivate run
         if (notify_events[i].data.fd == fd_wakeup)
         {
            uint64_t val;
            read(fd_wakeup, &val, sizeof(val));
            continue;
         }

         // UDP socket handling
         if (notify_events[i].data.fd == GetUDPSocket())
         {
            if (notify_events[i].events & EPOLLIN)
               AsyncSocketReadUDP(notify_events[i].data.fd);
            continue;
         }

         if (IsAcceptingSocket(notify_events[i].data.fd))
         {
            if (notify_events[i].events & ~EPOLLIN)
            {
               eprintf("RunMainLoop error on accepting socket %i\n",
                  notify_events[i].data.fd);
            }
            else
            {
               AsyncSocketAccept(notify_events[i].data.fd, FD_ACCEPT, 0,
                  GetAcceptingSocketConnectionType(notify_events[i].data.fd));
            }
         }
         else
         {
            if (notify_events[i].events & ~(EPOLLIN | EPOLLOUT))
            {
               AsyncSocketSelect(notify_events[i].data.fd, 0, 1);
            }
            else
            {
               if (notify_events[i].events & EPOLLIN)
                  AsyncSocketSelect(notify_events[i].data.fd, FD_READ, 0);
               if (notify_events[i].events & EPOLLOUT)
                  AsyncSocketSelect(notify_events[i].data.fd, FD_WRITE, 0);
            }
         }
      }

      EnterServerLock();
      PollSessions();
      TimerActivate();
      LeaveServerLock();
   }

   close(fd_epoll);
}

void StartupComplete(void)
{
   fd_epoll = epoll_create(1);

   // Create eventfd for timer wakeup - allows TimerAddNode to
   // wake the epoll loop immediately when a new earliest timer is added.
   fd_wakeup = eventfd(0, EFD_NONBLOCK);
   if (fd_wakeup >= 0)
   {
      struct epoll_event ee;
      ee.events = EPOLLIN;
      ee.data.fd = fd_wakeup;
      if (epoll_ctl(fd_epoll, EPOLL_CTL_ADD, fd_wakeup, &ee) != 0)
         eprintf("StartupComplete error adding wakeup eventfd: %s\n", GetLastErrorStr());
      else
         dprintf("Timer wakeup eventfd initialized (fd=%d)\n", fd_wakeup);
   }
   else
      eprintf("StartupComplete error creating wakeup eventfd: %s\n", GetLastErrorStr());
}

void WakeupMainLoop(void)
{
   if (fd_wakeup >= 0)
   {
      uint64_t val = 1;
      write(fd_wakeup, &val, sizeof(val));
   }
}

void StartAsyncSocketAccept(SOCKET sock, int connection_type)
{
   struct epoll_event ee;
   ee.events = EPOLLIN;
   ee.data.fd = sock;
   if (epoll_ctl(fd_epoll, EPOLL_CTL_ADD, sock, &ee) != 0)
   {
      eprintf("StartAsyncSocketAccept error adding socket %s\n", GetLastErrorStr());
      return;
   }
   AddAcceptingSocket(sock, connection_type);
}

void StartAsyncSession(session_node *s)
{
   struct epoll_event ee;
   ee.events = EPOLLIN;
   ee.data.fd = s->conn.socket;
   if (epoll_ctl(fd_epoll, EPOLL_CTL_ADD, s->conn.socket, &ee) != 0)
   {
      eprintf("StartAsyncSession error adding socket %s\n", GetLastErrorStr());
      return;
   }
}

void EnableSendEvents(SOCKET sock)
{
   struct epoll_event ee;
   ee.events = EPOLLIN | EPOLLOUT;
   ee.data.fd = sock;
   epoll_ctl(fd_epoll, EPOLL_CTL_MOD, sock, &ee);
}

void DisableSendEvents(SOCKET sock)
{
   struct epoll_event ee;
   ee.events = EPOLLIN;
   ee.data.fd = sock;
   epoll_ctl(fd_epoll, EPOLL_CTL_MOD, sock, &ee);
}

void StartAsyncSocketUDPRead(SOCKET sock)
{
   struct epoll_event ee;
   ee.events = EPOLLIN;
   ee.data.fd = sock;
   if (epoll_ctl(fd_epoll, EPOLL_CTL_ADD, sock, &ee) != 0)
   {
      eprintf("StartAsyncSocketUDPRead error adding UDP socket %s\n", GetLastErrorStr());
      return;
   }
   SetUDPSocket(sock);
}

void ExitAsyncConnections(void)
{
}

void InitAsyncConnections(void)
{
   maintenance_buffer = (char *)malloc(strlen(ConfigStr(SOCKET_MAINTENANCE_MASK)) + 1);
   strcpy(maintenance_buffer, ConfigStr(SOCKET_MAINTENANCE_MASK));

   maintenance_masks[num_maintenance_masks] = strtok(maintenance_buffer, ";");
   while (maintenance_masks[num_maintenance_masks] != NULL)
   {
      num_maintenance_masks++;
      if (num_maintenance_masks == MAX_MAINTENANCE_MASKS)
         break;
      maintenance_masks[num_maintenance_masks] = strtok(NULL, ";");
   }
}

int AsyncSocketAccept(SOCKET sock, int event, int error, int connection_type)
{
   SOCKET new_sock;
   SOCKADDR_IN6 acc_sin;
   socklen_t acc_sin_len;
   SOCKADDR_IN6 peer_info;
   socklen_t peer_len;
   struct in6_addr peer_addr;
   connection_node conn;
   session_node *s;

   if (event != FD_ACCEPT)
   {
      eprintf("AsyncSocketAccept got non-accept %i\n", event);
      return 0;
   }

   if (error != 0)
   {
      eprintf("AsyncSocketAccept got error %i\n", error);
      return 0;
   }

   acc_sin_len = sizeof(acc_sin);
   new_sock = accept(sock, (struct sockaddr *)&acc_sin, &acc_sin_len);

   if (new_sock == SOCKET_ERROR)
   {
      if (errno != EAGAIN && errno != EWOULDBLOCK)
         eprintf("AsyncSocketAccept accept failed, error %i\n", GetLastError());
      return SOCKET_ERROR;
   }

   peer_len = sizeof(peer_info);
   if (getpeername(new_sock, (SOCKADDR *)&peer_info, &peer_len) < 0)
   {
      eprintf("AsyncSocketAccept getpeername failed error %i\n", GetLastError());
      return 0;
   }

   memcpy(&peer_addr, &peer_info.sin6_addr, sizeof(struct in6_addr));
   memcpy(&conn.addr, &peer_addr, sizeof(struct in6_addr));
   inet_ntop(AF_INET6, &peer_addr, conn.name, sizeof(conn.name));

   if (connection_type == SOCKET_MAINTENANCE_PORT)
   {
      if (!CheckMaintenanceMask(&peer_info, peer_len))
      {
         lprintf("Blocked maintenance connection from %s.\n", conn.name);
         closesocket(new_sock);
         return 0;
      }
   }
   else
   {
      if (!CheckBlockList(&peer_addr))
      {
         lprintf("Blocked connection from %s.\n", conn.name);
         closesocket(new_sock);
         return 0;
      }
   }

   conn.type = CONN_SOCKET;
   conn.socket = new_sock;

   // Set TCP_NODELAY and non-blocking on accepted socket
   // Linux accept() does not inherit socket options from listen socket
   {
      int nodelay = 1;
      int flags;
      setsockopt(new_sock, IPPROTO_TCP, TCP_NODELAY, (char *)&nodelay, sizeof(nodelay));
      flags = fcntl(new_sock, F_GETFL, 0);
      if (fcntl(new_sock, F_SETFL, flags | O_NONBLOCK) < 0)
         eprintf("AsyncSocketAccept error setting non-blocking on socket\n");
   }

   s = CreateSession(conn);
   if (s != NULL)
   {
      StartAsyncSession(s);

      switch (connection_type)
      {
      case SOCKET_PORT:
         InitSessionState(s, STATE_SYNCHED);
         break;
      case SOCKET_MAINTENANCE_PORT:
         InitSessionState(s, STATE_MAINTENANCE);
         break;
      default:
         eprintf("AsyncSocketAccept got invalid connection type %i\n", connection_type);
      }

      s->conn.hLookup = 0;
   }

   return new_sock;
}

Bool CheckMaintenanceMask(SOCKADDR_IN6 *addr, int len_addr)
{
   IN6_ADDR mask;

   for (int i = 0; i < num_maintenance_masks; i++)
   {
      if (inet_pton(AF_INET6, maintenance_masks[i], &mask) != 1)
      {
         eprintf("CheckMaintenanceMask has invalid configured mask %s\n",
            maintenance_masks[i]);
         continue;
      }

      BOOL skip = 0;
      for (int k = 0; k < (int)sizeof(mask.s6_addr); k++)
      {
         if (mask.s6_addr[k] != 0 && mask.s6_addr[k] != addr->sin6_addr.s6_addr[k])
         {
            skip = 1;
            break;
         }
      }

      if (skip)
         continue;

      return True;
   }
   return False;
}
