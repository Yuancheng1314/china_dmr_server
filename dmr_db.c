/*
 * DMR Voice Relay Server - Database Module
 * 
 * This file contains the database functionality for the DMR voice relay server.
 * 
 * Copyright (c) 2025
 */

#include "dmr_server.h"

/* Global variables */
static MYSQL *mysql_conn = NULL;
static bool db_enabled = false;
static char db_error_message[1024];

/* Initialize database connection */
int dmr_db_init(dmr_db_config_t *config) {
    if (!config->enabled) {
        printf("Database logging disabled\n");
        db_enabled = false;
        return 0;
    }
    
    /* Initialize MySQL library */
    if (mysql_library_init(0, NULL, NULL)) {
        snprintf(db_error_message, sizeof(db_error_message), "Failed to initialize MySQL library");
        return -1;
    }
    
    /* Create MySQL connection handle */
    mysql_conn = mysql_init(NULL);
    if (mysql_conn == NULL) {
        snprintf(db_error_message, sizeof(db_error_message), "Failed to initialize MySQL connection: %s", 
                 mysql_error(mysql_conn));
        mysql_library_end();
        return -1;
    }
    
    /* Set connection options */
    my_bool reconnect = 1;
    mysql_options(mysql_conn, MYSQL_OPT_RECONNECT, &reconnect);
    
    /* Connect to database */
    if (!mysql_real_connect(mysql_conn, 
                           config->host, 
                           config->user, 
                           config->password, 
                           config->database, 
                           config->port, 
                           NULL, 0)) {
        snprintf(db_error_message, sizeof(db_error_message), "Failed to connect to database: %s", 
                 mysql_error(mysql_conn));
        mysql_close(mysql_conn);
        mysql_library_end();
        mysql_conn = NULL;
        return -1;
    }
    
    printf("Connected to MariaDB database: %s@%s:%d/%s\n", 
           config->user, config->host, config->port, config->database);
    
    /* Create tables if they don't exist */
    if (dmr_db_create_tables() != 0) {
        mysql_close(mysql_conn);
        mysql_library_end();
        mysql_conn = NULL;
        return -1;
    }
    
    db_enabled = true;
    return 0;
}

/* Create database tables if they don't exist */
int dmr_db_create_tables(void) {
    if (!db_enabled || mysql_conn == NULL) {
        return 0;
    }
    
    /* Create clients table */
    const char *create_clients_table = 
        "CREATE TABLE IF NOT EXISTS dmr_clients ("
        "  id INT AUTO_INCREMENT PRIMARY KEY,"
        "  dmr_id INT UNSIGNED NOT NULL,"
        "  callsign VARCHAR(10),"
        "  ip_address VARCHAR(45) NOT NULL,"
        "  port INT UNSIGNED NOT NULL,"
        "  first_seen DATETIME NOT NULL,"
        "  last_seen DATETIME NOT NULL,"
        "  active BOOLEAN NOT NULL DEFAULT TRUE,"
        "  INDEX (dmr_id),"
        "  INDEX (callsign),"
        "  INDEX (ip_address, port)"
        ")";
    
    if (mysql_query(mysql_conn, create_clients_table)) {
        snprintf(db_error_message, sizeof(db_error_message), "Failed to create clients table: %s", 
                 mysql_error(mysql_conn));
        return -1;
    }
    
    /* Create frames table */
    const char *create_frames_table = 
        "CREATE TABLE IF NOT EXISTS dmr_frames ("
        "  id INT AUTO_INCREMENT PRIMARY KEY,"
        "  timestamp DATETIME NOT NULL,"
        "  type TINYINT UNSIGNED NOT NULL,"
        "  slot TINYINT UNSIGNED NOT NULL,"
        "  src_id INT UNSIGNED NOT NULL,"
        "  dst_id INT UNSIGNED NOT NULL,"
        "  client_ip VARCHAR(45) NOT NULL,"
        "  client_port INT UNSIGNED NOT NULL,"
        "  payload_size TINYINT UNSIGNED NOT NULL,"
        "  INDEX (timestamp),"
        "  INDEX (src_id),"
        "  INDEX (dst_id)"
        ")";
    
    if (mysql_query(mysql_conn, create_frames_table)) {
        snprintf(db_error_message, sizeof(db_error_message), "Failed to create frames table: %s", 
                 mysql_error(mysql_conn));
        return -1;
    }
    
    /* Create events table */
    const char *create_events_table = 
        "CREATE TABLE IF NOT EXISTS dmr_events ("
        "  id INT AUTO_INCREMENT PRIMARY KEY,"
        "  timestamp DATETIME NOT NULL,"
        "  event_type VARCHAR(32) NOT NULL,"
        "  dmr_id INT UNSIGNED,"
        "  callsign VARCHAR(10),"
        "  ip_address VARCHAR(45),"
        "  port INT UNSIGNED,"
        "  details TEXT,"
        "  INDEX (timestamp),"
        "  INDEX (event_type),"
        "  INDEX (dmr_id)"
        ")";
    
    if (mysql_query(mysql_conn, create_events_table)) {
        snprintf(db_error_message, sizeof(db_error_message), "Failed to create events table: %s", 
                 mysql_error(mysql_conn));
        return -1;
    }
    
    return 0;
}

/* Log a DMR frame to the database */
int dmr_db_log_frame(dmr_frame_t *frame, struct sockaddr_in *client_addr) {
    if (!db_enabled || mysql_conn == NULL || frame == NULL || client_addr == NULL) {
        return 0;
    }
    
    char query[512];
    char client_ip[INET_ADDRSTRLEN];
    
    /* Get client IP address as string */
    inet_ntop(AF_INET, &client_addr->sin_addr, client_ip, INET_ADDRSTRLEN);
    
    /* Build query */
    snprintf(query, sizeof(query),
             "INSERT INTO dmr_frames (timestamp, type, slot, src_id, dst_id, client_ip, client_port, payload_size) "
             "VALUES (NOW(), %d, %d, %u, %u, '%s', %d, %d)",
             frame->type, frame->slot, frame->src_id, frame->dst_id,
             client_ip, ntohs(client_addr->sin_port), DMR_PAYLOAD_SIZE);
    
    /* Execute query */
    if (mysql_query(mysql_conn, query)) {
        snprintf(db_error_message, sizeof(db_error_message), "Failed to log frame: %s", 
                 mysql_error(mysql_conn));
        return -1;
    }
    
    return 0;
}

/* Log a client event to the database */
int dmr_db_log_client(dmr_client_t *client, const char *event) {
    if (!db_enabled || mysql_conn == NULL || client == NULL || event == NULL) {
        return 0;
    }
    
    char query[512];
    char client_ip[INET_ADDRSTRLEN];
    
    /* Get client IP address as string */
    inet_ntop(AF_INET, &client->addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    
    /* Build query */
    snprintf(query, sizeof(query),
             "INSERT INTO dmr_events (timestamp, event_type, dmr_id, callsign, ip_address, port) "
             "VALUES (NOW(), '%s', %u, '%s', '%s', %d)",
             event, client->dmr_id, client->callsign[0] ? client->callsign : "Unknown",
             client_ip, ntohs(client->addr.sin_port));
    
    /* Execute query */
    if (mysql_query(mysql_conn, query)) {
        snprintf(db_error_message, sizeof(db_error_message), "Failed to log client event: %s", 
                 mysql_error(mysql_conn));
        return -1;
    }
    
    /* Update client in clients table */
    if (strcmp(event, "connect") == 0) {
        /* Check if client exists */
        snprintf(query, sizeof(query),
                 "SELECT id FROM dmr_clients WHERE ip_address = '%s' AND port = %d",
                 client_ip, ntohs(client->addr.sin_port));
        
        if (mysql_query(mysql_conn, query)) {
            return -1;
        }
        
        MYSQL_RES *result = mysql_store_result(mysql_conn);
        if (result == NULL) {
            return -1;
        }
        
        if (mysql_num_rows(result) > 0) {
            /* Update existing client */
            mysql_free_result(result);
            
            snprintf(query, sizeof(query),
                     "UPDATE dmr_clients SET dmr_id = %u, callsign = '%s', last_seen = NOW(), active = TRUE "
                     "WHERE ip_address = '%s' AND port = %d",
                     client->dmr_id, client->callsign[0] ? client->callsign : "Unknown",
                     client_ip, ntohs(client->addr.sin_port));
        } else {
            /* Insert new client */
            mysql_free_result(result);
            
            snprintf(query, sizeof(query),
                     "INSERT INTO dmr_clients (dmr_id, callsign, ip_address, port, first_seen, last_seen, active) "
                     "VALUES (%u, '%s', '%s', %d, NOW(), NOW(), TRUE)",
                     client->dmr_id, client->callsign[0] ? client->callsign : "Unknown",
                     client_ip, ntohs(client->addr.sin_port));
        }
        
        if (mysql_query(mysql_conn, query)) {
            snprintf(db_error_message, sizeof(db_error_message), "Failed to update client: %s", 
                     mysql_error(mysql_conn));
            return -1;
        }
    } else if (strcmp(event, "disconnect") == 0 || strcmp(event, "timeout") == 0) {
        /* Mark client as inactive */
        snprintf(query, sizeof(query),
                 "UPDATE dmr_clients SET last_seen = NOW(), active = FALSE "
                 "WHERE ip_address = '%s' AND port = %d",
                 client_ip, ntohs(client->addr.sin_port));
        
        if (mysql_query(mysql_conn, query)) {
            snprintf(db_error_message, sizeof(db_error_message), "Failed to update client: %s", 
                     mysql_error(mysql_conn));
            return -1;
        }
    }
    
    return 0;
}

/* Get callsign for a DMR ID from the database */
int dmr_db_get_callsign(uint32_t dmr_id, char *callsign, size_t size) {
    if (!db_enabled || mysql_conn == NULL || callsign == NULL || size == 0) {
        return -1;
    }
    
    char query[256];
    
    /* Build query */
    snprintf(query, sizeof(query),
             "SELECT callsign FROM dmr_clients WHERE dmr_id = %u ORDER BY last_seen DESC LIMIT 1",
             dmr_id);
    
    /* Execute query */
    if (mysql_query(mysql_conn, query)) {
        snprintf(db_error_message, sizeof(db_error_message), "Failed to query callsign: %s", 
                 mysql_error(mysql_conn));
        return -1;
    }
    
    /* Get result */
    MYSQL_RES *result = mysql_store_result(mysql_conn);
    if (result == NULL) {
        snprintf(db_error_message, sizeof(db_error_message), "Failed to get result: %s", 
                 mysql_error(mysql_conn));
        return -1;
    }
    
    /* Check if we have a result */
    if (mysql_num_rows(result) == 0) {
        mysql_free_result(result);
        return -1;
    }
    
    /* Get callsign */
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row == NULL || row[0] == NULL) {
        mysql_free_result(result);
        return -1;
    }
    
    /* Copy callsign */
    strncpy(callsign, row[0], size - 1);
    callsign[size - 1] = '\0';
    
    mysql_free_result(result);
    return 0;
}

/* Clean up database connection */
void dmr_db_cleanup(void) {
    if (mysql_conn != NULL) {
        mysql_close(mysql_conn);
        mysql_conn = NULL;
    }
    
    mysql_library_end();
    db_enabled = false;
}