# Makefile for DMR Voice Relay Server

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = 

# Platform-specific settings
ifeq ($(OS),Windows_NT)
    LDFLAGS += -lws2_32 -lmariadb
    TARGET = dmr_server.exe
    RM = del /Q
    CFLAGS += -I"C:/Program Files/MariaDB/include"
    LDFLAGS += -L"C:/Program Files/MariaDB/lib"
else
    LDFLAGS += -lpthread -lmariadbclient
    TARGET = dmr_server
    RM = rm -f
    CFLAGS += $(shell mysql_config --cflags)
    LDFLAGS += $(shell mysql_config --libs)
endif

# Source files
SRCS = main.c dmr_server.c dmr_db.c
OBJS = $(SRCS:.c=.o)

# Header files
HDRS = dmr_server.h

# Default target
all: $(TARGET)

# Linking
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compilation
%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	$(RM) $(OBJS) $(TARGET)

# Install (Unix-like systems only)
install: $(TARGET)
	@echo "Installing DMR Voice Relay Server..."
	@mkdir -p /usr/local/bin
	@cp $(TARGET) /usr/local/bin/
	@chmod 755 /usr/local/bin/$(TARGET)
	@echo "Installation complete."

# Uninstall (Unix-like systems only)
uninstall:
	@echo "Uninstalling DMR Voice Relay Server..."
	@rm -f /usr/local/bin/$(TARGET)
	@echo "Uninstallation complete."

.PHONY: all clean install uninstall