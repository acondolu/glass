.PHONY = clean

CC = g++
CFLAGS = -O3 -std=c++14 -Wall -Ihopscotch-map/include
LIBS = -lnetfilter_queue

OBJ = out/main.o out/utils.o out/netfilter.o out/tcp.o out/table.o out/queue.o out/pipe.o out/socket.o out/shutdown.o

out/%.o: src/%.cpp src/%.h
	$(CC) $(CFLAGS) $(LIBS) -c $< -o $@

out/glass: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) $(LIBS) -o out/glass

clean:
	rm -f out/*.o out/glass
