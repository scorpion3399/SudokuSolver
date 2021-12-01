MCU=atmega328p
F_CPU=10000000
BAUD=9600

PARPORT=/dev/parport0
SERPORT=/dev/ttyS0
VERB=-v

CC=avr-gcc
OBJCOPY=avr-objcopy
CFLAGS=-std=c99 -Wall -g -Os -mmcu=${MCU} -DF_CPU=${F_CPU} -I.
TARGET=main
SRCS=main.c

all:
	${CC} ${CFLAGS} -o ${TARGET}.bin ${SRCS}
	${OBJCOPY} -j .text -j .data -O ihex ${TARGET}.bin ${TARGET}.hex

flash:
	avrdude ${VERB} ${VERB} ${VERB} ${VERB} -p ${MCU} -c arduino -b ${BAUD} -U flash:w:${TARGET}.hex:i -F -P ${SERPORT}

clean:
	rm -f *.bin *.hex
