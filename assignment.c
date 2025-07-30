#include <LPC21xx.h>
#include <stdio.h>

#define AD0CR  (*((volatile unsigned int *)0xE0034000))
#define AD0GDR (*((volatile unsigned int *)0xE0034004))

// Pin definitions
#define HEATER_PIN (1 << 16)   // P0.16 for Heater LED
#define BUZZER_PIN (1 << 17)   // P0.17 for Buzzer

// Temperature thresholds
#define HEATING_TEMP_LOW   30
#define HEATING_TEMP_HIGH  60
#define OVERHEAT_TEMP      75

// Delay function (approx 1ms for 15MHz PCLK)
void delay_ms(unsigned int ms) {
    for (unsigned int i = 0; i < ms * 6000; i++);
}

// UART0 Initialization (9600 baud)
void uart0_init(void) {
    PINSEL0 |= 0x00000005;     // P0.0 = TxD0, P0.1 = RxD0
    U0LCR = 0x83;              // 8-bit, 1 stop, no parity, DLAB=1
    U0DLL = 97;                // For 9600 baud @ 15MHz PCLK
    U0DLM = 0;
    U0LCR = 0x03;              // DLAB=0
}

// Transmit character via UART0
void uart0_tx(char ch) {
    while (!(U0LSR & 0x20));   // Wait until THR empty
    U0THR = ch;
}

// Transmit string via UART0
void uart0_print(char *str) {
    while (*str) {
        uart0_tx(*str++);
    }
}

// Initialize ADC0 for channel AD0.1 (P0.28)
void adc_init(void) {
    PINSEL1 &= ~(3 << 24);     // Clear bits for P0.28
    PINSEL1 |=  (1 << 24);     // Set P0.28 to AD0.1
    AD0CR = (1 << 1) |         // Select channel 1 (AD0.1)
            (4 << 8) |         // CLKDIV
            (1 << 21);         // Enable ADC
}

// Read from ADC0 channel 1
unsigned int read_adc(void) {
    AD0CR |= (1 << 24);                // Start conversion
    while (!(AD0GDR & (1UL << 31)));   // Wait for DONE bit
    return (AD0GDR >> 6) & 0x3FF;      // 10-bit result
}

// GPIO Init for Heater and Buzzer
void gpio_init(void) {
    IODIR0 |= HEATER_PIN | BUZZER_PIN;  // P0.16 & P0.17 as output
}

// Control Heater
void set_heater(int on) {
    if (on)
        IOSET0 = HEATER_PIN;
    else
        IOCLR0 = HEATER_PIN;
}

// Control Buzzer
void set_buzzer(int on) {
    if (on)
        IOSET0 = BUZZER_PIN;
    else
        IOCLR0 = BUZZER_PIN;
}

// Main Function
int main(void) {
    unsigned int adc_val, temp;
    char msg[50];
    char *state;

    adc_init();
    uart0_init();
    gpio_init();

    while (1) {
        adc_val = read_adc();                 // Read LM35 ADC
        temp = (adc_val * 330) / 1023;        // Convert to °C (10mV/°C)

        // Determine State
        if (temp < HEATING_TEMP_LOW) {
            state = "IDLE";
            set_heater(0);
            set_buzzer(0);
        } else if (temp < HEATING_TEMP_HIGH) {
            state = "HEATING";
            set_heater(1);
            set_buzzer(0);
        } else if (temp < OVERHEAT_TEMP) {
            state = "TARGET REACHED";
            set_heater(0);
            set_buzzer(0);
        } else {
            state = "OVERHEAT";
            set_heater(0);
            set_buzzer(1);
        }

        // Log via UART
        sprintf(msg, "Temp: %u C | State: %s\r\n", temp, state);
        uart0_print(msg);

        delay_ms(1000);  // Wait 1 second
    }
}
