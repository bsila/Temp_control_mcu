/*
 * Temp_control_mcu.c
 *
 * Created: 23.3.2021. 22:44:18
 * Author : Luka Županoviæ, Vedran Matiæ, Borna Sila
 * Version: 0.9
 */ 
#define F_CPU 7372800UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <stdlib.h>

#include "lcd.h"

/*
** Global variables
*/
static uint8_t temp = 0;		// current temperature
static uint8_t pswSet = 0;
static uint8_t pswUse = 0;
static uint8_t mAccess = 0;
static uint8_t pswError = 0;
static uint8_t update = 0;		// update after menu
static uint8_t lock = 0;		// lock menu access 
static char password[4];
static char tmpPassword[4];

// Names
const char *mode[4];
const char *menu[4];
const char *variables[6];
const char *alarms[5];

// Variables and alarms
static uint8_t var_mat[6];
static uint8_t alarms_mat[5];

// Modes/menu
static uint8_t dMode = 0;		// display mode
static uint8_t mMode = 0;		// menu mode
static uint8_t mVar = 0;		// menu variables
static uint8_t mSelect = 0;		// menu select flag
static uint8_t modeSelect = 0;	// working mode
static uint8_t subMenu = 0;		// sub menu flag


// Moving average constants
#define TOT_SAMPLES 32
#define MOVAVG_SHIFT 5
#define SUM_DIFF_THOLD TOT_SAMPLES/2    // 1/2LSB

// Moving average structure
typedef struct{
	int8_t    samIdx;
	uint32_t sum;
	uint16_t samples[TOT_SAMPLES];
}movAvg_t;

volatile uint8_t updateLCD;
uint16_t curAvg;
uint8_t curTemp;
uint8_t halfCelsius;

/*
** Functions
*/
void showTemperature();
void showMsg();
void showMenu();

void resetPsw(char *tmpPsw);
void setPsw();
void enterPsw();
uint8_t checkPsw(const char *toCheck);

uint16_t getMovAvg(uint16_t, movAvg_t *);
uint16_t readAdc(uint8_t);

void init_temp_ma(movAvg_t *, int8_t);
void init_adc();
void init_spec_char();
void nonBlockingDebounce();
void writeOnLCD();

int main(void)
{
	// Setting menu items
	menu[0] = "Variables";
	menu[1] = "Modes";
	menu[2] = "Alarm";
	
	// Setting variables names
	variables[0] = "max temp";
	variables[1] = "min temp";
	variables[2] = "set temp";
	variables[3] = "temp diff";
	variables[4] = "on time";
	variables[5] = "off time";
	
	// Setting alarm names
	alarms[0] = "alarm diff";
	alarms[1] = "alarm high";
	alarms[2] = "alarm low";
	alarms[3] = "alarm usage";
	alarms[4] = "lock usage";
	
	// Setting modes
	mode[0] = "heat";
	mode[1] = "cool";
	mode[2] = "bal ";
	
	// Initialize password to '0000'
	resetPsw(tmpPassword);
	resetPsw(password);
	
	// Initializing default variables
	var_mat[0] = 99;
	var_mat[1] = 0;
	var_mat[2] = 0;
	var_mat[3] = 2;
	var_mat[4] = 0;
	var_mat[5] = 1;
	
	// Initializing default alarm
	alarms_mat[0] = 2;
	alarms_mat[1] = 50;
	alarms_mat[2] = 0;
	alarms_mat[3] = 0;
	alarms_mat[4] = 0;

	// PORT/DDR/registers setup
	DDRA = _BV(1) | _BV(2) | _BV(3);
	PORTA = 0x00;

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
	
	// Initialize LCD and custom characters
	lcd_init(LCD_DISP_ON);
	init_spec_char();
	lcd_clrscr();

	writeOnLCD();
	
	uint16_t tmp;
	uint32_t lastDisplayedSum = 0;
	movAvg_t movingAverage;
	
	// Initialize moving average structure
	init_temp_ma(&movingAverage, TOT_SAMPLES);
	
	// Initialize ADC
	init_adc();
	
	sei();

	while (1) {
		tmp = readAdc(0);
		curAvg = getMovAvg(tmp, &movingAverage);
		
		if(abs(lastDisplayedSum - movingAverage.sum) > SUM_DIFF_THOLD ) {
			lastDisplayedSum = movingAverage.sum;
			updateLCD = 1;
			update = 1;
		}
		
		// update after change
		if (update){
			update = 0;
			uint16_t diff = abs(var_mat[2] - temp);
			
			// modes update
			if (diff > var_mat[3]){
				switch (modeSelect) {
					case 0:
					if (temp > var_mat[2]) {
						PORTA &= _BV(0);
						lock = 0;
						} else {
						PORTA |=  _BV(1);
						lock = 1;
					}
					break;
					case 1:
					if (temp < var_mat[2]) {
						PORTA &= _BV(0);
						lock = 0;
						} else {
						PORTA |=  _BV(2);
						lock = 1;
					}
					break;
					case 2:
					lock = 1;
					if (temp < var_mat[2]) {
						PORTA |=  _BV(1);
					} else PORTA |=  _BV(2);
					break;
				}
				} else {
				PORTA &= _BV(0) | _BV(3);
				lock = 0;
			}
			
			// alarm update
			if (alarms_mat[3]){
				if (diff > alarms_mat[0] || temp > alarms_mat[1] || temp < alarms_mat[2]){
					PORTA |= _BV(3);
				} else PORTA &= ~_BV(3);
			}
		}
		
		// Using keys (PORTB) to control
		if (bit_is_clear(PINB, 0)) {
			switch (dMode) {
				case 1:
				// key1 function on temp display screen
				break;
				case 2:
				if (!subMenu) {
					// switch between sub menus
					mMode = (mMode + 1) % 3;
					} else if (!mSelect) {
					// change sub menu items 0 = var, 1 = mode, 2 = alarm
					mVar = (mVar + 1) % (mMode == 0 ? 6 :  mMode == 1 ? 3 : 5);
					// mode changes directly
					if (mMode == 1) modeSelect = mVar;
					
					// variable setup
					} else if (mMode == 0) {
					var_mat[mVar] += 1;
					switch (mVar) {
						case 0:
						if (var_mat[mVar] > 99) var_mat[mVar] = var_mat[1] + 1;
						if (var_mat[2] > var_mat[mVar]) var_mat[2] = var_mat[mVar];
						break;
						case 1:
						if (var_mat[mVar] >= var_mat[0]) var_mat[mVar] = 0;
						if (var_mat[2] < var_mat[mVar]) var_mat[2] = var_mat[mVar];
						break;
						case 2:
						if (var_mat[mVar] > var_mat[0]) var_mat[mVar] = var_mat[1];
						break;
						case 3:
						if (var_mat[mVar] > 30) var_mat[mVar] = 0;
						break;
						case 4:
						if (var_mat[mVar] > 250) var_mat[mVar] = 0;
						break;
						case 5:
						if (var_mat[mVar] > 250) var_mat[mVar] = 0;
						break;
					}
					
					// alarm setup
					} else if (mMode == 2) {
					alarms_mat[mVar] += 1;
					switch (mVar) {
						case 0:
						if (alarms_mat[mVar] > 50) alarms_mat[mVar] = 1;
						break;
						case 1:
						if (alarms_mat[mVar] > 99) alarms_mat[mVar] = alarms_mat[2] + 1;
						break;
						case 2:
						if (alarms_mat[mVar] >= alarms_mat[1]) alarms_mat[mVar] = 0;
						break;
						case 3:
						alarms_mat[mVar] = alarms_mat[mVar] % 2;
						break;
						case 4:
						alarms_mat[mVar] = alarms_mat[mVar] % 2;
						break;
					}
				}
				break;
				case 3:
				if (!mSelect) {
					mVar = (mVar + 1) % 4;
					} else {
					password[mVar] += 1;
				}
				break;
				case 4:
				if (!mSelect) {
					mVar = (mVar + 1) % 4;
					} else {
					tmpPassword[mVar] += 1;
				}
				break;
			}
			} else if (bit_is_clear(PINB, 1)) {
			switch (dMode) {
				case 1:
				// // key2 function on temp display screen
				break;
				case 2:
				if (!subMenu) {
					subMenu = 1;
					mVar = mMode == 1 ? modeSelect : 0;
					} else if (!mSelect) {
					mSelect = 1;
					
					// variable setup
					} else if (mMode == 0) {
					switch (mVar) {
						case 0:
						if (var_mat[mVar] <= var_mat[1]) var_mat[mVar] = 100;
						if (var_mat[2] > var_mat[mVar]) var_mat[2] = var_mat[mVar] - 1;
						break;
						case 1:
						if (var_mat[mVar] <= 0) var_mat[mVar] = var_mat[0];
						if (var_mat[2] > var_mat[mVar]) var_mat[2] = var_mat[mVar] - 1;
						break;
						case 2:
						if (var_mat[mVar] <= var_mat[1]) var_mat[mVar] = var_mat[0] + 1;
						break;
						case 3:
						if (var_mat[mVar] <= 0) var_mat[mVar] = 31;
						break;
						case 4:
						if (var_mat[mVar] <= 0) var_mat[mVar] = 251;
						break;
						case 5:
						if (var_mat[mVar] <= 0) var_mat[mVar] = 251;
						break;
					}
					var_mat[mVar] -= 1;
					
					// alarm setup
					} else if (mMode == 2) {
					switch (mVar) {
						case 0:
						if (alarms_mat[mVar] <= 1) alarms_mat[mVar] = 51;
						break;
						case 1:
						if (alarms_mat[mVar] <= 0) alarms_mat[mVar] = 100;
						break;
						case 2:
						if (alarms_mat[mVar] <= 0) alarms_mat[mVar] = 100;
						break;
						case 3:
						alarms_mat[mVar] = (alarms_mat[mVar] + 1) % 2 + 1;
						break;
						case 4:
						alarms_mat[mVar] = (alarms_mat[mVar] + 1) % 2 + 1;
						break;
					}
					alarms_mat[mVar] -= 1;
				}
				break;
				case 3:
				if (!mSelect) {
					mSelect = 1;
					} else {
					password[mVar] -= 1;
				}
				break;
				case 4:
				if (!mSelect) {
					mSelect = 1;
					} else {
					tmpPassword[mVar] -= 1;
				}
				break;
			}
			} else if (bit_is_clear(PINB, 2)) {
			switch (dMode) {
				case 2:
				if (mSelect){
					mSelect = 0;
					} else if (subMenu){
					subMenu = 0;
				}
				break;
				case 3:
				if (!mSelect) {
					pswSet = 1;
					pswUse = !checkPsw("0000");
					mAccess = !pswUse;
					mVar = 0;
					} else {
					mSelect = 0;
				}
				break;
				case 4:
				if (mSelect) {
					mSelect = 0;
					} else if (checkPsw(tmpPassword)) {
					mAccess = 1;
					dMode = 2;
					mVar = 1;
					resetPsw(tmpPassword);
					} else {
					pswError = 1;
					mAccess = 0;
					mVar = 0;
					resetPsw(tmpPassword);
				}
				break;
			}
		}

		_delay_ms(200);
	}
}

/*
** ISR
*/

ISR(TIMER0_COMP_vect) {
	writeOnLCD();
	
	if(updateLCD == 1) {
		uint32_t temperature;

		temperature = curAvg << 8;
		temperature >>= 9;
		halfCelsius = temperature & 1;
		temperature >>= 1;
		temp = temperature;
		updateLCD = 0;
	}
}

ISR(INT0_vect) {
	
	switch (dMode) {
		// dMode 0 is only at the start
		// Set up password
		case 0:
		dMode = 3;
		break;
		
		// Switch between main and menu display
		case 1:
		if (alarms_mat[4] & lock) break;
		dMode = !mAccess ? 4 : 2;
		break;
		case 2:
		dMode = 1;
		mAccess = !pswUse;
		mMode = 0;
		mSelect = 0;
		subMenu = 0;
		update = 1;
		break;
		
		// After password go to main display
		case 3:
		if (pswSet) dMode = 1;
		break;
		
		// Exit error screen
		case 4:
		pswError = 0;
		dMode = 1;
		break;
	}

	writeOnLCD();

	nonBlockingDebounce();
}

/*
** Display functions
*/

// Main display
void showTemperature() {
	lcd_clrscr();

	char adcStr[16];
	itoa(temp, adcStr, 10);
	
	lcd_puts("Temp: ");
	lcd_puts(adcStr);
	lcd_putc('.');
	halfCelsius ? lcd_putc('5') : lcd_putc('0');
	lcd_putc(223);        //degree symbol
	lcd_puts("C  ");
	lcd_gotoxy(0, 1);
	lcd_puts("Mode: ");
	lcd_puts(mode[modeSelect]);
	lcd_gotoxy(11, 1);
	if (alarms_mat[4]) lcd_putc(0); // lock icon
	lcd_gotoxy(13, 1);
	if (alarms_mat[3]) lcd_putc(1); // bell icon
}

// Starting message
void showMsg() {
	lcd_clrscr();
	lcd_gotoxy(3, 0);
	lcd_puts("Welcome to");
	lcd_gotoxy(1, 1);
	lcd_puts("temp. control");
}

// Menu display
void showMenu() {
	lcd_clrscr();
	lcd_putc('<');
	
	// Menu items
	if (!subMenu){
		lcd_gotoxy((16 - strlen(menu[mMode])) / 2, 0);
		lcd_puts(menu[mMode]);
		
	// 'Variables' subMenu items
	} else if (mMode == 0) {
		lcd_gotoxy((16 - strlen(variables[mVar])) / 2, 0);
		lcd_puts(variables[mVar]);
		char buffer[4];
		
		if (!mSelect) {
			lcd_gotoxy(6, 1);
			if (mVar == 0 || mVar == 1 || mVar == 2) {
				lcd_puts(itoa(var_mat[mVar], buffer, 10));
				lcd_putc(223);
				lcd_putc('C');
			} else {
				lcd_putc(' ');
				lcd_puts(itoa(var_mat[mVar], buffer, 10));
				lcd_putc(' ');
			}
		} else {
			lcd_gotoxy(5, 1);
			lcd_putc('<');
			if (mVar == 0 || mVar == 1 || mVar == 2) {
				lcd_puts(itoa(var_mat[mVar], buffer, 10));
				lcd_putc(223);
				lcd_putc('C');
			} else {
				lcd_putc(' ');
				lcd_puts(itoa(var_mat[mVar], buffer, 10));
				lcd_putc(' ');
			}
			lcd_putc('>');
		}
		
	// 'Modes' subMenu items
	} else if (mMode == 1) {
		lcd_gotoxy(5, 0);
		lcd_puts("Mode:");
		lcd_gotoxy((14 - strlen(mode[mVar])) / 2, 1);
		lcd_putc('<');
		lcd_puts(mode[mVar]);
		lcd_putc('>');
		
	// 'Alarms' subMenu items
	} else {
		lcd_gotoxy((16 - strlen(alarms[mVar])) / 2, 0);
		lcd_puts(alarms[mVar]);
		char buffer[4];
		
		if (!mSelect) {
			lcd_gotoxy(6, 1);
			if (mVar == 1 || mVar == 2) {
				lcd_puts(itoa(alarms_mat[mVar], buffer, 10));
				lcd_putc(223);
				lcd_putc('C');
			} else {
				lcd_putc(' ');
				lcd_puts(itoa(alarms_mat[mVar], buffer, 10));
				lcd_putc(' ');
			}
		} else {
			lcd_gotoxy(5, 1);
			lcd_putc('<');
			if (mVar == 1 || mVar == 2) {
				lcd_puts(itoa(alarms_mat[mVar], buffer, 10));
				lcd_putc(223);
				lcd_putc('C');
			} else {
				lcd_putc(' ');
				lcd_puts(itoa(alarms_mat[mVar], buffer, 10));
				lcd_putc(' ');
			}
			lcd_putc('>');
		}
	}
	
	lcd_gotoxy(15, 0);
	lcd_putc('>');
}

/*
** Password functions
*/

void resetPsw(char *tmpPsw){
	for (uint8_t i = 0; i < 4; i++){
		tmpPsw[i] = '0';
	}
}

void setPsw() {
	if (!pswSet) {
		lcd_clrscr();
		lcd_gotoxy(1, 0);
		lcd_puts("Set password:");
		lcd_gotoxy(4, 1);
		
		for (uint8_t i = 0; i < 4; i++){
			if (mVar == i) {
				lcd_putc(mSelect ? '<' : ' ');
				lcd_putc(password[i]);
				lcd_putc(mSelect ? '>' : ' ');
			} else lcd_putc(password[i]);
		}
	} else if (pswUse) {
		lcd_clrscr();
		lcd_gotoxy(2, 0);
		lcd_puts("Password set");
		lcd_gotoxy(4, 1);
		lcd_puts("->");
		for (uint8_t i = 0; i < 4; i++){
			lcd_putc(password[i]);
		}
		lcd_puts("<-");
	} else {
		lcd_clrscr();
		lcd_gotoxy(2, 0);
		lcd_puts("Password not");
		lcd_gotoxy(6, 1);
		lcd_puts("used");
	}
}

void enterPsw() {
	if (!pswError) {
		lcd_clrscr();
		lcd_puts("Enter password:");
		lcd_gotoxy(4, 1);
		
		for (uint8_t i = 0; i < 4; i++){
			if (mVar == i) {
				lcd_putc(mSelect ? '<' : ' ');
				lcd_putc(tmpPassword[i]);
				lcd_putc(mSelect ? '>' : ' ');
			} else lcd_putc(tmpPassword[i]);
		}
	} else {
		lcd_clrscr();
		lcd_gotoxy(3, 0);
		lcd_puts("Incorrect");
		lcd_gotoxy(4, 1);
		lcd_puts("password");
	}
}

uint8_t checkPsw(const char *toCheck) {
	for (uint8_t i = 0; i < 4; i++) {
		if (toCheck[i] != password[i]) return 0;
	}
	return 1;
}

/*
** ADC and moving average functions
*/

// Calculate moving average
uint16_t getMovAvg(uint16_t newSample, movAvg_t *ma)
{
	// Remove oldest sample from the sum
	ma->sum -= ma->samples[ma->samIdx];
	// Add the new sample to the sum and to samples array
	ma->sum += newSample;
	ma->samples[ma->samIdx] = newSample;
	// Increment index and roll down to 0 if necessary
	ma->samIdx++;
	if( ma->samIdx == TOT_SAMPLES ){
		ma->samIdx = 0;
	}

	// return moving average - divide the sum by 2^MOVAVG_SHIFT
	return ma->sum >> MOVAVG_SHIFT;
}

// Read ADC value
uint16_t readAdc(uint8_t channel)
{
	//choose channel
	ADMUX &= ~(0x7);
	ADMUX |= channel;
	
	//start conversion
	ADCSRA |= _BV(ADSC);

	//wait until conversion completes
	while (ADCSRA & _BV(ADSC) );
	
	return ADCW;
}

/*
** Init and general functions
*/

// Initialize moving average structure
void init_temp_ma(movAvg_t *ma, int8_t totSamples)
{
	int i;
	
	ma->samIdx = 0;
	ma->sum = 0;
	for(i=0; i<totSamples; i++){
		ma->samples[i] = 0;
	}
}

void init_adc()
{
	//adc enable, prescaler=64 -> clk=115200
	ADCSRA = _BV(ADEN)|_BV(ADPS2)|_BV(ADPS1);
	//2.56V reference voltage
	ADMUX = _BV(REFS0) | _BV(REFS1);
}

void init_spec_char(){
	lcd_command(0x40);	// set CGRAM address for first character
	
	// lock icon
	lcd_data(0x0e); 
	lcd_data(0x11); 
	lcd_data(0x11);
	lcd_data(0x1f);
	lcd_data(0x1b);
	lcd_data(0x1f);
	lcd_data(0x1f);
	lcd_data(0x00); 
	lcd_putc(0);
	
	lcd_command(0x48);	// set CGRAM address for second character
	
	// bell icon
	lcd_data(0x00); 
	lcd_data(0x04); 
	lcd_data(0x0e);
	lcd_data(0x0e);
	lcd_data(0x0e);
	lcd_data(0x1f);
	lcd_data(0x04);
	lcd_data(0x00);
	lcd_putc(1);
}

void nonBlockingDebounce() {
	GICR &= ~_BV(INT0);
	sei();

	_delay_ms(500);
	GIFR = _BV(INTF0);
	GICR |= _BV(INT0);

	cli();
}

void writeOnLCD() {
	lcd_clrscr();
	
	switch (dMode){
		case 0:
		showMsg();
		break;
		case 1:
		showTemperature();
		break;
		case 2:
		showMenu();
		break;
		case 3:
		setPsw();
		break;
		case 4:
		enterPsw();
		break;
	}
}


