/**
 * Test program for NTSC signal generation.
 * Life game is demonstrated.
 * ATTENTION: You should compile this source with Release option of ARM compiler.
 * Feb.27--, 2021  Pa@ART
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "font8x8_basic.h"
//#include "font7x5_basic.h"

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
#define SYNC    gpio_put_masked(M14 | M15, 0)             // GPIO14='L' and GPIO15='L'
#define WHITE   gpio_put_masked(M14 | M15, M14 | M15)     // GPIO14='H' and GPIO15='H'
#define BLACK   gpio_put_masked(M14 | M15, M15)           // GPIO14='L' and GPIO15='H'
#define GRAY    gpio_put_masked(M14 | M15, M14)           // GPIO14='H' and GPIO15='L'
#define VRAM_W  30      // width size of VRAM
#define VRAM_H  28      // height size of VRAM
#define V_BASE  24      // horizontal line number to start displaying VRAM
#define BAR_MAX (VRAM_W-1)      // bar length

#define LIFE_SIZE   25  // size of life field
#define LIVE    1       // live state of life
#define DEAD    0       // dead state of life
#define DRAW_SX 2       // draw start of x
#define DRAW_SY 3       // draw start of y
#define BBOX    7       // character for living life (4)
#define WBOX    0       // character for dead life (5)
#define STABLE  30      // stages for judging stable state

volatile unsigned char vram[VRAM_W][VRAM_H]; // VRAM
volatile int count = 1;                      // horizontal line counter
int count_vsync = 0;
int bx = 0; int by = 0;
int bbx = 0; int bby = 0;
int buf_len = 1;
volatile bool state = true;

int life[LIFE_SIZE][LIFE_SIZE];

// to clear VRAM contents (set to 0)
void vram_clear( void ) {
    for (int i = 0; i < VRAM_W; i++) {
        for (int j = 0; j < VRAM_H; j++) {
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

// to write strings into VRAM located at (x, y)
void vram_strings( int x, int y, char *mes) {
    // if invalid (x, y), return
    if ((x < 0) || (x > VRAM_W) || (y < 0) || (y > VRAM_H)) {
        return;
    }
    int l = strlen(mes);
    for (int i = 0; i < l; i++) {
        // if x position overflows, return
        if (x + i >= VRAM_W) {
            return;
        // else put a character at the position
        } else {
            vram[x + i][y] = mes[i];
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
void measure_temp( int line ) {
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
    vram_strings(0, line, mes);
}

// initialize random seed by ADC data
void init_random( ) {
    int seed_val = 0;
    for (int i = 0; i < 5; i++) {
        adc_select_input(i);
        seed_val += adc_read();
    }
    srand(seed_val);
}

// generate initial life map
void gen_life( ) {
    for (int i = 0; i < LIFE_SIZE; i++) {
        for (int j = 0; j < LIFE_SIZE; j++) {
            life[i][j] = ((rand() % 100) <= 20) ? LIVE : 0;
        }
    }
}

// update life map
void update_life( ) {
    int tmp_life[LIFE_SIZE][LIFE_SIZE];

    for (int i = 0; i < LIFE_SIZE; i++) {
        for (int j = 0; j < LIFE_SIZE; j++) {
            tmp_life[i][j] = dead_or_alive(i, j);
        }
    }

    for (int i = 0; i < LIFE_SIZE; i++) {
        for (int j = 0; j < LIFE_SIZE; j++) {
            life[i][j] = tmp_life[i][j];
        }
    }
}

// judge life if dead or alive
int dead_or_alive(int x, int y) {
    int lcount = 0;

    // search around and countup if the life is alive
    if (x-1 >= 0) {
        if (life[x-1][y] == LIVE) lcount++;
        if (y-1 >= 0) {
            if (life[x-1][y-1] == LIVE) lcount++;
        }
        if (y+1 < LIFE_SIZE) {
            if (life[x-1][y+1] == LIVE) lcount++;
        }
    }
    if (x+1 < LIFE_SIZE) {
        if (life[x+1][y] == LIVE) lcount++;
        if (y-1 >= 0) {
            if (life[x+1][y-1] == LIVE) lcount++;
        }
        if (y+1 < LIFE_SIZE) {
            if (life[x+1][y+1] == LIVE) lcount++;
        }
    }
    if (y-1 >= 0) {
        if (life[x][y-1] == LIVE) lcount++;
    }
    if (y+1 < LIFE_SIZE) {
        if (life[x][y+1] == LIVE) lcount++;
    }

    // if life is alive
    if (life[x][y] == LIVE) {
        if ((lcount == 2) || (lcount == 3)) {
            return LIVE;
        } else {
            return DEAD;
        }
    // if life is dead
    } else {
        if (lcount == 3) {
            return LIVE;
        } else {
            return DEAD;
        }
    }
    return DEAD;
}

// count number of lives
int count_life( ) {
    int lives = 0;
    for (int i = 0; i < LIFE_SIZE; i++) {
        for (int j = 0; j < LIFE_SIZE; j++) {
            lives += (life[i][j] == LIVE) ? 1 : 0;
        }
    }
    return lives;
}

// draw life map
void draw_life( ) {
    for (int i = 0; i < LIFE_SIZE; i++) {
        for (int j = 0; j < LIFE_SIZE; j++) {
            int c = (life[i][j] >= LIVE) ? BBOX : WBOX;
            vram_write(i + DRAW_SX, j + DRAW_SY, c);
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
    } else if (count >= V_BASE && count < V_BASE + VRAM_H * CHAR_H) {
        hsync();
        // left blank??
        BLACK;
        sleep_us(0);    // should be tuned

        // calculate VRAM y position from scan line number
        int y = (count - V_BASE) / CHAR_H;
        int cline = count % CHAR_H;
        // horizontal VRAM drawing
        for (int x = 0; x < VRAM_W; x++) {
            // read VRAM
            volatile unsigned char c = vram[x][y];
            // draw bit from character table (ascii_table)
            for (int w = 0; w < CHAR_W; w++) {
                if ((ascii_table[c][cline] & (1 << w)) != 0) {
                    WHITE;
                } else {
                    volatile uint16_t tmp = 1;
                    BLACK;
                }
            }
        }
        // right blank??
        BLACK;
        sleep_us(0);
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
    // initialize random seed
    init_random();

    // GPPWM pin is the PWM output
    gpio_set_function(GPPWM, GPIO_FUNC_PWM);
    // Figure out which slice we just connected to the GPPWM pin
    uint slice_num = pwm_gpio_to_slice_num(GPPWM);

    // Mask our slice's IRQ output into the PWM block's single interrupt line,
    // and register our interrupt handler
    pwm_clear_irq(slice_num);
    pwm_set_irq_enabled(slice_num, true);
    irq_set_priority(PWM_IRQ_WRAP, 0xC0);   // somehow this is needed if you compile with release option
    irq_set_exclusive_handler(PWM_IRQ_WRAP, horizontal_line);
    irq_set_enabled(PWM_IRQ_WRAP, true); 

    // Set counter wrap value to generate PWM interrupt by this value
    pwm_set_wrap(slice_num, 7999);
    // Load the configuration into our PWM slice, and set it running.
    pwm_set_enabled(slice_num, true);

    // generate initial life map
    gen_life();

    int countup = 0;
    int stages = 0;
    int num_live, num_live_p;
    int stable_count = 0;
    char mes[VRAM_W];

    while (1) {
        if (countup % 200000 == 0) {
            // flip LED
            flip_led();
        }
        if (countup % 200000 == 0) {
            // draw life map
            draw_life();
            // update life map
            update_life();
            // count lives
            num_live = count_life();
            // display messages
            vram_strings(0, 1, "                  ");
            vram_strings(0, 2, "                  ");
            sprintf(mes, "Stage = %d", stages++);
            vram_strings(5, 1, mes);
            sprintf(mes, "Lives = %d", count_life());
            vram_strings(5, 2, mes);
            // if number of lives is equal to the previous number
            if (num_live == num_live_p) {
                // count up stable state
                stable_count++;
                // if stable state continues STABLE times
                if (stable_count > STABLE) {
                    // reset life map
                    gen_life();
                    stages = 0;
                    stable_count = 0;
                }
            } else {
                // reset stable count
                stable_count = 0;
            }
            // memory previous number of lives
            num_live_p = num_live;
        }

        countup++;
    }

    return 0;
}
