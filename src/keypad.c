#include "pico/stdlib.h"
#include <hardware/gpio.h>
#include <hardware/timer.h>
#include <hardware/irq.h>
#include <hardware/structs/sio.h>
#include <stdio.h>
#include "queue.h"

int col = -1;

static bool state[16];

const char keymap[17] = "DCBA#9630852*741";

void keypad_drive_column();
void keypad_isr();

void keypad_init_pins() {
    for (int i = 2; i <= 5; i++) {
        gpio_set_function(i, GPIO_FUNC_SIO);
        sio_hw->gpio_oe_clr = 1u << i;
        gpio_disable_pulls(i);
    }
    for (int i = 6; i <= 9; i++) {
        gpio_set_function(i, GPIO_FUNC_SIO);
        sio_hw->gpio_oe_set = 1u << i;
        sio_hw->gpio_clr = 1u << i;
        gpio_disable_pulls(i);
    }

    for (int i = 0; i < 16; i++) state[i] = false;
    col = -1;
}

void keypad_init_timer() {
    hw_set_bits(&timer_hw->inte, 1u << 0);
    irq_set_exclusive_handler(TIMER0_IRQ_0, keypad_drive_column);

    hw_set_bits(&timer_hw->inte, 1u << 1);
    irq_set_exclusive_handler(TIMER0_IRQ_1, keypad_isr);

    uint32_t now = timer_hw->timerawl;
    timer_hw->alarm[0] = now + 25000;
    timer_hw->alarm[1] = now + 35000;

    irq_set_enabled(TIMER0_IRQ_0, true);
    irq_set_enabled(TIMER0_IRQ_1, true);
}

uint8_t keypad_read_rows() {
    return ((sio_hw->gpio_in >> 2) & 0xF);
}

void keypad_drive_column() {
    timer_hw->intr = 1u << 0;

    col = (col + 1) & 0x3;

    uint32_t mask = (0xFu << 6);
    sio_hw->gpio_clr = mask;                
    sio_hw->gpio_set = 1u << (col + 6);     

    timer_hw->alarm[0] = timer_hw->timerawl + 25000;
}

void keypad_isr() {
    timer_hw->intr = 1u << 1;

    uint8_t rows = keypad_read_rows();

    for (int row = 0; row < 4; row++) {
        int idx = 4 * col + row;
        bool pressed_now = (rows & (1u << row)) != 0;

        if (pressed_now && !state[idx]) {
            state[idx] = true;
            char key_char = keymap[idx];
            uint16_t event = (1u << 8) | (uint8_t)key_char;
            key_push(event);
        }
        else if (!pressed_now && state[idx]) {
            state[idx] = false;
            char key_char = keymap[idx];
            uint16_t event = (uint8_t)key_char;
            key_push(event);
        }
    }

    timer_hw->alarm[1] = timer_hw->timerawl + 25000;
}