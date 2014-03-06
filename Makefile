all:
	gcc exposurecontrol.c main.c shapedetector.c -O3 -o dotdetector `pkg-config --libs --cflags opencv`
	

debug:
	gcc exposurecontrol.c main.c -o dotdetector -g -DPROFILING `pkg-config --libs --cflags opencv`
	
clean:
	rm -f dotdetector
