include Rules.mk

#######################################
# list of source files
########################################
LIB_IO_DRIVER_SOURCES = \
src/io_driver.c \
src/io_net.c \
src/io_telnet.c \
src/io_dns.c \
src/io_pipe.c \
src/dns_util.c \
src/io_timer.c \
src/soft_timer.c \
src/telnet_reader.c \
src/circ_buffer.c

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
ifeq ($V, 0)
	@echo "[CC]         $(notdir $<)"
endif
	$Q$(CC) -c $(CFLAGS) $< -o $@

#######################################
# main target
#######################################
$(BUILD_DIR)/$(TARGET): $(OBJECTS) Makefile
ifeq ($V, 0)
	@echo "[AR]         $@"
endif
	$Q$(AR) $(ARFLAGS) $@ $(OBJECTS)

$(BUILD_DIR):
ifeq ($V, 0)
	@echo "MKDIR          $(BUILD_DIR)"
endif
	$Qmkdir $@

#######################################
# tests demo
#######################################
include Tests.mk

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
