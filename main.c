#define F_CPU 1000000UL // 1 MHz clock
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

// Common Cathode hex codes for 0-9 (and blank for 10)
unsigned char seg_7[11] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F, 0x00};

// Keypad Map (4x4)
char keypad[4][4] = {
	{'7', '8', '9', '/'},
	{'4', '5', '6', '*'},
	{'1', '2', '3', '-'},
	{'C', '0', '=', '+'}
};

volatile unsigned char digit[4] = {10, 10, 10, 10}; // 10 = Blank display initially
volatile unsigned char current_digit = 0;
int display_number = 0;

// --- Timer0 Interrupt (Refreshes display in the background) ---
ISR(TIMER0_COMP_vect) {
	// Turn off all digits first to prevent ghosting
	PORTD = 0x00;
	
	// Output the segment data for the current digit
	PORTB = seg_7[digit[current_digit]];
	
	// Turn on the specific digit selector (PD0-PD3)
	PORTD = (1 << current_digit);
	
	// Move to next digit
	current_digit++;
	if(current_digit > 3) current_digit = 0;
}

// --- Keypad Scan Function ---
char scan_keypad(void) {
	unsigned char row, col;
	for(row = 0; row < 4; row++) {
		// Ground one row at a time (PC0-PC3)
		PORTC = ~(1 << row);
		
		// Small delay for lines to settle
		_delay_us(10);
		
		// Read columns (PC4-PC7)
		for(col = 0; col < 4; col++) {
			if(!(PINC & (1 << (col + 4)))) {
				_delay_ms(20); // Debounce
				if(!(PINC & (1 << (col + 4)))) {
					while(!(PINC & (1 << (col + 4)))); // Wait for release
					return keypad[row][col];
				}
			}
		}
	}
	return 0; // No key pressed
}

void update_display(int num) {
	digit[0] = (num / 1000) % 10; // Thousands
	digit[1] = (num / 100) % 10;  // Hundreds
	digit[2] = (num / 10) % 10;   // Tens
	digit[3] = num % 10;          // Ones
}

int main(void)
{
	// Disable JTAG
	MCUCSR |= (1 << JTD);
	MCUCSR |= (1 << JTD);

	DDRB = 0xFF; DDRD = 0xFF;
	DDRC = 0x0F;
	PORTC = 0xF0;
	
	TCCR0 = (1 << WGM01) | (1 << CS01);
	OCR0 = 125;
	TIMSK = (1 << OCIE0);
	sei();

	// Calculator Variables
	unsigned long current_number = 0;
	unsigned long previous_number = 0;
	char current_operator = 0; // 0 means no operator pressed yet
	unsigned long result = 0;

	while(1)
	{
		char key = scan_keypad();
		
		if(key)
		{
			// 1. If a NUMBER is pressed
			if(key >= '0' && key <= '9') {
				current_number = (current_number * 10) + (key - '0');
				if(current_number>9999)
				current_number=current_number % 10000;
				update_display(current_number);
			}
			
			// 2. If an OPERATOR is pressed (+, -, *, /)
			else if(key == '+' || key == '-' || key == '*' || key == '/') {
				previous_number = current_number; // Save the current number
				current_number = 0;               // Clear screen for the next number
				current_operator = key;           // Remember which operator was pressed
			}
			
			// 3. If EQUALS is pressed
			else if(key == '=') {
				if(current_operator == '+') {
					result = previous_number + current_number;
				}
				else if(current_operator == '-') {
					result = previous_number - current_number;
				}
				else if(current_operator == '*') {
					result = previous_number * current_number;
				}
				else if(current_operator == '/') {
					if(current_number != 0) {
						result = previous_number / current_number;
						} else {
						result = 9999; // Error: Cannot divide by zero
					}
					
				}
				if(result>9999)
				result=result % 10000;
				// Show the result and reset for the next calculation
				update_display(result);
				current_number = result;
				current_operator = 0;
			}
			
			// 4. If CLEAR is pressed
			else if(key == 'C') {
				current_number = 0;
				previous_number = 0;
				current_operator = 0;
				result = 0;
				digit[0] = 10; digit[1] = 10; digit[2] = 10; digit[3] = 10; // Blank screen
			}
		}
	}
	return 0;
}