// Meridian 59, Copyright 1994-2012 Andrew Kirmse and Chris Kirmse.
// All rights reserved.
//
// This software is distributed under a license that is described in
// the LICENSE file that accompanies it.
//
// Meridian is a registered trademark.

#include "blakserv.h"

#define MAX_MAINTENANCE_MASKS 15
char *maintenance_masks[MAX_MAINTENANCE_MASKS];
int num_maintenance_masks = 0;
char *maintenance_buffer = NULL;

Bool CheckMaintenanceMask(SOCKADDR* addr, int len_addr);

void InitAsyncConnections(void) {
    WSADATA WSAData;
    struct addrinfo hints, * res;
    char hostname[NI_MAXHOST];

    if (WSAStartup(MAKEWORD(2, 2), &WSAData) != 0) {
        eprintf("InitAsyncConnections can't open WinSock!\n");
        return;
    }

    maintenance_buffer = (char*)malloc(strlen(ConfigStr(SOCKET_MAINTENANCE_MASK)) + 1);
    if (maintenance_buffer == NULL) {
        eprintf("InitAsyncConnections can't allocate maintenance buffer!\n");
        return;
    }
    strcpy(maintenance_buffer, ConfigStr(SOCKET_MAINTENANCE_MASK));

    // Parse maintenance IPs and perform DNS lookups if enabled
    maintenance_masks[num_maintenance_masks] = strtok(maintenance_buffer, ";");
    while (maintenance_masks[num_maintenance_masks] != NULL) {
        if (ConfigBool(SOCKET_DNS_LOOKUP)) {
            // Try resolving with IPv6 first
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET6; // Use IPv6
            hints.ai_socktype = SOCK_STREAM;
            if (getaddrinfo(maintenance_masks[num_maintenance_masks], NULL, &hints, &res) == 0) {
                // Get hostname from IP if possible
                if (getnameinfo(res->ai_addr, res->ai_addrlen, hostname, sizeof(hostname), NULL, 0, NI_NAMEREQD) == 0) {
                    // dprintf("Maintenance mask %s resolves to hostname: %s\n", maintenance_masks[num_maintenance_masks], hostname);
                }
                // Get string representation of IP
                char ipstr[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, &((struct sockaddr_in6*)res->ai_addr)->sin6_addr, ipstr, sizeof(ipstr));
                dprintf("Maintenance mask %s resolves to IP: %s\n", maintenance_masks[num_maintenance_masks], ipstr);
                freeaddrinfo(res);
            }
            else {
                // Try resolving with IPv4 if IPv6 fails
                memset(&hints, 0, sizeof(hints));
                hints.ai_family = AF_INET; // Use IPv4
                hints.ai_socktype = SOCK_STREAM;
                if (getaddrinfo(maintenance_masks[num_maintenance_masks], NULL, &hints, &res) == 0) {
                    // Get hostname from IP if possible
                    if (getnameinfo(res->ai_addr, res->ai_addrlen, hostname, sizeof(hostname), NULL, 0, NI_NAMEREQD) == 0) {
                        // dprintf("Maintenance mask %s resolves to hostname: %s\n", maintenance_masks[num_maintenance_masks], hostname);
                    }
                    // Get string representation of IP
                    char ipstr[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &((struct sockaddr_in*)res->ai_addr)->sin_addr, ipstr, sizeof(ipstr));
                    dprintf("Maintenance mask %s resolves to IP: %s\n", maintenance_masks[num_maintenance_masks], ipstr);
                    freeaddrinfo(res);
                }
                else {
                    dprintf("Could not resolve maintenance mask: %s\n", maintenance_masks[num_maintenance_masks]);
                }
            }
        }
        num_maintenance_masks++;
        if (num_maintenance_masks == MAX_MAINTENANCE_MASKS)
            break;
        maintenance_masks[num_maintenance_masks] = strtok(NULL, ";");
    }
}

void ExitAsyncConnections(void)
{
    if (WSACleanup() == SOCKET_ERROR)
        eprintf("ExitAsyncConnections can't close WinSock!\n");
}

void AsyncSocketAccept(SOCKET sock, int event, int error, int connection_type) {
    SOCKET new_sock;
    SOCKADDR_IN6 acc_sin; /* Accept socket address - internet style */
    int acc_sin_len; /* Accept socket address length */
    SOCKADDR_IN6 peer_info;
    int peer_len;
    struct in6_addr peer_addr;
    connection_node conn;
    session_node* s;

    if (event != FD_ACCEPT) {
        eprintf("AsyncSocketAccept got non-accept %i\n", event);
        return;
    }

    if (error != 0) {
        eprintf("AsyncSocketAccept got error %i\n", error);
        return;
    }

    acc_sin_len = sizeof acc_sin;
    new_sock = accept(sock, (struct sockaddr*)&acc_sin, &acc_sin_len);
    if (new_sock == SOCKET_ERROR) {
        eprintf("AcceptSocketConnections accept failed, error %i\n", GetLastError());
        return;
    }

    peer_len = sizeof peer_info;
    if (getpeername(new_sock, (SOCKADDR*)&peer_info, &peer_len) < 0) {
        eprintf("AcceptSocketConnections getpeername failed error %i\n", GetLastError());
        return;
    }

    memcpy(&peer_addr, &peer_info.sin6_addr, sizeof(struct in6_addr));
    memcpy(&conn.addr, &peer_addr, sizeof(struct in6_addr));
    inet_ntop(AF_INET6, &peer_addr, conn.name, sizeof(conn.name));

    if (connection_type == SOCKET_MAINTENANCE_PORT) {
        if (!CheckMaintenanceMask((SOCKADDR*)&peer_info, peer_len)) {
            lprintf("Blocked maintenance connection from %s.\n", conn.name);
            closesocket(new_sock);
            return;
        }
    }
    else {
        if (!CheckBlockList(&peer_addr)) {
            lprintf("Blocked connection from %s.\n", conn.name);
            closesocket(new_sock);
            return;
        }
    }

    conn.type = CONN_SOCKET;
    conn.socket = new_sock;
    EnterServerLock();
    s = CreateSession(conn);
    if (s != NULL) {
        StartAsyncSession(s);
        switch (connection_type) {
        case SOCKET_PORT:
            InitSessionState(s, STATE_SYNCHED);
            break;
        case SOCKET_MAINTENANCE_PORT:
            InitSessionState(s, STATE_MAINTENANCE);
            break;
        default:
            eprintf("AcceptSocketConnections got invalid connection type %i\n", connection_type);
        }
        /* need to do this AFTER s->conn is set in place, because the async call writes to that mem address */
        if (ConfigBool(SOCKET_DNS_LOOKUP)) {
            s->conn.hLookup = StartAsyncNameLookup((char*)&peer_info, s->conn.peer_data);
        }
        else {
            s->conn.hLookup = 0;
        }
    }
    LeaveServerLock();
}

void AsyncSocketSelect(SOCKET sock,int event,int error)
{
   session_node *s;

   EnterSessionLock();

   if (error != 0)
   {
      LeaveSessionLock();
      s = GetSessionBySocket(sock);
      if (s != NULL)
      {
      /* we can get events for sockets that have been closed by main thread
         (and hence get NULL here), so be aware! */

         /* eprintf("AsyncSocketSelect got error %i session %i\n",error,s->session_id); */

         HangupSession(s);
         return;
      }

      /* eprintf("AsyncSocketSelect got socket that matches no session %i\n",sock); */

      return;
   }

   switch (event)
   {
   case FD_CLOSE :
      AsyncSocketClose(sock);
      break;

   case FD_WRITE :
      AsyncSocketWrite(sock);
      break;

   case FD_READ :
      AsyncSocketRead(sock);
      break;

   default :
      eprintf("AsyncSocketSelect got unknown event %i\n",event);
      break;
   }

   LeaveSessionLock();
}

void AsyncSocketSelectUDP(SOCKET sock)
{
   EnterSessionLock();

   // no event cases, UDP is stateless
   // only event FD_READ is registered on socket
   AsyncSocketReadUDP(sock);

   LeaveSessionLock();
}

Bool CheckMaintenanceMask(SOCKADDR* addr, int len_addr) {
    struct addrinfo hints, * res;
    int i;
    char hostname[NI_MAXHOST];
    char connecting_ip[INET6_ADDRSTRLEN];
    int result;

    // Convert connecting address to string for logging
    if (addr->sa_family == AF_INET) {
        inet_ntop(AF_INET, &((struct sockaddr_in*)addr)->sin_addr, connecting_ip, sizeof(connecting_ip));
    }
    else if (addr->sa_family == AF_INET6) {
        inet_ntop(AF_INET6, &((struct sockaddr_in6*)addr)->sin6_addr, connecting_ip, sizeof(connecting_ip));
    }

    // First try to get the hostname of the connecting IP
    result = getnameinfo(addr, len_addr, hostname, sizeof(hostname), NULL, 0, NI_NAMEREQD);
    //dprintf("Checking maintenance mask for connection from IP: %s, Hostname: %s\n",
        //connecting_ip,
        //(result == 0) ? hostname : "Hostname lookup failed");

    for (i = 0; i < num_maintenance_masks; i++) {
        //dprintf("Checking against maintenance mask: %s\n", maintenance_masks[i]);

        // Direct hostname comparison
        if (result == 0 && strcmp(maintenance_masks[i], hostname) == 0) {
            //dprintf("Match found via hostname\n");
            return True;
        }

        // Try resolving the mask for IPv4
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET; // Use IPv4
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(maintenance_masks[i], NULL, &hints, &res) == 0) {
            char mask_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &((struct sockaddr_in*)res->ai_addr)->sin_addr, mask_ip, sizeof(mask_ip));
            //dprintf("Comparing IPv4: Mask IP %s vs Connecting IP %s\n", mask_ip, connecting_ip);

            // Compare IPv4 addresses, handling mapped IPv6 addresses
            if (addr->sa_family == AF_INET) {
                // Direct IPv4 comparison
                if (memcmp(&((struct sockaddr_in*)res->ai_addr)->sin_addr,
                    &((struct sockaddr_in*)addr)->sin_addr, sizeof(struct in_addr)) == 0) {
                    //dprintf("Match found via IPv4 address (direct)\n");
                    freeaddrinfo(res);
                    return True;
                }
            }
            else if (addr->sa_family == AF_INET6) {
                // Check for IPv4-mapped IPv6 address
                struct sockaddr_in6* addr6 = (struct sockaddr_in6*)addr;
                if (IN6_IS_ADDR_V4MAPPED(&addr6->sin6_addr)) {
                    struct in_addr* v4_addr = (struct in_addr*)(&addr6->sin6_addr.s6_addr[12]);
                    if (memcmp(&((struct sockaddr_in*)res->ai_addr)->sin_addr, v4_addr, sizeof(struct in_addr)) == 0) {
                        //dprintf("Match found via IPv4-mapped IPv6 address\n");
                        freeaddrinfo(res);
                        return True;
                    }
                }
            }
            freeaddrinfo(res);
        }

        // Try resolving the mask for IPv6
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET6; // Use IPv6
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(maintenance_masks[i], NULL, &hints, &res) == 0) {
            char mask_ip[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &((struct sockaddr_in6*)res->ai_addr)->sin6_addr, mask_ip, sizeof(mask_ip));
            //dprintf("Comparing IPv6: Mask IP %s vs Connecting IP %s\n", mask_ip, connecting_ip);

            if (addr->sa_family == AF_INET6) {
                struct sockaddr_in6* addr6 = (struct sockaddr_in6*)addr;
                struct sockaddr_in6* mask6 = (struct sockaddr_in6*)res->ai_addr;

                // Compare IPv6 addresses directly
                if (memcmp(&addr6->sin6_addr, &mask6->sin6_addr, sizeof(struct in6_addr)) == 0) {
                    //dprintf("Match found via IPv6 address\n");
                    freeaddrinfo(res);
                    return True;
                }
            }
            freeaddrinfo(res);
        }
    }

    dprintf("No matching maintenance mask found\n");
    return False;
}
