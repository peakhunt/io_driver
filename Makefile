include Rules.mk

#######################################
# list of source files
########################################
LIB_IO_DRIVER_SOURCES = \
src/io_driver.c \
src/io_net.c \
src/io_timer.c \
src/io_telnet.c \
src/soft_timer.c \
src/circ_buffer.c \
src/telnet_reader.c

#######################################
C_DEFS  = 

#######################################
# include and lib setup
#######################################
C_INCLUDES =                              \
-Isrc
 
LIBS =  -Lbuild
LIBDIR = 

#######################################
# for verbose output
#######################################
# Prettify output
V = 0
ifeq ($V, 0)
  Q = @
  P = > /dev/null
else
  Q =
  P =
endif

#######################################
# build directory and target setup
#######################################
BUILD_DIR = build
TARGET    = libiodriver.a

#######################################
# compile & link flags
#######################################
CFLAGS += -g $(C_DEFS) $(C_INCLUDES)

# Generate dependency information
CFLAGS += -MMD -MF .dep/$(*F).d

LDFLAGS +=  $(LIBDIR) $(LIBS) 

#######################################
# build target
#######################################
all: $(BUILD_DIR)/$(TARGET)

#######################################
# target source setup
#######################################
TARGET_SOURCES := $(LIB_IO_DRIVER_SOURCES)
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(TARGET_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(TARGET_SOURCES)))

#######################################
# C source build rule
#######################################
$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	@echo "[CC]         $(notdir $<)"
	$Q$(CC) -c $(CFLAGS) $< -o $@

#######################################
# main target
#######################################
$(BUILD_DIR)/$(TARGET): $(OBJECTS) Makefile
	@echo "[AR]         $@"
	$Q$(AR) $(ARFLAGS) $@ $(OBJECTS)

$(BUILD_DIR):
	@echo "MKDIR          $(BUILD_DIR)"
	$Qmkdir $@

#######################################
# tests demo
#######################################
TEST_TARGETS= \
$(BUILD_DIR)/cli_server

.PHONY: tests
tests: $(TEST_TARGETS)

CLI_SERVER_SRC= \
test/cli_server.c \
test/cli.c
CLI_SERVER_OBJS = $(addprefix $(BUILD_DIR)/,$(notdir $(CLI_SERVER_SRC:.c=.o)))
vpath %.c $(sort $(dir $(CLI_SERVER_SRC)))

$(BUILD_DIR)/cli_server: $(BUILD_DIR)/$(TARGET) $(CLI_SERVER_OBJS)
	@echo "[LD]         $@"
	$Q$(CC) $(CLI_SERVER_OBJS) $(LDFLAGS) -o $@ -liodriver


#######################################
# clean up
#######################################
clean:
	@echo "[CLEAN]          $(TARGET) $(BUILD_DIR) .dep"
	$Q-rm -fR .dep $(BUILD_DIR)
	$Q-rm -f test/*.o unit_test/*.o

#######################################
# dependencies
#######################################
-include $(shell mkdir .dep 2>/dev/null) $(wildcard .dep/*)
