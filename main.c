/*
 * DMR Voice Relay Server - Main Program
 * 
 * This file contains the main entry point for the DMR voice relay server.
 * 
 * Copyright (c) 2025
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "dmr_server.h"

/* Global variables */
static volatile int running = 1;

/* Signal handler */
void signal_handler(int sig) {
    printf("Received signal %d, shutting down...\n", sig);
    running = 0;
}

/* Print usage */
void print_usage(const char *program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -p PORT     Server port (default: %d)\n", DMR_SERVER_PORT);
    printf("  -b ADDR     Bind address (default: any)\n");
    printf("  -t TIMEOUT  Client timeout in seconds (default: 300)\n");
    printf("  -v          Verbose output\n");
    printf("  -h          Print this help message\n");
    printf("\nDatabase options:\n");
    printf("  --db-enable Enable database logging\n");
    printf("  --db-host   Database host (default: localhost)\n");
    printf("  --db-port   Database port (default: 3306)\n");
    printf("  --db-user   Database user (default: dmr)\n");
    printf("  --db-pass   Database password\n");
    printf("  --db-name   Database name (default: dmr_server)\n");
}

/* Main function */
int main(int argc, char *argv[]) {
    dmr_config_t config;
    int i;
    
    /* Set default configuration */
    config.port = DMR_SERVER_PORT;
    config.bind_addr = NULL;
    config.verbose = false;
    config.timeout = 300; /* 5 minutes */
    
    /* Set default database configuration */
    config.db.enabled = false;
    config.db.host = "localhost";
    config.db.port = 3306;
    config.db.user = "dmr";
    config.db.password = NULL;
    config.db.database = "dmr_server";
    
    /* Parse command line arguments */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            config.port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
            config.bind_addr = argv[++i];
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            config.timeout = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-v") == 0) {
            config.verbose = true;
        } else if (strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--db-enable") == 0) {
            config.db.enabled = true;
        } else if (strcmp(argv[i], "--db-host") == 0 && i + 1 < argc) {
            config.db.host = argv[++i];
        } else if (strcmp(argv[i], "--db-port") == 0 && i + 1 < argc) {
            config.db.port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--db-user") == 0 && i + 1 < argc) {
            config.db.user = argv[++i];
        } else if (strcmp(argv[i], "--db-pass") == 0 && i + 1 < argc) {
            config.db.password = argv[++i];
        } else if (strcmp(argv[i], "--db-name") == 0 && i + 1 < argc) {
            config.db.database = argv[++i];
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    
    /* Set up signal handlers */
#ifdef _WIN32
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
#else
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
#endif
    
    /* Initialize DMR server */
    if (dmr_server_init(&config) != 0) {
        fprintf(stderr, "Failed to initialize DMR server\n");
        return 1;
    }
    
    printf("DMR Voice Relay Server\n");
    printf("Listening on port: %d\n", config.port);
    if (config.bind_addr) {
        printf("Bind address: %s\n", config.bind_addr);
    }
    printf("Client timeout: %d seconds\n", config.timeout);
    printf("Verbose mode: %s\n", config.verbose ? "enabled" : "disabled");
    
    /* Print database configuration if enabled */
    if (config.db.enabled) {
        printf("\nDatabase logging: enabled\n");
        printf("Database host: %s\n", config.db.host);
        printf("Database port: %d\n", config.db.port);
        printf("Database user: %s\n", config.db.user);
        printf("Database name: %s\n", config.db.database);
    } else {
        printf("\nDatabase logging: disabled\n");
    }
    
    printf("\nPress Ctrl+C to exit\n");
    
    /* Run server in a separate thread */
#ifdef _WIN32
    HANDLE thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)dmr_server_run, NULL, 0, NULL);
    if (thread == NULL) {
        fprintf(stderr, "Failed to create server thread\n");
        dmr_server_cleanup();
        return 1;
    }
#else
    pthread_t thread;
    if (pthread_create(&thread, NULL, (void *(*)(void *))dmr_server_run, NULL) != 0) {
        fprintf(stderr, "Failed to create server thread\n");
        dmr_server_cleanup();
        return 1;
    }
#endif
    
    /* Wait for signal */
    while (running) {
#ifdef _WIN32
        Sleep(1000);
#else
        sleep(1);
#endif
    }
    
    /* Clean up */
    dmr_server_cleanup();
    
#ifdef _WIN32
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
#else
    pthread_cancel(thread);
    pthread_join(thread, NULL);
#endif
    
    return 0;
}