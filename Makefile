CC = gcc
CFLAGS = -I/opt/homebrew/include $$(/opt/homebrew/bin/pkg-config --cflags raylib)
LDFLAGS = -L/opt/homebrew/lib -lsndfile $$(/opt/homebrew/bin/pkg-config --libs raylib)

build: clean
	$(CC) $(CFLAGS) -O3 -o spectrogram main.c $(LDFLAGS)

debug: clean
	$(CC) $(CFLAGS) -g -O0 -o spectrogram main.c $(LDFLAGS)

clean:
	rm -f spectrogram