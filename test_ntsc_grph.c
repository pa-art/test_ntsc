/**
 * Test program for NTSC signal generation;
 * Feb.27--, 2021  Pa@ART
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "font8x8_basic.h"

#define CONV_FACTOR (3.3f / (1 << 12)) // ADC data -> voltage (white Pico)
#define ADC_TEMP    4 // temperature sensor input
#define LED     25      // GPIO connected LED on the board
#define MLED    (1 << LED)
#define LEDON   gpio_put_masked(MLED, MLED)
#define LEDOFF  gpio_put_masked(MLED, 0)
#define GPPWM   2       // GPIO2 is PWM output
#define GP14    14      // GPIO14 connected to RCA+ pin via 330 ohm
#define GP15    15      // GPIO15 connected to RCA+ pin via 1k ohm
#define M14     (1 << GP14) // bit mask for GPIO14
#define M15     (1 << GP15) // bit mask for GPIO15
#define SYNC    gpio_put_masked((M14 | M15), 0)             // GPIO14='L' and GPIO15='L'
#define WHITE   gpio_put_masked((M14 | M15), (M14 | M15))   // GPIO14='H' and GPIO15='H'
#define BLACK   gpio_put_masked((M14 | M15), M15)           // GPIO14='L' and GPIO15='H'
#define GRAY    gpio_put_masked((M14 | M15), M14)           // GPIO14='H' and GPIO15='L'
#define VRAM_W  32      // width size of VRAM
#define VRAM_H  24      // height size of VRAM
//#define CHAR_W  8       // width of a character
//#define CHAR_H  8       // height of a character
#define GVRAM_W (VRAM_W * CHAR_W)   // width size of graphic VRAM
#define GVRAM_H (VRAM_H * CHAR_H)   // height size of graphic VRAM
#define V_BASE  48      // horizontal line number to start displaying VRAM
#define BDOT    0       // black dot
#define WDOT    1       // white dot
#define GDOT    2       // gray dot
#define BAR_MAX 20      // bar length
#define PI      3.14159265

volatile unsigned char vram[GVRAM_W][GVRAM_H]; // VRAM
int count = 1;                      // horizontal line counter
int count_vsync = 0;
int bx = 0; int by = 0;
int bbx = 0; int bby = 0;
int buf_len = 1;
volatile bool state = true;
int bar_len = 0;
bool bar_inc = true;

// to clear VRAM contents (set to 0)
void vram_clear( void ) {
    for (int i = 0; i < GVRAM_W; i++) {
        for (int j = 0; j < GVRAM_H; j++) {
            vram[i][j] = 0;
        }
    }
}

// to write a value into VRAM located at (x, y)
void vram_write ( int x, int y, unsigned char value ) {
    vram[x][y] = value;
}

// to read a value from VRAM located at (x, y)
unsigned char vram_read( int x, int y) {
    return vram[x][y];
}

// put a character on VRAM
void vram_put_char( int x, int y, char c, char col ) {
    unsigned char cp;
    for (int i = 0; i < CHAR_H; i++) {
        cp = ascii_table[c][i];
        for (int j = 0; j < CHAR_W; j++) {
            if ((cp & (1 << j)) != 0) {
                vram[x + j][y + i] = col;
            } else {
                vram[x + j][y + i] = BDOT;
            }
        }
    }
}

// put strings on VRAM
void vram_strings( int x, int y, char *mes, char col ) {
    // if invalid (x, y), return
    if ((x < 0) || (x > GVRAM_W) || (y < 0) || (y > GVRAM_H)) {
        return;
    }
    int l = strlen(mes);
    for (int i = 0; i < l * CHAR_W; i += CHAR_W) {
        // if x position overflows, return
        if (x + i >= GVRAM_W) {
            return;
        // else put a character at the position
        } else {
            vram_put_char(x + i, y, mes[i / CHAR_W], col);
        }
    }
    return;
}

// flip LED
void flip_led( void ) {
    if (state == true) {
        LEDON;
    } else {
        LEDOFF;
    }
    state = !state;
}

// measure Temperature and display
void measure_temp( int x, int y, int col ) {
    uint16_t temp_dat;
    float voltage, temp;
    char mes[VRAM_W];
    // read ADC(temperature) data
    temp_dat = adc_read();
    // convert ADC data to voltage
    voltage = temp_dat * CONV_FACTOR;
    // convert voltage to temperature
    temp = 27 - (voltage - 0.706) / 0.001721;
    // make massage from voltage and temp
    sprintf(mes, "V=%2.3f T=%2.1f", voltage, temp);
    // display voltage and temp
    vram_strings(x, y, mes, col);
}

// display bar at the line with given character
void display_bar( int x, int y, char c, char col ) {
    if (bar_inc == true) {
        vram_put_char(x + bar_len * CHAR_W, y, c, col);
        bar_len++;
        if (bar_len == BAR_MAX) {
            bar_inc = false;
        }
    } else {
        vram_put_char(x + bar_len * CHAR_W, y, ' ', col);
        bar_len--;
        if (bar_len == 0) {
            bar_inc = true;
        }
    }
}

// to generate horizontal sync siganl
void hsync( void ) {
    SYNC;
    sleep_us(5);
    BLACK;
    sleep_us(7);
}

// to generate vertical sync siganl
void vsync ( void ) {
    SYNC;
    sleep_us(10);
    sleep_us(5);
    sleep_us(10);
    BLACK;
    sleep_us(5);

    SYNC;
    sleep_us(10);
    sleep_us(5);
    sleep_us(10);
    BLACK;
    sleep_us(5);

    count_vsync++;
}

// handler for holizontal line processing
void horizontal_line( ) {
    // Clear the interrupt flag that brought us here
    pwm_clear_irq(pwm_gpio_to_slice_num(GPPWM));

    // vertical synchronization duration
    if (count >= 3 && count <= 5) {
        vsync();    // vertical SYNC

    // VRAM drawing area
    } else if (count >= V_BASE && count < V_BASE + GVRAM_H) {
        hsync();
        // left blank??
        BLACK;
        sleep_us(0);    // should be tuned

        int y = count - V_BASE;
        for (int x = 0; x < GVRAM_W; x++) {
            int c = vram[x][y];
            switch (c) {
                case BDOT:  // black
                BLACK;
                break;
                case WDOT:  // white
                WHITE;
                break;
                case GDOT:  // gray
                GRAY;
                break;
                default:
                BLACK;
            }
        }

/*
        WHITE;
        sleep_us(10);
        sleep_us(10);
*/
        // right blank??
        BLACK;
    } else {
        hsync();
        BLACK;
    }
    // count up scan line 
    count++;
    // if scan line reach to max
    if (count > 262) {
        count = 1;
    }
    return;
}

int main() {

    // initialize GPIO14 and GPIO15 for masked output
    gpio_init(GP14);
    gpio_init(GP15);
    gpio_init_mask(M14 | M15);
    gpio_set_dir(GP14, GPIO_OUT);
    gpio_set_dir(GP15, GPIO_OUT);
    // initialize LED GPIO
    gpio_init(LED);
    gpio_init_mask(MLED);
    gpio_set_dir(LED, GPIO_OUT);
    // init stdio
    stdio_init_all();
    // init ADC
    adc_init();
    // enable temperature sensor
    adc_set_temp_sensor_enabled(true);
    // select ADC input
    adc_select_input(ADC_TEMP);    // ADC selected
    // clear VRAM
    vram_clear();

    // GPPWM pin is the PWM output
    gpio_set_function(GPPWM, GPIO_FUNC_PWM);
    // Figure out which slice we just connected to the GPPWM pin
    uint slice_num = pwm_gpio_to_slice_num(GPPWM);

    // Mask our slice's IRQ output into the PWM block's single interrupt line,
    // and register our interrupt handler
    pwm_clear_irq(slice_num);
    pwm_set_irq_enabled(slice_num, true);
    irq_set_priority(PWM_IRQ_WRAP, 0xC0);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, horizontal_line);
    irq_set_enabled(PWM_IRQ_WRAP, true); 

    // Get some sensible defaults for the slice configuration. By default, the
    // counter is allowed to wrap over its maximum range (0 to 2**16-1)
    pwm_config config = pwm_get_default_config();
    // Set counter wrap value to generate PWM interrupt by this value
    pwm_set_wrap(slice_num, 7999);
    // Load the configuration into our PWM slice, and set it running.
    pwm_set_enabled(slice_num, true);

    volatile int countup;
    double y0, y1;

    for (int x = 0; x < GVRAM_W; x++) {
        y0 = sin(2.0 * PI / GVRAM_W * (double)x) * (GVRAM_H / 2) + GVRAM_H / 2;
        y1 = cos(2.0 * PI / GVRAM_W * (double)x) * (GVRAM_H / 2) + GVRAM_H / 2;
        vram_write(x, y0, 1);
        vram_write(x, y1, 2);
    }
    vram_strings(10, 10, "SIN curve", WDOT);
    vram_strings(10, 20, "COS curve", GDOT);

    while (1) {
        if (countup % 200000 == 0) {
            // flip LED
            flip_led();
        }
        if (countup % 300000 == 0) {
            measure_temp(10, 30, WDOT);
        }
        if (countup % 10000 == 0) {
            display_bar(0, 50, '#', GDOT);
        }

        countup++;
    }

    return 0;
}
