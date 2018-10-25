#######################################
# tests demo
#######################################
TEST_TARGETS= \
$(BUILD_DIR)/cli_server  \
$(BUILD_DIR)/cli_client  \
$(BUILD_DIR)/ssl_server  \
$(BUILD_DIR)/ssl_client  \
$(BUILD_DIR)/dns_client  \
$(BUILD_DIR)/pipe_test  

.PHONY: tests
tests: $(TEST_TARGETS)

CLI_SERVER_SRC= \
test/cli_server.c \
test/cli.c
CLI_SERVER_OBJS = $(addprefix $(BUILD_DIR)/,$(notdir $(CLI_SERVER_SRC:.c=.o)))
vpath %.c $(sort $(dir $(CLI_SERVER_SRC)))

$(BUILD_DIR)/cli_server: $(BUILD_DIR)/$(TARGET) $(CLI_SERVER_OBJS)
	@echo "[LD]         $@"
	$Q$(CC) $(CLI_SERVER_OBJS) $(LDFLAGS) -o $@ -liodriver -lmbedtls -lmbedx509 -lmbedcrypto

CLI_CLIENT_SRC= \
test/cli_client.c
CLI_CLIENT_OBJS = $(addprefix $(BUILD_DIR)/,$(notdir $(CLI_CLIENT_SRC:.c=.o)))
vpath %.c $(sort $(dir $(CLI_CLIENT_SRC)))

$(BUILD_DIR)/cli_client: $(BUILD_DIR)/$(TARGET) $(CLI_CLIENT_OBJS)
	@echo "[LD]         $@"
	$Q$(CC) $(CLI_CLIENT_OBJS) $(LDFLAGS) -o $@ -liodriver -lmbedtls -lmbedx509 -lmbedcrypto

SSL_SERVER_SRC= \
test/ssl_server.c
SSL_SERVER_OBJS = $(addprefix $(BUILD_DIR)/,$(notdir $(SSL_SERVER_SRC:.c=.o)))
vpath %.c $(sort $(dir $(SSL_SERVER_SRC)))

$(BUILD_DIR)/ssl_server: $(BUILD_DIR)/$(TARGET) $(SSL_SERVER_OBJS)
	@echo "[LD]         $@"
	$Q$(CC) $(SSL_SERVER_OBJS) $(LDFLAGS) -o $@ -liodriver -lmbedtls -lmbedx509 -lmbedcrypto

SSL_CLIENT_SRC= \
test/ssl_client.c
SSL_CLIENT_OBJS = $(addprefix $(BUILD_DIR)/,$(notdir $(SSL_CLIENT_SRC:.c=.o)))
vpath %.c $(sort $(dir $(SSL_CLIENT_SRC)))

$(BUILD_DIR)/ssl_client: $(BUILD_DIR)/$(TARGET) $(SSL_CLIENT_OBJS)
	@echo "[LD]         $@"
	$Q$(CC) $(SSL_CLIENT_OBJS) $(LDFLAGS) -o $@ -liodriver -lmbedtls -lmbedx509 -lmbedcrypto

DNS_CLIENT_SRC= \
test/dns_client.c
DNS_CLIENT_OBJS = $(addprefix $(BUILD_DIR)/,$(notdir $(DNS_CLIENT_SRC:.c=.o)))
vpath %.c $(sort $(dir $(DNS_CLIENT_SRC)))

$(BUILD_DIR)/dns_client: $(BUILD_DIR)/$(TARGET) $(DNS_CLIENT_OBJS)
	@echo "[LD]         $@"
	$Q$(CC) $(DNS_CLIENT_OBJS) $(LDFLAGS) -o $@ -liodriver -lmbedtls -lmbedx509 -lmbedcrypto

PIPE_TEST_SRC= \
test/pipe_test.c
PIPE_TEST_OBJS = $(addprefix $(BUILD_DIR)/,$(notdir $(PIPE_TEST_SRC:.c=.o)))
vpath %.c $(sort $(dir $(PIPE_TEST_SRC)))

$(BUILD_DIR)/pipe_test: $(BUILD_DIR)/$(TARGET) $(PIPE_TEST_OBJS)
	@echo "[LD]         $@"
	$Q$(CC) $(PIPE_TEST_OBJS) $(LDFLAGS) -o $@ -liodriver -lmbedtls -lmbedx509 -lmbedcrypto
