all:
	g++ -g cse4001_sync.cpp -o cse4001_sync -lpthread -lm 

clean:
	rm -f cse4001_sync
	

