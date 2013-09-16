
BIN   	= client
SRC 	= main.c event.c mainloop.c ntp.c audio.c log.c

CFLAGS  += -Wall -O3 -g -Werror
LDFLAGS += -g -lm

ifdef android
CROSS	= /opt/toolchains/arm-linux-androideabi-4.6/bin/arm-linux-androideabi-
CFLAGS  += -I/opt/android-ndk-r9/platforms/android-9/arch-arm/usr/include
LDFLAGS += -L/opt/android-ndk-r9/platforms/android-18/arch-arm/usr/lib/
LDFLAGS += -llog -lOpenSLES
SRC += audio-android.c
else
SRC += audio-linux.c
endif

OBJS    = $(subst .c,.o, $(SRC))
CC 	= ccache $(CROSS)gcc
LD 	= $(CROSS)gcc

.c.o:
	$(CC) $(CFLAGS) -c $<

$(BIN):	$(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

clean:	
	rm -f $(OBJS) $(BIN) core

install: $(BIN)
	adb push client /data/local/client

run: install
	adb shell /data/local/client -l3



