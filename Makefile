TARGET:=sndsample_u
SRCS:=usermode-hw-player.c
OBJS:=$(SRCS:.c=.o)
LZED:=-lzed
LALSA:= -lasound -lpthread -lrt -ldl -lm

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LALSA) $(LZED)

%.o: %.c %.h
	$(CC) -c $<

%.o: %.c
	$(CC) -c $<

clean:
	rm -rf $(OBJS) $(TARGET)