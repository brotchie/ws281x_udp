server: server.c
	gcc -I../rpi_ws281x server.c -lws2811 -lm -L../rpi_ws281x -o server

clean:
	rm server
