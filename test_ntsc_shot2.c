/**
 * Test program for NTSC signal generation.
 * "FLYING METEOR!!" game is implemented.
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

#define RKEYGP  11      // Right key GP11
#define UKEYGP  10      // Up key GP10
#define DKEYGP  9       // Down key GP9
#define LKEYGP  8       // Left key GP8
#define AKEYGP  7       // A key GP7
#define BKEYGP  6       // B key GP6
#define RKEY    (1 << RKEYGP)
#define UKEY    (1 << UKEYGP)
#define LKEY    (1 << LKEYGP)
#define DKEY    (1 << DKEYGP)
#define AKEY    (1 << AKEYGP)
#define BKEY    (1 << BKEYGP)
#define CMETEOR  0xA     // character of meteor
#define CHEART  0x8     // character of heart
#define CME     0x9     // character of me
#define CBEAM   '-'
#define NMETEOR  30      // max number of METEOR
#define IMRATE  30000   // initial METEOR rate
#define MMRATE  1000    // minimum METEOR rate
#define IPMETEOR 20      // initial METEOR probability
#define NHEART  3       // max number of heart
#define HTRATE  30000   // heart rate
#define MYRATE  10000   // my rate
#define BRATE   8000    // beam rate
#define NBEAM   8       // max number of my shot
#define ME_X    5       // initial x of me
#define ME_Y    12      // initial y of me
#define ME_HP   3       // HP of me
#define POWERUP 3       // max recovery of HP
#define METEOR_SCORE    10  // score of meteor
#define HEART_SCORE     100 // score of heart
#define LSCORE  1       // line of score drawing
#define LHP     2       // line of score drawing

volatile unsigned char vram[VRAM_W][VRAM_H]; // VRAM
volatile int count = 1;                      // horizontal line counter
volatile bool state = true;
typedef struct {
    int x;          // x of entity
    int y;          // y of entity
    int hp;         // HP of entity
    char c;         // character of entity
    int sp;         // special power of entity
} entity;

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

// initialize video and LED GPIO
void init_video_and_led_GPIO( ) {
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
}

// initialize Key GPIO setting
void init_key_GPIO( ) {
    // GPIO init
    gpio_init(RKEYGP);
    gpio_init(UKEYGP);
    gpio_init(DKEYGP);
    gpio_init(LKEYGP);
    gpio_init(AKEYGP);
    gpio_init(BKEYGP);
    // GPIO direction
    gpio_set_dir(RKEYGP, GPIO_IN);
    gpio_set_dir(UKEYGP, GPIO_IN);
    gpio_set_dir(DKEYGP, GPIO_IN);
    gpio_set_dir(LKEYGP, GPIO_IN);
    gpio_set_dir(AKEYGP, GPIO_IN);
    gpio_set_dir(BKEYGP, GPIO_IN);
    // GPIO pullup
    gpio_pull_up(RKEYGP);
    gpio_pull_up(UKEYGP);
    gpio_pull_up(DKEYGP);
    gpio_pull_up(LKEYGP);
    gpio_pull_up(AKEYGP);
    gpio_pull_up(BKEYGP);
}

// scan keys
uint32_t key_scan( ) {

    uint32_t result = 0x0;

    if (gpio_get(RKEYGP) == 0) result |= RKEY;
    if (gpio_get(UKEYGP) == 0) result |= UKEY;
    if (gpio_get(DKEYGP) == 0) result |= DKEY;
    if (gpio_get(LKEYGP) == 0) result |= LKEY;
    if (gpio_get(AKEYGP) == 0) result |= AKEY;
    if (gpio_get(BKEYGP) == 0) result |= BKEY;

    return result;
}

// initialize METEOR array
void init_meteor( entity *e ) {
    for (int i = 0; i < NMETEOR; i++) {
        e[i].x = -1;
        e[i].y = -1;
        e[i].c = CMETEOR;
        e[i].hp = 1;
        e[i].sp = 0;
    }
}

// initialize heart array
void init_heart( entity *e ) {
    for (int i = 0; i < NHEART; i++) {
        e[i].x = -1;
        e[i].y = -1;
        e[i].c = CHEART;
        e[i].hp = 1;
        e[i].sp = 0;
    }
}

// initialize beam array
void init_beam( entity *e ) {
    for (int i = 0; i < NBEAM; i++) {
        e[i].x = -1;
        e[i].y = -1;
        e[i].c = CBEAM;
        e[i].hp = 0;
        e[i].sp = 0;
    }
}

// move other entity
void move_entity( entity *e, int max_num, int threshold ) {
    int i;
    // clear previous entity
    for (i = 0; i < max_num; i++) {
        if (e[i].x >= 0) {
            vram_write(e[i].x, e[i].y, ' ');
        }
    }
    // move entity
    for (i = 0; i < max_num; i++) {
        if (e[i].x >= 0) {
            // move left; if e[i].x < 0, the entity disappears
            e[i].x--;
            // move up/down or stay
            if ((rand() % 4) == 0) {
                e[i].y += ((rand() % 2) == 0) ? +1 : -1;
            }
            if (e[i].y >= VRAM_H) e[i].y = VRAM_H - 1;
            if (e[i].y <= LHP) e[i].y = LHP + 1;
        }
    }
    // generate new entity
    if (rand() % 100 < threshold) {
        // search disappeared entity position
        for (i = 0; i < max_num; i++) {
            if (e[i].x < 0) break;
        }
        // if we can generate new entity
        if (i < max_num) {
            e[i].x = VRAM_W - 1;
            e[i].y = rand() % (VRAM_H - LHP) + LHP + 1;
            e[i].sp = rand() % POWERUP + 1;
        }
    }
    // draw present entity
    for (i = 0; i < max_num; i++) {
        // if the entity exists
        if (e[i].x >= 0) {
            vram_write(e[i].x, e[i].y, e[i].c);
        }
    }
}

// scan keys and move me 
bool move_me( entity *e ) {
    uint32_t keys;

    // clear previous me
    vram_write(e->x, e->y, ' ');
    // scan keys
    keys = key_scan();
    // move right
    if ((keys & RKEY) != 0) {
        e->x++;
        if (e->x >= VRAM_W) e->x = VRAM_W - 1;
    }
    // move left
    if ((keys & LKEY) != 0) {
        e->x--;
        if (e->x < 0) e->x = 0;
    }
    // move up
    if ((keys & UKEY) != 0) {
        e->y--;
        if (e->y <= LHP) e->y = LHP + 1;
    }
    // move down
    if ((keys & DKEY) != 0) {
        e->y++;
        if (e->y >= VRAM_H) e->y = VRAM_H - 1;
    }
    // draw present me
    vram_write(e->x, e->y, e->c);

    return true;
}

// shot beam
void shot_beam( entity *e, entity *me ) {
    uint32_t keys;
    int i;
    // clear previous beams
    for (i = 0; i < NBEAM; i++) {
        if (e[i].hp > 0) {
            vram_write(e[i].x, e[i].y, ' ');
        }
    }
    // move beams
    for (i = 0; i < NBEAM; i++) {
        if (e[i].hp > 0) {
            // beam goes right
            e[i].x++;
            // if beam is out of range, clear it
            if (e[i].x >= VRAM_W) {
                e[i].hp = 0;
            }
        }
    }
    // scan keys
    keys = key_scan();
    // if A key is pushed
    if ((keys & AKEY) != 0) {
        // if new beam can be created
        for (i = 0; i < NBEAM; i++) {
            if (e[i].hp == 0) break;
        }
        if ((i < NBEAM) && (me->x < VRAM_W - 1)) {
            e[i].hp = 1;
            e[i].x = me->x + 1;
            e[i].y = me->y;
        }
    }
    // draw present beams
    for (i = 0; i < NBEAM; i++) {
        if (e[i].hp > 0) {
            vram_write(e[i].x, e[i].y, CBEAM);
        }
    }
}

// judge if I've got a heart or bumped into METEOR
int judge_me( entity *me, entity *METEOR, entity *heart) {
    int i;
    int bonus = 0;
    // heart 
    for (i = 0; i < NHEART; i++) {
        // if I've got a heart
        if ((me->x == heart[i].x) && (me->y == heart[i].y)) {
            // calculate bonus score
            bonus = heart[i].sp * HEART_SCORE;
            // clear the heart
            vram_write(heart[i].x, heart[i].y, ' ');
            heart[i].x = -1;
        }
    }
    // METEOR
    for (i = 0; i < NMETEOR; i++) {
        // if I've bumped into METEOR
        if ((me->x == METEOR[i].x) && (me->y == METEOR[i].y)) {
            // power down my HP
            me->hp--;
            // clear the METEOR
            vram_write(METEOR[i].x, METEOR[i].y, ' ');
            METEOR[i].x = -1;
        }
    }
    // return bonus score to be added
    return bonus;
}

// judge beam shot
int judge_beam( entity *meteor, entity *beam ) {
    int bonus = 0;
    // for all beams
    for (int i = 0; i < NBEAM; i++) {
        // if beam is alive
        if (beam[i].hp > 0) {
            // for all meteors
            for (int j = 0; j < NMETEOR; j++) {
                // if meteor is alive
                if (meteor[j].x > 0) {
                    // if beam bump into meteor
                    if ((beam[i].x == meteor[j].x) && (beam[i].y == meteor[j].y)) {
                        // clear the meteor
                        meteor[j].x = -1;
                        // clear the beam
                        beam[i].hp = 0;
                        vram_write(beam[i].x, beam[i].y, ' ');
                        // increment bonus
                        bonus += METEOR_SCORE;
                    }
                }
            }
        }
    }
    return bonus;
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
    // initialize video and LED GPIO
    init_video_and_led_GPIO();
    // init stdio
    stdio_init_all();
    // init ADC
    adc_init();
    // enable temperature sensor
    adc_set_temp_sensor_enabled(true);
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

    // initialize key GPIO
    init_key_GPIO();

    int countup = 0;
    int score = 0;
    int hi_score = 0;
    int meteor_rate = IMRATE;
    int p_meteor = IPMETEOR;
    uint32_t keys;
    bool blink = true;
    bool initial;
    char mes[VRAM_W];
    entity me, meteor[NMETEOR], heart[NHEART], beam[NBEAM];
    enum State {IDLE, PLAY, OVER} game_state;

    // initialize game state
    game_state = IDLE;

    while (1) {
        // monitoring process speed
        if (countup % 200000 == 0) {
            // flip LED
            flip_led();
        }
        // playing game
        if (game_state == PLAY) {
            // if needs initializing
            if (initial == true) {
                // initialize METEOR
                init_meteor(meteor);
                // initialize heart
                init_heart(heart);
                // initialize beam
                init_beam(beam);
                // initialize me
                me.x = ME_X; me.y = ME_Y; me.hp = ME_HP; me.c = CME;
                // clear score
                score = 0;
                // clear VRAM
                vram_clear();
                // clear initializing flag
                initial = false;
            }
            // my turn
            if (countup % MYRATE == 0) {
                // move me
                move_me(&me);
                // count up score
                score++;
                // judge me
                score += judge_me(&me, meteor, heart);
                // if HP is 0
                if (me.hp <= 0) {
                    // game state is game over
                    game_state = OVER;
                }
                // display score, hi-score and HP
                sprintf(mes, "SCORE%6d HiSCORE%6d HP%2d", score, hi_score, me.hp);
                vram_strings(0, LSCORE, mes);
            }
            // beam turn
            if (countup % BRATE == 0) {
                // shot beam
                shot_beam(beam, &me);
                // judge beam
                score += judge_beam(meteor, beam);
            }
            // METEOR turn
            if (countup % meteor_rate == 0) {
                // move METEOR
                move_entity(meteor, NMETEOR, p_meteor);
                // change METEOR rate
                if (IMRATE - score > MMRATE) {
                    meteor_rate = IMRATE - score;
                } else {
                    meteor_rate = MMRATE;
                }
                // change metro probability
                p_meteor = IPMETEOR + score / 200;
                if (p_meteor >= 99) p_meteor = 99;
            }        
            // heart turn
            if (countup % HTRATE == 0) {
                // move heart
                move_entity(heart, NHEART, 5);
            }        
        }
        // idle, waiting for A button
        if (game_state == IDLE) {
            if (countup % 50000 == 0) {
                // game title
                vram_strings(8,  8, "FLYING METEOR 2!!");
                vram_strings(8, 10, "    by Pa@ART    ");
                vram_write(6, 12, CMETEOR);
                sprintf(mes, ": meteor (%d points)", METEOR_SCORE);
                vram_strings(7, 12, mes);
                vram_write(6, 14, CHEART);
                sprintf(mes, ": heart (%d points)", HEART_SCORE);
                vram_strings(7, 14, mes);
                if (blink == true) {
                    vram_strings(10, 16, "Push A button ");
                } else {
                    vram_strings(10, 16, "              ");
                }
                blink = !blink;
            }
            if (countup % 200000 == 0) {
                // scan keys
                keys = key_scan();
                // if A button is pushed
                if ((keys & AKEY) != 0) {
                    // change game state to PLAY
                    game_state = PLAY;
                    // set initializing flag
                    initial = true;
                }
            }
        }
        // game over, waiting for B button
        if (game_state == OVER) {
            if (countup % 50000 == 0) {
                // game over title
                vram_strings(10, 10, "GAME OVER!!");
                // if score is higher than hi-score
                if (score > hi_score) {
                    hi_score = score;
                    vram_strings(10, 12, "Hi-Score!!");
                }
                if (blink == true) {
                    vram_strings(10, 16, "Push B button ");
                } else {
                    vram_strings(10, 16, "              ");
                }
                blink = !blink;
            }
            if (countup % 200000) {
                // scan keys
                keys = key_scan();
                // if B button is pushed
                if ((keys & BKEY) != 0) {
                    // game state is IDLE
                    game_state = IDLE;
                    // clear VRAM
                    vram_clear();
                }
            }
        }
        countup++;
    }

    return 0;
}
