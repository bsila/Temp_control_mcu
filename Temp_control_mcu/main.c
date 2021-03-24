/*
 * Temp_control_mcu.c
 *
 * Created: 23.3.2021. 22:44:18
 * Author : Luka Županoviæ, Vedran Matiæ, Borna Sila
 * Version: 0.1
 */ 
#define F_CPU 7372800UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>

#include "lcd.h"

static uint8_t tm = 0;
static uint8_t temp = 0;

static uint8_t subMenu = 0;

const char *mode[4];
const char *menu[4];
const char *variables[2];

static uint8_t max_temp = 0;
static uint8_t min_temp = 0;

static uint8_t fMode = 0;
static uint8_t mMode = 0;
static uint8_t mVar = 1;
static uint8_t mSelect = 0;


void showTemperature() {
	char tmp[3];
	tmp[0] = ' ' + temp / 10;
	tmp[1] = '0' + temp % 10;
	tmp[2] = '\0';
	lcd_puts("Temperatura: ");
	lcd_puts(tmp);
	lcd_gotoxy(0, 1);
	lcd_puts("Mode: ");
	lcd_puts(mode[mSelect]);
}

// Starting message
void showMsg() {
	lcd_clrscr();
	lcd_gotoxy(3, 0);
	lcd_puts_P("Welcome to");
	lcd_gotoxy(1, 1);
	lcd_puts_P("temp. control");
}

void showMenu() {
	lcd_putc('<');
	
	if (!subMenu){
		switch (mMode) {
			case 0:
			lcd_gotoxy((16 - strlen(menu[mMode])) / 2, 0);
			lcd_puts(menu[mMode]);
			break;
			case 1:
			lcd_gotoxy((16 - strlen(menu[mMode])) / 2, 0);
			lcd_puts(menu[mMode]);
			break;
			case 2:
			lcd_gotoxy((16 - strlen(menu[mMode])) / 2, 0);
			lcd_puts(menu[mMode]);
			break;
			case 3:
			lcd_gotoxy((16 - strlen(menu[mMode])) / 2, 0);
			lcd_puts(menu[mMode]);
			break;
		}
	}else {
		switch (mVar) {
			case 1:
			lcd_gotoxy((16 - strlen(variables[mVar - 1])) / 2, 0);
			lcd_puts(variables[mVar - 1]);
			
			char tmp[3];
			tmp[0] = '0' + max_temp / 10;
			tmp[1] = '0' + max_temp % 10;
			tmp[2] = '\0';
			
			if (!mSelect) {
				lcd_gotoxy((16 - strlen(tmp)) / 2, 1);
				lcd_puts(tmp);
			} else {
				lcd_gotoxy((15 - strlen(tmp)) / 2, 1);
				lcd_putc('<');
				lcd_puts(tmp);
				lcd_putc('>');
			}
			break;
			case 2:
			lcd_gotoxy((16 - strlen(variables[mVar - 1])) / 2, 0);
			lcd_puts(variables[mVar - 1]);
			
			//char tmp[3];
			tmp[0] = '0' + min_temp / 10;
			tmp[1] = '0' + min_temp % 10;
			tmp[2] = '\0';
			
			if (!mSelect) {
				lcd_gotoxy((16 - strlen(tmp)) / 2, 1);
				lcd_puts(tmp);
				} else {
				lcd_gotoxy((15 - strlen(tmp)) / 2, 1);
				lcd_putc('<');
				lcd_puts(tmp);
				lcd_putc('>');
			}
			break;
		}
		
	}
	lcd_gotoxy(15, 0);
	lcd_putc('>');
}

void writeOnLCD() {
	lcd_clrscr();

	if (!fMode) {
		showMsg();
	} else if (fMode == 1) {
		showTemperature();
	} else {
		showMenu();	
	}
}

ISR(TIMER0_COMP_vect) {
	tm++;

	if (tm == 100) {
		tm = 0;
		writeOnLCD();
	}
}

void nonBlockingDebounce() {
	GICR &= ~_BV(INT0);
	sei();

	_delay_ms(500);
	GIFR = _BV(INTF0);
	GICR |= _BV(INT0);

	cli();
}

ISR(INT0_vect) {
	//fMode 0 is only at the start
	fMode = 1 + (fMode % 2);

	writeOnLCD();

	nonBlockingDebounce();
}

int main(void)
{
	// Setting menu items
	menu[0] = "Variables";
	menu[1] = "Modes";
	menu[2] = "Test_menu";
	menu[3] = "Test_menu.";
	
	// Setting variables names
	variables[0] = "max_temp";
	variables[1] = "min_temp";
	
	// Setting modes
	mode[0] = "heating";
	mode[1] = "cooling";
	mode[2] = "balance";
	mode[3] = "test3";
	
	// First time temp variables are same as measured temp
	max_temp = min_temp = temp;

	DDRA = _BV(5) | _BV(6) | _BV(7);
	PORTB = _BV(0) | _BV(1) | _BV(2);
	DDRB = 0;

	DDRD = _BV(4);

	TCCR1A = _BV(COM1B1) | _BV(WGM10);
	TCCR1B = _BV(WGM12) | _BV(CS11);
	OCR1B = 128;

	TCCR0 = _BV(WGM01) | _BV(CS02) | _BV(CS00);
	OCR0 = 72;

	TIMSK = _BV(OCIE0);

	MCUCR = _BV(ISC01);
	GICR = _BV(INT0);
	sei();

	lcd_init(LCD_DISP_ON);
	lcd_clrscr();

	writeOnLCD();

	while (1) {
		
		// Using keys (PORTB) to control
		if (bit_is_clear(PINB, 0)) {
			switch (fMode) {
				case 1:
				
				break;
				case 2:
					if (!subMenu) {
						mMode = (mMode + 1) % 4;
					} else if (!mSelect) {
						mVar = 1 + (mVar % 2);
					} else {
						switch (mVar) {
							case 1:
								max_temp += 1;
								if (max_temp > 99) max_temp = 0;
							break;
							case 2:
								min_temp += 1;
								if (min_temp > 99) min_temp = 0;
							break;
						}	
					}
				
				break;
				case 3:
				
				break;
			}
		} else if (bit_is_clear(PINB, 1)) {
			switch (fMode) {
				case 1:
				
				break;
				case 2:
					if (!subMenu) {
						subMenu = 1;
					} else if (!mSelect) {
						mSelect = mVar;
					} else {
						switch (mVar) {
							case 1:
								if (max_temp <= 0) max_temp = 100;
								max_temp -= 1;
							break;
							case 2:
								if (min_temp <= 0) min_temp = 100;
								min_temp -= 1;
							break;
						}
					}
				
				break;
				case 3:
				
				break;
			}
		} else if (bit_is_clear(PINB, 2)) {
			switch (fMode) {
				case 2:
					if (mSelect){
						mSelect = 0;
					} else if (subMenu){
						subMenu = 0;
					}
				break;
			}
		}

		_delay_ms(200);
	}
}


