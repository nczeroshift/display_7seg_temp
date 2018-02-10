MCU=atmega8
CC=avr-gcc
OBJCOPY=avr-objcopy
CFLAGS=-g -std=c99 -mmcu=$(MCU) -Wall -Wstrict-prototypes -Os -mcall-prologues

all: program.hex

program.hex : program.out 
	$(OBJCOPY) -R .eeprom -O ihex program.out program.hex
program.out : main.o dht.o
	$(CC) $(CFLAGS) -o program.out -Wl,-Map,program.map main.o dht.o
main.o : main.c 
	$(CC) $(CFLAGS) -Os -c main.c
dht.o : dht.c 
	$(CC) $(CFLAGS) -Os -c dht.c

load: program.hex
	avrdude -c usbasp -p m8 -u -U flash:w:program.hex	

fusesRC:
	avrdude -c usbasp -p m8 -U lfuse:w:0xe4:m -U hfuse:w:0xd9:m # RC (8MHz)

clean:
	rm -f *.o *.map *.out *.hex

