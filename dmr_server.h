/*
 * DMR Voice Relay Server
 * 
 * This file contains the main definitions and structures for the DMR voice relay server.
 * 
 * Copyright (c) 2025
 */

#ifndef DMR_SERVER_H
#define DMR_SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <mysql/mysql.h>  /* MariaDB/MySQL client library */

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "libmariadb.lib")  /* MariaDB client library for Windows */
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/time.h>
#include <errno.h>
#endif

/* DMR constants */
#define DMR_FRAME_SIZE          33      /* Standard DMR frame size in bytes */
#define DMR_PAYLOAD_SIZE        27      /* DMR payload size in bytes */
#define DMR_HEADER_SIZE         6       /* DMR header size in bytes */
#define DMR_SLOT_TIME_MS        60      /* DMR slot time in milliseconds */
#define DMR_MAX_CLIENTS         100     /* Maximum number of connected clients */
#define DMR_SERVER_PORT         62031   /* Default UDP port for DMR server */
#define DMR_BUFFER_SIZE         1024    /* Buffer size for receiving data */

/* DMR packet types */
#define DMR_PKT_VOICE           0x01    /* Voice packet */
#define DMR_PKT_DATA            0x02    /* Data packet */
#define DMR_PKT_CONTROL         0x03    /* Control packet */
#define DMR_PKT_SYNC            0x04    /* Synchronization packet */

/* DMR slot types */
#define DMR_SLOT_1              0x01    /* DMR slot 1 */
#define DMR_SLOT_2              0x02    /* DMR slot 2 */

/* DMR client structure */
typedef struct {
    struct sockaddr_in addr;            /* Client address */
    time_t last_seen;                   /* Last time client was seen */
    uint32_t dmr_id;                    /* DMR ID of the client */
    bool active;                         /* Is client active */
    char callsign[10];                  /* Client callsign */
} dmr_client_t;

/* DMR frame structure */
typedef struct {
    uint8_t type;                       /* Packet type */
    uint8_t slot;                       /* Slot number */
    uint32_t src_id;                    /* Source DMR ID */
    uint32_t dst_id;                    /* Destination DMR ID */
    uint8_t payload[DMR_PAYLOAD_SIZE];  /* Payload data */
} dmr_frame_t;

/* Database configuration */
typedef struct {
    char *host;                         /* Database host */
    char *user;                         /* Database user */
    char *password;                     /* Database password */
    char *database;                     /* Database name */
    uint16_t port;                      /* Database port */
    bool enabled;                       /* Database enabled flag */
} dmr_db_config_t;

/* DMR server configuration */
typedef struct {
    uint16_t port;                      /* Server port */
    bool verbose;                        /* Verbose output */
    char *bind_addr;                    /* Bind address */
    int timeout;                        /* Client timeout in seconds */
    dmr_db_config_t db;                 /* Database configuration */
} dmr_config_t;

/* Function prototypes */
int dmr_server_init(dmr_config_t *config);
int dmr_server_run(void);
void dmr_server_cleanup(void);
int dmr_process_frame(dmr_frame_t *frame, struct sockaddr_in *client_addr);
int dmr_relay_frame(dmr_frame_t *frame, struct sockaddr_in *exclude_addr);
int dmr_add_client(struct sockaddr_in *addr, uint32_t dmr_id, const char *callsign);
int dmr_remove_client(struct sockaddr_in *addr);
void dmr_cleanup_clients(void);
void dmr_print_stats(void);

/* Database function prototypes */
int dmr_db_init(dmr_db_config_t *config);
void dmr_db_cleanup(void);
int dmr_db_log_frame(dmr_frame_t *frame, struct sockaddr_in *client_addr);
int dmr_db_log_client(dmr_client_t *client, const char *event);
int dmr_db_get_callsign(uint32_t dmr_id, char *callsign, size_t size);
int dmr_db_create_tables(void);

#endif /* DMR_SERVER_H */