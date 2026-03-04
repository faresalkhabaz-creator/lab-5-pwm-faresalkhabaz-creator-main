#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "queue.h"
#include "support.h"

//////////////////////////////////////////////////////////////////////////////

const char* username = "falkhabb";

//////////////////////////////////////////////////////////////////////////////

static int duty_cycle = 0;
static int dir = 0;
static int color = 0;

void display_init_pins();
void display_init_timer();
void display_char_print(const char message[]);
void keypad_init_pins();
void keypad_init_timer();
void init_wavetable(void);
void set_freq(int chan, float f);
extern KeyEvents kev;
void drum_machine();

extern void autotest(void);

static inline uint16_t pwm_read_top(uint slice) {
    return (uint16_t)pwm_hw->slice[slice].top;
}

//////////////////////////////////////////////////////////////////////////////

// When testing static duty-cycle PWM
//#define STEP2
// When testing variable duty-cycle PWM
// #define STEP3
// When testing 8-bit audio synthesis
#define STEP4
// When trying out drum machine
// #define DRUM_MACHINE

//////////////////////////////////////////////////////////////////////////////

void init_pwm_static(uint32_t period, uint32_t duty_cycle) {
    // Configure pins 37, 38, 39 as PWM outputs
    const uint pins[3] = {37, 38, 39};

    if (duty_cycle > period) duty_cycle = period;

    for (int i = 0; i < 3; i++) {
        uint pin = pins[i];
        gpio_set_function(pin, GPIO_FUNC_PWM);

        uint slice = pwm_gpio_to_slice_num(pin);
        uint chan  = pwm_gpio_to_channel(pin);

        pwm_set_clkdiv(slice, 150.0f);
        pwm_set_wrap(slice, (uint16_t)(period - 1));
        pwm_set_chan_level(slice, chan, (uint16_t)duty_cycle);

        pwm_set_enabled(slice, true);
    }
}

void pwm_breathing() {
    // fill in
    const uint pins[3] = {37, 38, 39};

    uint slice0 = pwm_gpio_to_slice_num(37);

    pwm_clear_irq(slice0);

    uint16_t top = pwm_read_top(slice0);
    uint32_t current_period = (uint32_t)top + 1;

    if (dir == 0 && duty_cycle == 100) {
        color = (color + 1) % 3;
    }

    if (duty_cycle == 100 && dir == 0) dir = 1;
    else if (duty_cycle == 0 && dir == 1) dir = 0;

    if (dir == 0) duty_cycle += 1;
    else duty_cycle -= 1;

    if (duty_cycle > 100) duty_cycle = 100;
    if (duty_cycle < 0) duty_cycle = 0;

    uint32_t level = (current_period * (uint32_t)duty_cycle) / 100;

    for (int i = 0; i < 3; i++) {
        uint pin = pins[i];
        uint s = pwm_gpio_to_slice_num(pin);
        uint c = pwm_gpio_to_channel(pin);
        pwm_set_chan_level(s, c, 0);
    }

    {
        uint pin = pins[color];
        uint s = pwm_gpio_to_slice_num(pin);
        uint c = pwm_gpio_to_channel(pin);
        pwm_set_chan_level(s, c, (uint16_t)level);
    }
}

void init_pwm_irq() {
    // fill in
    uint slice = pwm_gpio_to_slice_num(37);

    pwm_clear_irq(slice);
    pwm_set_irq_enabled(slice, true);

    #if defined(PWM_IRQ_WRAP0)
        const uint pwm_irq = PWM_IRQ_WRAP0;
    #elif defined(PWM_IRQ_WRAP_0)
        const uint pwm_irq = PWM_IRQ_WRAP_0;
    #else
        const uint pwm_irq = PWM_IRQ_WRAP;
    #endif

    irq_set_exclusive_handler(pwm_irq, pwm_breathing);
    irq_set_enabled(pwm_irq, true);

    uint16_t top = pwm_read_top(slice);
    uint32_t current_period = (uint32_t)top + 1;

    duty_cycle = 100;
    dir = 1;

    const uint pins[3] = {37, 38, 39};
    for (int i = 0; i < 3; i++) {
        uint pin = pins[i];
        uint s = pwm_gpio_to_slice_num(pin);
        uint c = pwm_gpio_to_channel(pin);
        pwm_set_chan_level(s, c, (uint16_t)current_period);
    }
}

void pwm_audio_handler() {
    // fill in
    uint slice = pwm_gpio_to_slice_num(36);
    uint chan  = pwm_gpio_to_channel(36);

    pwm_clear_irq(slice);

    offset0 += step0;
    offset1 += step1;

    if (offset0 >= (N << 16)) offset0 -= (N << 16);
    if (offset1 >= (N << 16)) offset1 -= (N << 16);

    uint32_t samp = (uint32_t)wavetable[offset0 >> 16] + (uint32_t)wavetable[offset1 >> 16];
    samp /= 2;

    uint16_t top = pwm_read_top(slice);
    uint32_t period = (uint32_t)top + 1;

    uint32_t level = (samp * period) >> 16;
    pwm_set_chan_level(slice, chan, (uint16_t)level);
}

void init_pwm_audio() {
    // fill in
    gpio_set_function(36, GPIO_FUNC_PWM);

    uint slice = pwm_gpio_to_slice_num(36);
    uint chan  = pwm_gpio_to_channel(36);

    pwm_set_clkdiv(slice, 150.0f);

    uint32_t top = (1000000u / (uint32_t)RATE) - 1u;
    pwm_set_wrap(slice, (uint16_t)top);

    pwm_set_chan_level(slice, chan, 0);

    init_wavetable();

    pwm_clear_irq(slice);
    pwm_set_irq_enabled(slice, true);

    #if defined(PWM_IRQ_WRAP0)
        const uint pwm_irq = PWM_IRQ_WRAP0;
    #elif defined(PWM_IRQ_WRAP_0)
        const uint pwm_irq = PWM_IRQ_WRAP_0;
    #else
        const uint pwm_irq = PWM_IRQ_WRAP;
    #endif

    irq_set_exclusive_handler(pwm_irq, pwm_audio_handler);
    irq_set_enabled(pwm_irq, true);

    pwm_set_enabled(slice, true);
}
//////////////////////////////////////////////////////////////////////////////

int main()
{
    
    stdio_init_all();

    // Uncomment when you need to run autotest.
    autotest();

    keypad_init_pins();
    keypad_init_timer();
    display_init_pins();
    display_init_timer();

    /*
    *******************************************************
    * Make sure to go through the code in the steps below.  
    * A lot of it can be very useful for your projects.
    *******************************************************
    */

#ifdef STEP2
    init_pwm_static(100, (50 * 100) / 100); 
    display_char_print("      50");

    uint16_t percent = 50;
    uint16_t disp_buffer = 0;
    char buf[9];

    snprintf(buf, sizeof(buf), "      50");
    display_char_print(buf);

    bool new_entry = true;

    for (;;) {
        uint16_t keyevent = key_pop();
        if (keyevent & 0x100) {
            char key = keyevent & 0xFF;

            if (key >= '0' && key <= '9') {
                if (new_entry) {
                    disp_buffer = 0;
                    new_entry = false;
                }
                disp_buffer = (disp_buffer * 10) + (key - '0');
                snprintf(buf, sizeof(buf), "%8d", disp_buffer);
                display_char_print(buf);
            }
            else if (key == '#') {
                percent = disp_buffer;
                if (percent > 100) percent = 100;

                init_pwm_static(100, (percent * 100) / 100);
                snprintf(buf, sizeof(buf), "%8d", percent);
                display_char_print(buf);

                new_entry = true;
            }
            else if (key == '*') {
                disp_buffer = 50;
                percent = 50;

                init_pwm_static(100, (percent * 100) / 100);
                snprintf(buf, sizeof(buf), "      50");
                display_char_print(buf);

                new_entry = true;
            }
            else {
                new_entry = true;
            }
        }
    }
#endif

#ifdef STEP3
    init_pwm_static(10000, 5000); 
    init_pwm_irq(); 

    for(;;) {
        tight_loop_contents();
    }
#endif

#ifdef STEP4
    char freq_buf[9] = {0};
    int pos = 0;
    bool decimal_entered = false;
    int decimal_pos = 0;
    int current_channel = 0;
    char out_buf[9];

    init_pwm_audio();

    set_freq(0, 440.0f);
    set_freq(1, 0.0f);
    display_char_print(" 440.000");

    for (;;) {
        uint16_t keyevent = key_pop();

        if (keyevent & 0x100) {
            char key = keyevent & 0xFF;

            if (key == 'A' || key == 'B') {
                current_channel = (key == 'A') ? 0 : 1;
                pos = 0;
                freq_buf[0] = '\0';
                decimal_entered = false;
                display_char_print("        ");
            }
            else if (key >= '0' && key <= '9') {
                if (pos < 8) {
                    freq_buf[pos++] = key;
                    freq_buf[pos] = '\0';
                    snprintf(out_buf, sizeof(out_buf), "%-8s", freq_buf);
                    display_char_print(out_buf);
                }
            }
            else if (key == '*') {
                if (!decimal_entered && pos < 7) {
                    freq_buf[pos++] = '.';
                    freq_buf[pos] = '\0';
                    decimal_entered = true;
                    snprintf(out_buf, sizeof(out_buf), "%-8s", freq_buf);
                    display_char_print(out_buf);
                }
            }
            else if (key == '#') {
                float freq = strtof(freq_buf, NULL);
                set_freq(current_channel, freq);

                char out[9];
                snprintf(out, sizeof(out), "%8.3f", freq);
                display_char_print(out);

                pos = 0;
                freq_buf[0] = '\0';
                decimal_entered = false;
            }
        }
        asm volatile ("wfi");
    }
#endif

#ifdef DRUM_MACHINE
    drum_machine();
#endif

    while (true) {
        printf("Hello, world!\n");
        sleep_ms(1000);
    }

    return 0;
}