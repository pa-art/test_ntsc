/**
 * Test program for NTSC signal generation;
 * Feb.27--, 2021  Pa@ART
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
//#include "ascii_chars.h"
#include "font8x8_basic.h"

#define LED     25      // GPIO connected LED on the board
#define MLED    (1 << LED)
#define LEDON   gpio_put_masked(MLED, MLED)
#define LEDOFF  gpio_put_masked(MLED, 0)
#define GP14    14      // GPIO14 connected to RCA+ pin via 330 ohm
#define GP15    15      // GPIO15 connected to RCA+ pin via 1k ohm
#define M14     (1 << GP14) // bit mask for GPIO14
#define M15     (1 << GP15) // bit mask for GPIO15
#define SYNC    gpio_put_masked((M14 | M15), 0)             // GPIO14='L' and GPIO15='L'
#define WHITE   gpio_put_masked((M14 | M15), (M14 | M15))   // GPIO14='H' and GPIO15='H'
#define BLACK   gpio_put_masked((M14 | M15), M15)           // GPIO14='L' and GPIO15='H'
#define GRAY    gpio_put_masked((M14 | M15), M14)           // GPIO14='H' and GPIO15='L'
#define VRAM_W  20      // width size of VRAM
#define VRAM_H  20      // height size of VRAM
#define V_BASE  48      // horizontal line number to start displaying VRAM

unsigned char vram[VRAM_W][VRAM_H]; // VRAM
int count = 1;                      // horizontal line counter
int count_vsync = 0;
int bx = 0;
int by = 0;
int bbx, bby;
bool state = true;
char buf[VRAM_W];
int bar_len = 0;
bool bar_inc = true;
#define BAR_MAX 16

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

// to generate horizontal sync siganl
void hsync( void ) {
    SYNC;
    //sleep_us(5);
    sleep_us(5);
    BLACK;
    //sleep_us(7);
    sleep_us(7);
}

// to generate vertical sync siganl
void vsync ( void ) {
    SYNC;
/*
    sprintf(buf, "(%2d,%2d)", bx, by);
    vram_strings(bx, by, buf);
    if (count_vsync % 200 == 0) {
        vram_strings(bbx, bby, "       ");
        bx = rand() % VRAM_W;
        by = rand() % VRAM_H;
        bbx = bx; bby = by;
    }
*/
    if (count_vsync % 50 == 0) {
        if (bar_inc == true) {
            vram_write(bar_len, 13, '@');
            bar_len++;
            if (bar_len == BAR_MAX) {
                bar_inc = false;
            }
        } else {
            vram_write(bar_len, 13, ' ');
            bar_len--;
            if (bar_len == 0) {
                bar_inc = true;
            }
        }
    }
    if (state == true) {
        LEDON;
    } else {
        LEDOFF;
    }
    if (count_vsync % 200 == 0) {
        state = !state;
    }
    //sleep_us(25);
    sleep_us(25);
    BLACK;
    sleep_us(5);
    SYNC;
    sleep_us(25);
    BLACK;
    sleep_us(5);
    count_vsync++;
}

int main() {
    // initialize GPIO14 and GPIO15
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
    // clear VRAM
    vram_clear();
/*  
    int cnt = 0;
    for (int y = 0; y < VRAM_H; y += 2) {
        for (int x = 0; x < VRAM_W; x++) {
            vram_write(x, y, cnt++);
            if (cnt > ASCII_CHAR_NUM) {
                cnt = 0;
            }
        }
    }
*/   
/*
    for (int i = 0; i < 5; i++) {
        vram_strings(i, 5+2*i, "Hello, world!");
    }
*/
    // write message to VRAM
    vram_strings(0, 3, "Hello, world!");
    vram_strings(0, 5, "This is a demo");
    vram_strings(0, 6, "of NTSC signal");
    vram_strings(0, 7, "generation.");
    vram_strings(0, 9, "0123456789");
    vram_strings(0, 10, "@[]+-<>!/%$#()\\");

    while (1) {
        // vertical synchronization duration
        if (count >= 3 && count <= 5) {
            vsync();    // vertical SYNC
        // upper blank area 
        } else if (count >= 6 && count < V_BASE) {
            hsync();    // horizontal SYNC
            BLACK;
            sleep_us(48);
        // VRAM drawing area
        } else if (count >= V_BASE && count < V_BASE + VRAM_H * CHAR_H) {
            hsync();
            // left blank??
            sleep_us(1);    // should be tuned
            // calculate VRAM y position from scan line number
            int y = (count - V_BASE) / CHAR_H;
            // horizontal VRAM drawing
            for (int x = 0; x < VRAM_W; x++) {
                // read VRAM
                int c = vram[x][y];
                // draw bit from character table (ascii_table)
                for (int w = 0; w < CHAR_W; w++) {
                    if ((ascii_table[c][count % CHAR_H] & (1 << w)) != 0) {
                        WHITE;
                    } else {
                        BLACK;
                    }
                }
            }
            // right blank??
            BLACK;
            sleep_us(6);    // should be tuned
        } else {
            hsync();
            BLACK;
            sleep_us(48);
        }
        // count up scan line 
        count++;
        // if scan line reach to max
        if (count > 262) {
            count = 1;
        }
    }

    return 0;
}