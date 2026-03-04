#include "hardware/timer.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"
// 7-segment display message buffer
// Declared as static to limit scope to this file only.
static char msg[8] = {
    0x3F, // seven-segment value of 0
    0x06, // seven-segment value of 1
    0x5B, // seven-segment value of 2
    0x4F, // seven-segment value of 3
    0x66, // seven-segment value of 4
    0x6D, // seven-segment value of 5
    0x7D, // seven-segment value of 6
    0x07, // seven-segment value of 7
};
extern char font[]; // Font mapping for 7-segment display
static int index = 0; // Current index in the message buffer

// We provide you with this function for directly displaying characters.
// This now accounts for the decimal point.
void display_char_print(const char message[]) {
    int dp_found = 0;
    for (int i = 0; i < 8; i++) {
        if (message[i] == '.') {
            // add it to the decimal point bit for prev digit if i > 0
            if (i > 0) {
                msg[i - 1] |= (1 << 7); // Set the decimal point bit
                dp_found = 1; // Mark that we found a decimal point
            }
        }
        else {
            msg[i - dp_found] = font[message[i] & 0xFF];
        }
    }
    if (dp_found) {
        msg[7] = font[message[8] & 0xFF];}
}

/********************************************************* */
// Implement the functions below.

void display_init_pins() {
    for (int p = 10; p <= 20; p++) {
        gpio_init(p);
        gpio_set_dir(p, GPIO_OUT);
        gpio_put(p, 0);
    }
}

void display_isr() {
    timer_hw_t *t1 = (timer_hw_t *)TIMER1_BASE;

    t1->intr = 1u << 0;

    uint32_t mask = (0xFFu << 10) | (0x7u << 18);
    uint32_t val  = ((uint32_t)(msg[index] & 0xFF) << 10) | ((uint32_t)(index & 0x7) << 18);
    gpio_put_masked(mask, val);

    index = (index + 1) & 7;

    uint32_t now = t1->timerawl;
    t1->alarm[0] = now + 3000;
}

void display_init_timer() {
    timer_hw_t *t1 = (timer_hw_t *)TIMER1_BASE;

    hw_set_bits(&t1->inte, 1u << 0);
    irq_set_exclusive_handler(TIMER1_IRQ_0, display_isr);
    irq_set_enabled(TIMER1_IRQ_0, true);

    uint32_t now = t1->timerawl;
    t1->alarm[0] = now + 3000;
}

void display_print(const uint16_t message[]) {
    for (int i = 0; i < 8; i++) {
        uint16_t v = message[i];
        char ch = (char)(v & 0xFF);
        msg[i] = font[(unsigned char)ch];
        if (v & 0x100) msg[i] |= (1 << 7);
        else msg[i] &= ~(1 << 7);
    }
}