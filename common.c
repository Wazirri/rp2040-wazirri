#define common_c
#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "common.h"
#include "voltage.h"

//DEBUG
// uint8_t rs485_recieved_in_last_sec = 0;
void led_c_set(LED_TYPE* led, uint8_t on) {
    if(!on){
        gpio_put(led->g, 0);
        led->o = 0;
        return;
    }
    led->o = 1;
    // CLOSE TO FLICKER
    gpio_put(led->g, on);
}

int last_flash_led_state = 0;
void flash_led(){
    led_c_set(&leds[FLASH_LED_POS], !last_flash_led_state);
    last_flash_led_state = !last_flash_led_state;
}


void flick_leds(uint8_t s) {
    for (int i = 0; i < num_leds; i++) {
        if(!leds[i].loop_on && leds[i].is_loop)
            continue;
        if (leds[i].o)
            gpio_put(leds[i].g, s);
    }
}

void flicker_LEDs() {
    //uint64_t n = time_us_64();
    
    ////// OPEN TO FLICKER
    // if (led_s){
    //     if ((n - led_fl_tm) > LED_ON_MS * 1000) {
    //         led_s = 0;
    //         flick_leds(0);
    //         led_fl_tm = n;
    //     }
    // }
    // else{
    //     if ((n - led_fl_tm) > (LED_PERIOD_MS - LED_ON_MS) * 1000) {
    //         led_s = 1;
    //         flick_leds(1);
    //         led_fl_tm = n;
    //     }
    // }
    //////////////////////
}



void loop_LEDs(){
    static uint64_t last_toggle_time = 0;
    uint64_t current_time = time_us_64();
    
    if ((current_time - last_toggle_time) > ( (loop_s ? LOOP_OFF_MS : LOOP_ON_MS) * 1000 )) {
        for (int i = 0; i < num_leds; i++) {
            if (leds[i].is_loop && leds[i].o) {
                leds[i].loop_on = loop_s;
                gpio_put(leds[i].g, leds[i].loop_on);
            }
        }
        loop_s = !loop_s;
        last_toggle_time = current_time;
    }
}

void init_adc() {
    adc_init();
    adc_gpio_init(VIN_PIN);
    bzero(analog_buffer, MAX_ANALOG_READING);
    analog_idx = 0;
    analog_avg0 = 0;
}

void init_gpios(){
    gpio_init(2);
    gpio_set_dir(2, GPIO_OUT);

    //RK REGULATOR PINS
    gpio_init(19);
    gpio_init(20);
    gpio_init(21);
    gpio_init(22);
    gpio_init(23);
    // gpio_set_function(19, GPIO_FUNC_SIO);
    // gpio_set_function(20, GPIO_FUNC_SIO);
    // gpio_set_function(21, GPIO_FUNC_SIO);
    // gpio_set_function(22, GPIO_FUNC_SIO);
    // gpio_set_function(23, GPIO_FUNC_SIO);

    gpio_set_dir(19, GPIO_OUT);
    gpio_set_dir(20, GPIO_OUT);
    gpio_set_dir(21, GPIO_OUT);
    gpio_set_dir(22, GPIO_OUT);
    gpio_set_dir(23, GPIO_OUT);

    // gpio_pull_down(19);
    // gpio_pull_down(20);
    // gpio_pull_down(21);
    // gpio_pull_down(22);
    // gpio_pull_down(23);

    // gpio_set_drive_strength(19, GPIO_DRIVE_STRENGTH_2MA);
    // gpio_set_drive_strength(20, GPIO_DRIVE_STRENGTH_2MA);
    // gpio_set_drive_strength(21, GPIO_DRIVE_STRENGTH_2MA);
    // gpio_set_drive_strength(22, GPIO_DRIVE_STRENGTH_2MA);
    // gpio_set_drive_strength(23, GPIO_DRIVE_STRENGTH_2MA);
    
    // //DEBUG 
    // gpio_init(15);
    // gpio_set_dir(15, GPIO_OUT);
    // gpio_put(15, 0);
    // //RK REGULATOR PINS

    //LED PINS
    for (int i = 0; i < num_leds; i++) {
        gpio_init(leds[i].g);
        gpio_set_dir(leds[i].g, GPIO_OUT);
    }
    //LED PINS
    
    gpio_init(IGNITION_PIN);
    gpio_set_dir(IGNITION_PIN, GPIO_IN);
    gpio_init(PANIC);
    gpio_set_dir(PANIC, GPIO_IN);
    gpio_init(HORN_PIN);
    gpio_set_dir(HORN_PIN, GPIO_OUT);
    gpio_pull_down(HORN_PIN);
    gpio_set_drive_strength(HORN_PIN, GPIO_DRIVE_STRENGTH_2MA);
    gpio_init(FLASH_PIN);
    gpio_set_dir(FLASH_PIN, GPIO_OUT);
    gpio_pull_down(FLASH_PIN);
    gpio_set_drive_strength(FLASH_PIN, GPIO_DRIVE_STRENGTH_2MA);
}

void closeRK(){
    gpio_put(2, 0); //RP KENDINI ACIP KAPATMASI ICIN
    gpio_put(19, 0);
    gpio_put(20, 0);
    gpio_put(21, 0);
    gpio_put(22, 0);
    gpio_put(23, 0);
    log_base(REG_CLOSE_NOIGN_BLCHAR);
    for(int i=0;i<num_leds;i++){
        if (leds[i].o) {
            led_c_set(&leds[i], 0);
        }
    }
    // Synchronize all state variables related to regulator control
    is_reg_opened = 0;
    cam_reg_closed = 1;
    extern int regulatorIsOff;
    extern int camregulatorIsOff;
    regulatorIsOff = 1;
    camregulatorIsOff = 1;
    regulator_state = ALL_REGS_OFF;
    printf("RK closed: all state variables synchronized\n");
}

void openRK(){
    gpio_put(2, 1);  
    gpio_put(19, 1);
    gpio_put(20, 1);
    gpio_put(21, 1);
    gpio_put(22, 1);
    gpio_put(23, 1); 
    led_c_set(&leds[FLASH_LED_POS], 0);  
    // Synchronize all state variables related to regulator control
    is_reg_opened = 1;
    cam_reg_closed = 0;
    extern int regulatorIsOff;
    extern int camregulatorIsOff;
    regulatorIsOff = 0;
    camregulatorIsOff = 0;
    poweroff_sent = 0;
    // Ekcalismadakika nedeniyle kapatılmış olabilecek regülatörleri sıfırla
    closed_by_ekcalisma = 0;
    last_message_time_from_rk = time_us_64();
    regulator_state = ALL_REGS_ON;
    printf("RK opened: all state variables synchronized (closed_by_ekcalisma reset)\n");
}



/**Checks whether a GPIO is defined to be an LED.
 * @param l The GPIO number to check
 * @return (i + 1) if the GPIO is an LED, 0 otherwise
*/
uint8_t isLED(uint8_t l) {
    for (int i = 0; i < num_leds; i++) {
        if (leds[i].g == l) {
            return i + 1;
        }
    }
    return 0;
}



/**
 * Returns the time passed since the given time.
 * @param end_time The time to subtract from.
 * @param start_time The time to start from.
 * @return The time passed in microseconds.
 */
uint64_t tm_passed_us( uint64_t end_time, uint64_t start_time) {
    if ( end_time < start_time ) {
        return 0xFFFFFFFFFFFFFFFF - start_time + end_time;
    }
    return end_time - start_time;
}

void log_base(char s) {
    if ( baseLogNum >= LOG_MAX_LENGTH ) {
        baseLog[baseLogStart] = s;
        baseLogStart++;
        return;
    }
    baseLog[baseLogNum++] = s;
}

/**
 * Prints the log. It includes the current time in approximate seconds and the base log which includes the regulator related actions.
 */
void printLog() {
    printf("LOG(T:%llu,", 	time_us_64() >> 20 );
    for ( int i = 0; i < baseLogNum; i++ ) {
        printf("%c", baseLog[(i + baseLogStart) % LOG_MAX_LENGTH]);
    }
    printf(")\r\n");
}

void init_pins() {
    for (int i = 0; i < NUM_PINS; i++) {
        gpio_init(PIN_LIST[i]);
        gpio_set_dir(PIN_LIST[i], GPIO_IN);
        gpio_pull_up(PIN_LIST[i]);
        gpio_set_drive_strength(PIN_LIST[i], GPIO_DRIVE_STRENGTH_2MA);
    }
    
    for (int i = 0; i < NUM_PINS; i++) {
        prev_state[i] = gpio_get(PIN_LIST[i]);
    }

}


void pins_loop() {
    
    for (int i = 0; i < NUM_PINS; i++) {
        current_state[i] = gpio_get(PIN_LIST[i]);
    }

    // Her pin için durum değişikliklerini kontrol et
    for (int i = 0; i < NUM_PINS; i++) {
        // Durum değiştiyse, printf ile durumu yazdır
        if (current_state[i] != prev_state[i]) {
            printf("gpioset(%d,%d)\n", PIN_LIST[i], current_state[i]);
            prev_state[i] = current_state[i];
        }
    }
}