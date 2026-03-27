PREFIX = /usr/local/bin
CFLAGS = -std=c11 -pedantic -Wall -Wextra -Os
all: mpd volmon net uptime timedate multicat setroot memwatch cpuload
mpd: 
	gcc $(CFLAGS) -o sb-mpd sb-mpd.c -lmpdclient
volmon: 
	gcc $(CFLAGS) -o sb-volmon sb-volmon.c -lasound -lm
net: 
	gcc $(CFLAGS) -o sb-net sb-net.c
uptime: 
	gcc $(CFLAGS) -o sb-uptime sb-uptime.c
timedate:
	gcc $(CFLAGS) -o sb-timedate sb-timedate.c
multicat:
	gcc $(CFLAGS) -o multicat multicat.c 
setroot:
	gcc $(CFLAGS) -o sb-setroot sb-setroot.c -lxcb
memwatch:
	gcc $(CFLAGS) -o sb-memwatch sb-memwatch.c
cpuload:
	gcc $(CFLAGS) -o sb-cpuload sb-cpuload.c
install:
	-install -m 755 sb-mpd      $(PREFIX)/sb-mpd
	-install -m 755 sb-volmon   $(PREFIX)/sb-volmon
	-install -m 755 sb-net      $(PREFIX)/sb-net
	-install -m 755 sb-uptime   $(PREFIX)/sb-uptime
	-install -m 755 sb-timedate $(PREFIX)/sb-timedate
	-install -m 755 multicat    $(PREFIX)/multicat
	-install -m 755 sb-setroot  $(PREFIX)/sb-setroot
	-install -m 755 sb-memwatch $(PREFIX)/sb-memwatch
	-install -m 755 sb-cpuload  $(PREFIX)/sb-cpuload
clean:
	-rm sb-mpd
	-rm sb-volmon
	-rm sb-net
	-rm sb-uptime
	-rm sb-timedate
	-rm multicat
	-rm sb-setroot
	-rm sb-memwatch
	-rm sb-cpuload
