
/**
 * Small temperature/humidity display
 * with 7-segment display and multiplexing
 * https://github.com/nczeroshift/display_7seg_temp
 */

#define REFRESH_TIME 1000000L
#define REFRESH_TIME_NEXT REFRESH_TIME-1000L
#define F_CPU 8000000
#define BAUD 9600

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <avr/io.h>
#include <avr/interrupt.h>

#include <util/delay.h> 
#include <util/setbaud.h>

#include <compat/twi.h>

#include "dht.h"

volatile uint8_t char_id = 0;
volatile uint8_t digit_id = 0;
volatile uint64_t time = 0;
char text_buffer[34];

// PD2 - Top
// PC1 - Top Left
// PB5 - Top Right
// PD6 - Middle
// PD7 - Bottom Left
// PD6 - Bottom Right
// PB0 - Down
// PB3 - Dot

unsigned char map[13] = {
    0b00000000,
    0b01101111, // 0
    0b00001100, // 1
    0b01110101, // 2
    0b01011101, // 3
    0b00011110, // 4
    0b01011011, // 5
    0b01111011, // 6
    0b00001101, // 7
    0b01111111, // 8
    0b00011111, // 9
    0b10000000, // .
    0b00010000, // -
};

void setChar(unsigned char code){
    unsigned mask = code;

    if(mask & 0b00000001)
        PORTD &= ~(1<<PD2);
    else
        PORTD |= (1<<PD2);

    if(mask & 0b00000010)
        PORTC &= ~(1<<PC1);
    else
        PORTC |= (1<<PC1);

    if(mask & 0b00000100)
        PORTB &= ~(1<<PB5);
    else
        PORTB |= (1<<PB5);

    if(mask & 0b00001000)
        PORTB &= ~(1<<PB4);
    else
        PORTB |= (1<<PB4);

    if(mask & 0b00010000)
        PORTD &= ~(1<<PD6);
    else
        PORTD |= (1<<PD6);

    if(mask & 0b00100000)
        PORTD &= ~(1<<PD7);
    else
        PORTD |= (1<<PD7);

    if(mask & 0b01000000)
        PORTB &= ~(1<<PB0);
    else
        PORTB |= (1<<PB0);

    if(mask & 0b10000000)
        PORTB &= ~(1<<PB3);
    else
        PORTB |= (1<<PB3);
}

// PD4 - Display 8
// PB6 - Display 7
// PB7 - Display 6
// PD5 - Display 5
// PD0 - Display 4
// PC3 - Display 3
// PD1 - Display 2
// PC2 - Display 1

void setDisplay(unsigned char mask){
    if(mask & 0b00000001)
        PORTC |= (1<<PC2);
    else
        PORTC &= ~(1<<PC2);

    if(mask & 0b00000010)
        PORTD |= (1<<PD1);
    else
        PORTD &= ~(1<<PD1);

    if(mask & 0b00000100)
        PORTC |= (1<<PC3);
    else
        PORTC &= ~(1<<PC3);

    if(mask & 0b00001000)
        PORTD |= (1<<PD0);
    else
        PORTD &= ~(1<<PD0);

    if(mask & 0b00010000)
        PORTD |= (1<<PD5);
    else
        PORTD &= ~(1<<PD5);

    if(mask & 0b00100000)
        PORTB |= (1<<PB7);
    else
        PORTB &= ~(1<<PB7);

    if(mask & 0b01000000)
        PORTB |= (1<<PB6);
    else
        PORTB &= ~(1<<PB6);

    if(mask & 0b10000000)
        PORTD |= (1<<PD4);
    else
        PORTD &= ~(1<<PD4);
}

void TIMER0_Init(void){
    TCCR0 = (1 << CS01)  ; // 1024 CPU divisor
    TIMSK |= (1 << TOIE0); // Enable overflow interrupt
    TCNT0 = 0; // Start counting from 0
}


ISR(TIMER0_OVF_vect){
    const char ch = text_buffer[char_id];

    setChar(0);
    setDisplay(1 << digit_id);

    uint8_t mapped_digit = 0;

    if(ch >= '0' && ch <= '9')
        mapped_digit = map[1 + ch -'0'];
    else if(ch == '-') 
        mapped_digit = map[12];

    if(text_buffer[char_id+1] == '.'){
        mapped_digit |= 0b10000000;
    }
    else{
        digit_id++;
        if(digit_id > 7)
            digit_id = 0;
    }

    setChar(mapped_digit);
    char_id++;

    if(text_buffer[char_id] == '\0'){
        char_id = 0;
        digit_id=0;
    }

    TCNT0 = 0; // Restart counting from 0(it's unnecessary due to overflow)    
    time++;
}


void resetIO(void){
    DDRB |= (1<<PB0) | (1<<PB1) | (1<<PB2) | (1<<PB3) | (1<<PB4) | (1<<PB5) | (1<<PB6) | (1<<PB7);
    DDRD |= (1<<PD0) | (1<<PD1) | (1<<PD2) | (1<<PD3) | (1<<PD4) | (1<<PD5) | (1<<PD6) | (1<<PD7);
    DDRC |= (1<<PC0) | (1<<PC1) | (1<<PC2) | (1<<PC3);
}

void printVal(float value, char * buffer){
    int v_int = (int)value;
    int v_dec = (value - v_int)*100;
    sprintf(buffer,"%02d.%02d",v_int,v_dec);
    buffer[5] = '\0';
}


int main(void)
{
    float temp = 0;
    float hum = 0;

    dht_gettemperaturehumidity(&temp, &hum);

    resetIO();
    TIMER0_Init();
    sei();

    // Test all segments.
    strcpy(text_buffer,"8.8.8.8.8.8.8.8.");    

    time = REFRESH_TIME_NEXT;

    while(1){
        if((time++) > REFRESH_TIME){
            cli();    

            memset(text_buffer, 0, 32); 

            int ret = dht_gettemperaturehumidity(&temp, &hum);

            if(ret < 0){
                sprintf(text_buffer,"%d",ret);
                time = REFRESH_TIME_NEXT;
            }else{
                printVal(temp,text_buffer);
                printVal(hum,text_buffer+5);
                time = 0;        
            }      

            resetIO();    
            digit_id = 0;
            char_id = 0;

            sei();
        }
    }

    return 0;
}
