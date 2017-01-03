name=fat-gundel
CFLAGS=-Wall -g -O2

$(name): $(name).c
	$(CC) $(CFLAGS) $(name).c -o $(name)

clean:
	rm -rf *.orig $(name) *.o *.jpg
