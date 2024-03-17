TARGET:=sndsample_u
SRCS:=play_user.c
OBJS:=$(SRCS:.c=.o)
LZED:=-lzed
LALSA:= -lasound -lpthread -lrt -ldl -lm

include zed.mk
.PHONY: clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LALSA) $(LZED)

%.o: %.c %.h
	$(CC) -c $<

%.o: %.c
	$(CC) -c $<

clean:
	rm -rf $(OBJS) $(TARGET)
