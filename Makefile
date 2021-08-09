.PHONY = clean

CC = g++
CFLAGS = -O3 -std=c++14 -Wall -Ihopscotch-map/include -Irapidjson/include
LIBS = -lnetfilter_queue -lsystemd -lpthread

OBJ = out/main.o out/utils.o out/netfilter.o out/tcp.o out/table.o out/socket.o out/shutdown.o out/config.o out/control.o

out/%.o: src/%.cpp src/%.h
	$(CC) $(CFLAGS) -c $< -o $@

out/glass-trans: $(OBJ)
	$(CC) $(OBJ) $(LIBS) -o out/glass-trans

clean:
	rm -f out/*.o out/glass-trans
