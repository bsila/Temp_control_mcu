# Temp_control_mcu

### Idea

The idea is to write a program for atmega16 that will be able to control heating system. The main goal is to have a temperature sensor that will read temperature (tmp35 is used) and a menu with different modes. Modes can be changed in menu implemented on AVR. Depending on the mode, AVR can either turn on a fan (controlled with mini dc motor) or heater (0,75 A light bulb).

---

### States

##### 0 - hello
	Basic starting state
	
##### 1 - temperature display
	This state is showing temperature on LCD display
	
##### 2 - menu
	Menu state is used for configuring modes
	- variables
	- modes
	- alarms
	
##### 3 - set password
	After start password for accessing menu must be set
	If it is not set (deafult is '0000'), password will not be used

##### 4 - check password
	Password is needed to get access to menu (only if password is used)
---	

### Modes
- 0 : heating
- 1 : cooling
- 2 : balance

---

### Controls

- int0 -> change states
- key1 -> next/increase value (in menu)
- key2 -> down/select submenu/decrease value (in menu)
- key3 -> confirm change/up (in menu)

---

### Variables

- max temp -> max value that can be added to the set
- min temp -> min value that can be added to the set
- set -> temperature set point
- temp diff -> temperature difference from set, for starting heating/cooling

---

### Alarms

- alarm diff -> temperature difference from set, for starting alarm
- alarm high -> upper temperature limit for triggering alarm
- alarm low -> lower temperature limit for triggering alarm
- alarm usage -> alarm usage (on/off)
- lock usage -> lock usage (on/off), lock menu access when heating/cooling




