MCU=atmega328p
MCU2=m328p
F_CPU=1843200UL
BAUD=115200 # NOT the program's BAUD rate

AVRDUDE=/home/panos/Downloads/arduino-1.8.16-linux64/arduino-1.8.16/hardware/tools/avr/bin/avrdude
# AVRDUDE=avrdude
PROGRAMMER=arduino
PORT=/dev/ttyACM0
# PARPORT=/dev/parport0
# SERPORT=/dev/ttyS0
CONFIG=/home/panos/Downloads/arduino-1.8.16-linux64/arduino-1.8.16/hardware/tools/avr/etc/avrdude.conf
# CONFIG=/home/panos/.avrduderc
# CONFIG=./avrdude.conf
VERB=-v

CC=avr-gcc
OBJCOPY=avr-objcopy
CFLAGS=-std=c99 -Wall -g -O3 -mmcu=${MCU} -D F_CPU=${F_CPU} -I.
TARGET=main
SRCS=main.c

all:
	${CC} ${CFLAGS} -o ${TARGET}.bin ${SRCS}
	${OBJCOPY} -j .text -j .data -O ihex ${TARGET}.bin ${TARGET}.hex

flash:
	${AVRDUDE} -C ${CONFIG} ${VERB} -p ${MCU} -c ${PROGRAMMER} -P ${PORT} -b ${BAUD} -D -U flash:w:${TARGET}.hex:i 

clean:
	rm -f *.bin *.hex