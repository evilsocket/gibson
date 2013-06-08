all:
	gcc set-benchmark.c -I../src/ -L../ -lgibsonclient -o test.set-benchmark
	gcc mget.c -I../src/ -L../ -lgibsonclient -o test.mget
clean:
	rm -rf test.*