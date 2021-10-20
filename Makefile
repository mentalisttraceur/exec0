default:
	gcc -std=c89 -pedantic \
	    -fPIE -Os -s -Wl,--gc-sections \
	    -o exec0 exec0.c
	strip -s exec0

clean:
	rm -f exec0
