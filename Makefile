default:
	gcc -std=c89 -pedantic -fPIE -Os -o exec0 exec0.c
	strip exec0

clean:
	rm -f exec0
