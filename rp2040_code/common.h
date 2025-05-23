#ifndef	common_h
#define	common_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include	<stdint.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/watchdog.h"

//#define	DEBUG

#define BAUD_RATE       115200
#define UART_ID         uart0

#define IGNITION_PIN    3
#define FLASH_PIN       7
#define HORN_PIN        8
#define PANIC           25
#define VIN_PIN         27

#define MAX_READ_LENGTH     128
#define MAX_ANALOG_READING  128

#define STATE_BEFORE_IGNITION   0
#define STATE_IGNITION          1
#define STATE_AFTER_IGNITION    2
#define STATE_RK_CLOSER    3

#define ALL_REGS_OFF    0
#define CAM_REGS_OFF    1
#define ALL_REGS_ON     2


#define LED_FREQ        50
#define LED_PERIOD_MS   (1000 / LED_FREQ)
#define LED_PERC        0.5
#define LED_ON_MS       (LED_PERIOD_MS * LED_PERC)

#define WATCHDOG_PERIOD_MS 5000

#define LOOP_OFF_MS 1000
#define LOOP_ON_MS  1000

#define WAIT_FIRST_MIN  6
#define DEFAULT_RK_TMOUT_MIN  1

// VOLTAGE CONTROL DEFINES

#define VOLTAGE_CHECK_S             2
#define VOLTAGE_INFO_CHECK_S        1
#define VOLT_SAFETY_EXTRA           0.4
#define VOLT_ABS_MIN                7.3
#define VOLT_ABS_MIN_CAM            8.3
#define VOLT_ABS_MAX                100
#define VOLT_LOW_MIN                10.5
#define VOLT_HIGH_MIN               19
#define VOLT_LOW_HIGH_TH            16
#define VOLT_HIGH_ABS_MIN           17
#define VOLT_HIGH_ABS_MIN_CAM       18
#define RAPID_FLASH_PERIOD_MS       50
#define LOW_VOLTAGE_INTERVAL_MS     500
#define LOW_VOLTAGE_INITIAL_WAIT_S  25
#define LOW_VOLTAGE_FLUCT  5
#define EKCALISMA_RK_KAPANMA_BEKLEME_S 60 // Waiting time after rk poweroff sent for the rk to be closed down.

#define NUM_PINS 2

typedef struct {
    uint8_t 	g;      //gpio num
    uint8_t 	o;      //on/off
    int		is_loop;
    uint8_t 	loop_on;
} LED_TYPE;

#define EKCALISMA_CONTROLLED_FLASH 0
#define LOW_VOLT_CONTROLLED_FLASH  1


#ifdef	DEBUG
	#define 	UART_TX_PIN		0  
	#define 	UART_RX_PIN		1
	
	#define	MAX_KSS_V		16
	#define	CLK_PIN		8
	#define	INH_PIN		7
	#define	LD_PIN		9
	#define	QH_PIN		6
#else
	#define 	UART_TX_PIN		16  
	#define 	UART_RX_PIN		17

	#define	MAX_KSS_V		16
	#define	CLK_PIN		11
	#define	INH_PIN		4
	#define	LD_PIN		10
	#define	QH_PIN		5
#endif


#define SIM_LED_POS 0
#define GPS_LED_POS 1
#define FLASH_LED_POS 2
#define KAYIT_LED_POS 3
#define DISK_LED_POS 4

#define LOG_PRINT_INTERVAL_MS   10000
#define LOG_MAX_LENGTH          32
#define VOLT_DROP_BLCHAR        'D'
#define REG_CLOSE_NOIGN_BLCHAR  'N'
#define REG_CLOSE_IGN_BLCHAR    'I'
#define REG_CLOSE_EKCAL_BLCHAR  'E'
#define REG_CLOSE_FLUC_BLCHAR   'F'
#define REG_OPEN_BLCHAR         'O'
#define RESET_RK_BLCHAR         'R'

#define BATTERY_12V         0
#define BATTERY_24V         1
#define BATTERY_UNDEFINED   2

#define VOLT_ABS_MIN_12V            8
#define VOLT_CAM_THRESHOLD_12V      9
#define VOLT_LOW_12V                10

#define VOLT_THRESHOLD_12V_24V      16

#define VOLT_ABS_MIN_24V            17
#define VOLT_CAM_THRESHOLD_24V      18
#define VOLT_LOW_24V                19

// #define VOLT_ABS_MIN_CAM            9
// #define VOLT_ABS_MAX                100
// #define VOLT_LOW_MIN                (10)
// #define VOLT_HIGH_MIN               19
// #define VOLT_LOW_HIGH_TH            16
// #define VOLT_HIGH_ABS_MIN           17
// #define VOLT_HIGH_ABS_MIN_CAM       18

#ifdef common_c

    // Bundan sonra versiyon notlarını lütfen ReadME'de güncelleyiniz.
    char        version[128]="19_17_05_2025"; // By uur
    uint8_t     system_state = STATE_BEFORE_IGNITION;
    int         regulator_state = ALL_REGS_OFF;
    int         battery_standart = BATTERY_12V;

    int         loop_s = 0;

    // ATTENTION: DO NOT CHANGE THE ORDER OF THE LEDS UNLESS YOU CHANGE THE DEFINED LED POSITIONS
    LED_TYPE       leds[5] = {{6,0,1,0},{9,0,1,0},
                            {12,0,0,0},{13,0,0,0},{14,0,1,0}};

    // const       uint8_t num_leds = 7;
    const       uint8_t num_leds = 5;
    uint64_t    led_fl_tm = 0;
    uint8_t     led_s = 0;
    int         ekcalismadakika = 15;
    uint64_t    first10min_timer;

    int         is_reg_opened=0;
    int         cam_reg_closed=0;
    
    int         poweroff_sent = 0;
    int         closed_by_ekcalisma = 0; // Ekcalismadakika süresi dolduğu için regülatörlerin kapatıldığını belirten bayrak
    
    size_t      json_index = 0;
    
    uint16_t    analog_buffer[MAX_ANALOG_READING];
    char        received_json[1024];
    uint64_t    analog_idx=0, analog_cnt=0;
    uint64_t    analog_avg0=0;

    uint64_t    flash_led_start_time;
    uint64_t    voltage_info_start_time;
    uint64_t    contact_close_tm = 0;
    uint64_t    voltage_start_time;
    uint64_t    voltage_start_checker = 0;

    uint64_t    low_voltage_tm = 0;
    uint64_t    abs_voltage_tm = 0;
    int         low_voltage_count = 0;
    int         abs_voltage_count = 0;
    
    int         voltage_LED_flasher = 0;

    uint64_t    last_message_time_from_rk = 0;
    int         rk_alert = 0;

    uint64_t    ignition_led_time = 0;
    uint64_t    heartbeat_led_time = 0;
    int         ignition_led_state = 0;
    int         heartbeat_led_state = 0;

    char        baseLog[LOG_MAX_LENGTH] = {0};
    int         baseLogNum = 0;
    int         baseLogStart = 0;
    uint64_t    log_print_tm = 0;

    int         closerk_process = 0;
    int         closerk_timer = 0;

    const uint PIN_LIST[NUM_PINS] = {3,25};
    bool prev_state[NUM_PINS];
    bool current_state[NUM_PINS];

    int flash_LED_controller = LOW_VOLT_CONTROLLED_FLASH;
#else

    extern char     version[128];
    extern uint8_t  system_state;
    extern int      regulator_state;
    extern int      battery_standart;

    extern int      loop_s;
    extern LED_TYPE    leds[7];
    extern const uint8_t num_leds;
    extern uint64_t led_fl_tm;
    extern uint8_t  led_s;
    extern int      ekcalismadakika;
    extern uint64_t first10min_timer;

    extern int      is_reg_opened;
    extern int      cam_reg_closed;
    extern int      poweroff_sent;

    extern size_t   json_index;

    extern uint16_t analog_buffer[MAX_ANALOG_READING];
    extern char     received_json[1024];
    extern uint64_t analog_idx;
    extern uint64_t analog_cnt;
    extern uint64_t analog_avg0;

    extern uint64_t flash_led_start_time;
    extern uint64_t voltage_info_start_time;
    extern uint64_t last_message_time_from_rk;
    extern uint64_t contact_close_tm;
    extern uint64_t voltage_start_time;
    extern uint64_t voltage_start_checker;

    extern uint64_t low_voltage_tm;
    extern uint64_t abs_voltage_tm;
    
    extern int      low_voltage_count;
    extern int      abs_voltage_count;
    extern int      voltage_LED_flasher;

    extern int      rk_alert;

    extern uint64_t ignition_led_time;
    extern uint64_t heartbeat_led_time;
    
    extern int ignition_led_state;
    extern int heartbeat_led_state;

    extern char     baseLog[LOG_MAX_LENGTH];
    extern int      baseLogNum;
    extern int      baseLogStart;
    extern uint64_t log_print_tm;

    extern int      closerk_process;
    extern int      closerk_timer;

    extern int      flash_LED_controller;

#endif


uint8_t isLED(uint8_t l);

void led_c_set(LED_TYPE* led, uint8_t on);
void flick_leds(uint8_t s);
void flicker_LEDs();

void loop_LEDs();

void init_adc();
void init_gpios();
void closeRK();
void openRK();

void flash_led();

uint64_t tm_passed_us( uint64_t end_time, uint64_t start_time);
void log_base(char s);
void printLog();

void init_pins();
void pins_loop();
#endif