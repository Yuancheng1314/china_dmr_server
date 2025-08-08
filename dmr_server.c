/*
 * DMR Voice Relay Server
 * 
 * This file contains the implementation of the DMR voice relay server.
 * 
 * Copyright (c) 2025
 */

#include "dmr_server.h"

/* Global variables */
static int server_socket = -1;
static dmr_client_t clients[DMR_MAX_CLIENTS];
static int client_count = 0;
static dmr_config_t server_config;

/* Statistics */
static uint64_t packets_received = 0;
static uint64_t packets_relayed = 0;
static uint64_t bytes_received = 0;
static uint64_t bytes_sent = 0;

/* Initialize the DMR server */
int dmr_server_init(dmr_config_t *config) {
    int i;
    
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        fprintf(stderr, "Failed to initialize Winsock\n");
        return -1;
    }
#endif

    /* Copy configuration */
    memcpy(&server_config, config, sizeof(dmr_config_t));
    
    /* Initialize client array */
    for (i = 0; i < DMR_MAX_CLIENTS; i++) {
        clients[i].active = false;
    }
    
    /* Initialize database if enabled */
    if (config->db.enabled) {
        if (dmr_db_init(&config->db) != 0) {
            fprintf(stderr, "Warning: Failed to initialize database connection\n");
            /* Continue without database */
        }
    }
    
    /* Create UDP socket */
    server_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server_socket < 0) {
#ifdef _WIN32
        fprintf(stderr, "Failed to create socket: %d\n", WSAGetLastError());
        WSACleanup();
#else
        perror("Failed to create socket");
#endif
        return -1;
    }
    
    /* Set up server address */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config->port);
    
    if (config->bind_addr) {
        if (inet_pton(AF_INET, config->bind_addr, &server_addr.sin_addr) <= 0) {
            fprintf(stderr, "Invalid bind address: %s\n", config->bind_addr);
#ifdef _WIN32
            closesocket(server_socket);
            WSACleanup();
#else
            close(server_socket);
#endif
            return -1;
        }
    } else {
        server_addr.sin_addr.s_addr = INADDR_ANY;
    }
    
    /* Bind socket */
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
#ifdef _WIN32
        fprintf(stderr, "Failed to bind socket: %d\n", WSAGetLastError());
        closesocket(server_socket);
        WSACleanup();
#else
        perror("Failed to bind socket");
        close(server_socket);
#endif
        return -1;
    }
    
    printf("DMR Voice Relay Server initialized on port %d\n", config->port);
    return 0;
}

/* Run the DMR server */
int dmr_server_run(void) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    uint8_t buffer[DMR_BUFFER_SIZE];
    int bytes_read;
    dmr_frame_t frame;
    
    printf("DMR Voice Relay Server running...\n");
    
    while (1) {
        /* Receive data */
        bytes_read = recvfrom(server_socket, (char *)buffer, DMR_BUFFER_SIZE, 0, 
                             (struct sockaddr *)&client_addr, &addr_len);
        
        if (bytes_read < 0) {
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK || err == WSAEINTR) {
                continue;
            }
            fprintf(stderr, "Error receiving data: %d\n", err);
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                continue;
            }
            perror("Error receiving data");
#endif
            continue;
        }
        
        /* Update statistics */
        packets_received++;
        bytes_received += bytes_read;
        
        /* Process received data */
        if (bytes_read >= DMR_HEADER_SIZE) {
            /* Parse DMR frame */
            frame.type = buffer[0];
            frame.slot = buffer[1];
            frame.src_id = (buffer[2] << 16) | (buffer[3] << 8) | buffer[4];
            frame.dst_id = (buffer[5] << 16) | (buffer[6] << 8) | buffer[7];
            
            /* Copy payload */
            int payload_size = bytes_read - DMR_HEADER_SIZE;
            if (payload_size > DMR_PAYLOAD_SIZE) {
                payload_size = DMR_PAYLOAD_SIZE;
            }
            memcpy(frame.payload, buffer + DMR_HEADER_SIZE, payload_size);
            
            /* Process frame */
            dmr_process_frame(&frame, &client_addr);
            
            /* Relay frame to other clients */
            dmr_relay_frame(&frame, &client_addr);
        }
        
        /* Periodically clean up inactive clients */
        static time_t last_cleanup = 0;
        time_t now = time(NULL);
        if (now - last_cleanup > 60) { /* Clean up every minute */
            dmr_cleanup_clients();
            last_cleanup = now;
            
            /* Print statistics */
            if (server_config.verbose) {
                dmr_print_stats();
            }
        }
    }
    
    return 0;
}

/* Process a DMR frame */
int dmr_process_frame(dmr_frame_t *frame, struct sockaddr_in *client_addr) {
    int i;
    bool client_found = false;
    
    /* Check if client exists */
    for (i = 0; i < DMR_MAX_CLIENTS; i++) {
        if (clients[i].active && 
            clients[i].addr.sin_addr.s_addr == client_addr->sin_addr.s_addr && 
            clients[i].addr.sin_port == client_addr->sin_port) {
            
            /* Update last seen time */
            clients[i].last_seen = time(NULL);
            
            /* Update DMR ID if needed */
            if (clients[i].dmr_id == 0 && frame->src_id != 0) {
                clients[i].dmr_id = frame->src_id;
                
                /* Try to get callsign from database */
                if (server_config.db.enabled) {
                    char callsign[10];
                    if (dmr_db_get_callsign(frame->src_id, callsign, sizeof(callsign)) == 0) {
                        strncpy(clients[i].callsign, callsign, sizeof(clients[i].callsign) - 1);
                        clients[i].callsign[sizeof(clients[i].callsign) - 1] = '\0';
                    }
                }
            }
            
            client_found = true;
            break;
        }
    }
    
    /* Add new client if not found */
    if (!client_found) {
        dmr_add_client(client_addr, frame->src_id, NULL);
    }
    
    /* Print frame info if verbose */
    if (server_config.verbose) {
        char src_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr->sin_addr, src_ip, INET_ADDRSTRLEN);
        
        printf("Received %s frame from %s:%d, Src ID: %u, Dst ID: %u, Slot: %d\n",
               frame->type == DMR_PKT_VOICE ? "Voice" :
               frame->type == DMR_PKT_DATA ? "Data" :
               frame->type == DMR_PKT_CONTROL ? "Control" :
               frame->type == DMR_PKT_SYNC ? "Sync" : "Unknown",
               src_ip, ntohs(client_addr->sin_port),
               frame->src_id, frame->dst_id, frame->slot);
    }
    
    /* Log frame to database if enabled */
    if (server_config.db.enabled) {
        dmr_db_log_frame(frame, client_addr);
    }
    
    return 0;
}

/* Relay a DMR frame to all clients except the sender */
int dmr_relay_frame(dmr_frame_t *frame, struct sockaddr_in *exclude_addr) {
    int i;
    uint8_t buffer[DMR_BUFFER_SIZE];
    int buffer_size = 0;
    
    /* Build frame buffer */
    buffer[0] = frame->type;
    buffer[1] = frame->slot;
    buffer[2] = (frame->src_id >> 16) & 0xFF;
    buffer[3] = (frame->src_id >> 8) & 0xFF;
    buffer[4] = frame->src_id & 0xFF;
    buffer[5] = (frame->dst_id >> 16) & 0xFF;
    buffer[6] = (frame->dst_id >> 8) & 0xFF;
    buffer[7] = frame->dst_id & 0xFF;
    
    /* Copy payload */
    memcpy(buffer + DMR_HEADER_SIZE, frame->payload, DMR_PAYLOAD_SIZE);
    buffer_size = DMR_HEADER_SIZE + DMR_PAYLOAD_SIZE;
    
    /* Send to all active clients except the sender */
    for (i = 0; i < DMR_MAX_CLIENTS; i++) {
        if (clients[i].active) {
            /* Skip sender */
            if (exclude_addr && 
                clients[i].addr.sin_addr.s_addr == exclude_addr->sin_addr.s_addr && 
                clients[i].addr.sin_port == exclude_addr->sin_port) {
                continue;
            }
            
            /* Send frame */
            int sent = sendto(server_socket, (char *)buffer, buffer_size, 0,
                             (struct sockaddr *)&clients[i].addr, sizeof(clients[i].addr));
            
            if (sent < 0) {
#ifdef _WIN32
                fprintf(stderr, "Failed to send to client: %d\n", WSAGetLastError());
#else
                perror("Failed to send to client");
#endif
            } else {
                bytes_sent += sent;
                packets_relayed++;
            }
        }
    }
    
    return 0;
}

/* Add a new client */
int dmr_add_client(struct sockaddr_in *addr, uint32_t dmr_id, const char *callsign) {
    int i;
    
    /* Find an empty slot */
    for (i = 0; i < DMR_MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            /* Add client */
            memcpy(&clients[i].addr, addr, sizeof(struct sockaddr_in));
            clients[i].last_seen = time(NULL);
            clients[i].dmr_id = dmr_id;
            clients[i].active = true;
            
            if (callsign) {
                strncpy(clients[i].callsign, callsign, sizeof(clients[i].callsign) - 1);
                clients[i].callsign[sizeof(clients[i].callsign) - 1] = '\0';
            } else {
                clients[i].callsign[0] = '\0';
            }
            
            client_count++;
            
            /* Print client info if verbose */
            if (server_config.verbose) {
                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &addr->sin_addr, client_ip, INET_ADDRSTRLEN);
                
                printf("New client connected: %s:%d, DMR ID: %u, Total clients: %d\n",
                       client_ip, ntohs(addr->sin_port), dmr_id, client_count);
            }
            
            /* Log client connection to database if enabled */
            if (server_config.db.enabled) {
                dmr_db_log_client(&clients[i], "connect");
            }
            
            return 0;
        }
    }
    
    fprintf(stderr, "Maximum number of clients reached\n");
    return -1;
}

/* Remove a client */
int dmr_remove_client(struct sockaddr_in *addr) {
    int i;
    
    for (i = 0; i < DMR_MAX_CLIENTS; i++) {
        if (clients[i].active && 
            clients[i].addr.sin_addr.s_addr == addr->sin_addr.s_addr && 
            clients[i].addr.sin_port == addr->sin_port) {
            
            /* Print client info if verbose */
            if (server_config.verbose) {
                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &addr->sin_addr, client_ip, INET_ADDRSTRLEN);
                
                printf("Client disconnected: %s:%d, DMR ID: %u\n",
                       client_ip, ntohs(addr->sin_port), clients[i].dmr_id);
            }
            
            /* Log client disconnection to database if enabled */
            if (server_config.db.enabled) {
                dmr_db_log_client(&clients[i], "disconnect");
            }
            
            /* Log client timeout to database if enabled */
            if (server_config.db.enabled) {
                dmr_db_log_client(&clients[i], "timeout");
            }
            
            /* Remove client */
            clients[i].active = false;
            client_count--;
            
            return 0;
        }
    }
    
    return -1;
}

/* Clean up inactive clients */
void dmr_cleanup_clients(void) {
    int i;
    time_t now = time(NULL);
    
    for (i = 0; i < DMR_MAX_CLIENTS; i++) {
        if (clients[i].active && now - clients[i].last_seen > server_config.timeout) {
            /* Print client info if verbose */
            if (server_config.verbose) {
                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &clients[i].addr.sin_addr, client_ip, INET_ADDRSTRLEN);
                
                printf("Client timed out: %s:%d, DMR ID: %u\n",
                       client_ip, ntohs(clients[i].addr.sin_port), clients[i].dmr_id);
            }
            
            /* Log client timeout to database if enabled */
            if (server_config.db.enabled) {
                dmr_db_log_client(&clients[i], "timeout");
            }
            
            /* Remove client */
            clients[i].active = false;
            client_count--;
        }
    }
}

/* Print server statistics */
void dmr_print_stats(void) {
    printf("=== DMR Server Statistics ===\n");
    printf("Active clients: %d\n", client_count);
    printf("Packets received: %llu\n", (unsigned long long)packets_received);
    printf("Packets relayed: %llu\n", (unsigned long long)packets_relayed);
    printf("Bytes received: %llu\n", (unsigned long long)bytes_received);
    printf("Bytes sent: %llu\n", (unsigned long long)bytes_sent);
    printf("============================\n");
}

/* Clean up the DMR server */
void dmr_server_cleanup(void) {
    if (server_socket >= 0) {
#ifdef _WIN32
        closesocket(server_socket);
        WSACleanup();
#else
        close(server_socket);
#endif
        server_socket = -1;
    }
    
    /* Clean up database connection */
    dmr_db_cleanup();
    
    printf("DMR Voice Relay Server shut down\n");
}