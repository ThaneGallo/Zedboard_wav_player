ZEDBOARD_IP?=10.42.1.1
DEBUG_PORT?=1234
BIN_DEST?=/home/root
GDB?=gdb-multiarch
TARGET_ARGS?=

.PHONY: upload run kill debug debug-remote

all:

upload: $(TARGET)
	@scp -q $^ root@$(ZEDBOARD_IP):$(BIN_DEST)

run: upload
	@ssh -q root@$(ZEDBOARD_IP) $(BIN_DEST)/$(TARGET) $(TARGET_ARGS)

kill:
	@ssh -q root@$(ZEDBOARD_IP) pkill -9 -f $(TARGET) || true

debug-remote: upload
	@ssh -q -f root@$(ZEDBOARD_IP) "bash -c 'nohup gdbserver --once :$(DEBUG_PORT) $(BIN_DEST)/$(TARGET) $(TARGET_ARGS) &> /dev/null < /dev/null &'"

debug: debug-remote
	$(GDB) $(TARGET) -ex "target remote $(ZEDBOARD_IP):$(DEBUG_PORT)"
