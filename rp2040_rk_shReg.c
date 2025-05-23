#define rp2040_rk_shReg_c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/watchdog.h"
#include "common.h"
#include "kss.h"
#include "voltage.h"

void 	process_command(const char *command);
void 	read_uart();
void 	low_volt_routine();

void 	rk_close_handler();
void 	heartbeat_led();
void 	low_voltage_LED_flasher();

// int	kss_loop_v2(uint64_t semi_period, shiftr *sp);
int	heartbeat_led_count = 0;

// Dış değişken bildirimleri
extern int closed_by_ekcalisma; // common.h'de tanımlı olan ekcalismadakika ile kapanma durumu bayrağı


int main() {
	//uart connection with RK is uart4 and /dev/ttyS4
	stdio_uart_init_full(uart0, 115200, UART_TX_PIN, UART_RX_PIN);
	init_gpios();
	init_adc();		//it is used for checking voltage
	init_pins();	// the digital input pins

	watchdog_reboot(0, 0, WATCHDOG_PERIOD_MS); //PERIOD_MS sürede watchdog güncellenmezse reboot et.
	watchdog_update();

	voltage_start_time = time_us_64();
	voltage_info_start_time = time_us_64();
	flash_led_start_time = time_us_64();
	first10min_timer = time_us_64();
    
	last_message_time_from_rk = time_us_64 ();

	system_state = STATE_BEFORE_IGNITION;
	float voltage;

	multicore_launch_core1(side_core);

	while (true) {
		watchdog_update();
		//flicker_LEDs();
		loop_LEDs();
		get_ADC1();
		pins_loop();
		rk_close_handler();
		voltage = convert_adc_to_voltage(get_ADC1(), 4095, 3.3);
		int current_voltage_interval = voltageInterval(voltage, VOLT_ABS_MIN,VOLT_ABS_MIN_CAM, VOLT_ABS_MAX, VOLT_LOW_MIN ,
		VOLT_HIGH_ABS_MIN,VOLT_HIGH_ABS_MIN_CAM , VOLT_LOW_HIGH_TH, VOLT_HIGH_MIN, VOLT_SAFETY_EXTRA);
		
		
		heartbeat_led();

		//print voltage every VOLTAGE_INFO_CHECK_S seconds
		if ( tm_passed_us(time_us_64(), voltage_info_start_time) > VOLTAGE_INFO_CHECK_S * 1000 * 1000) { 
			printf("voltage(%f)\n", voltage); 
			voltage_info_start_time = time_us_64();
			//printf("gpio19(%d)",gpio_get(19));
		}
		// if ( tm_passed_us(time_us_64(), log_print_tm) > LOG_PRINT_INTERVAL_MS * 1000) { 
		// 	printLog();
		// 	log_print_tm = time_us_64();
		// }
		//checkFluctuation();
		low_voltage_LED_flasher();

		switch (system_state) {
			case STATE_BEFORE_IGNITION:
				// May be used to check gsm for whether there is a wakeup request
				if (tm_passed_us(time_us_64(), voltage_start_time) > VOLTAGE_CHECK_S * 1000 * 1000 ) {//just waiting in the power is just plugged in
					// Voltaj yeterli durumdaysa ve kontak açıksa veya ekcalisma nedeniyle kapatıldıysa ve şimdi kontak açıldıysa
					if ( ( current_voltage_interval == 4 ) || ( current_voltage_interval == 7 ) ) { // 4 = enough for low voltage battery, 7 = enough for high voltage battery
						if (gpio_get(IGNITION_PIN)) {
							printf("VOLTAJ DEĞERLERİ YETERLİ, REGULATORLER ACILIYOR...\n ");
							// Regülatörler kapalıysa aç
							if (!is_reg_opened) {
								openRK();
								log_base(REG_OPEN_BLCHAR);
								// Eğer ekcalismadakika nedeniyle kapatıldıysa bayrağı sıfırla
								if (closed_by_ekcalisma) {
									closed_by_ekcalisma = 0;
									printf("Ekcalisma nedeniyle kapatılan cihaz kontak açılarak tekrar başlatıldı.\n");
								}
							}
							system_state = STATE_IGNITION;
							low_voltage_count = 0;
							voltage_LED_flasher = 0;
						}
					}
					else{
						voltage_LED_flasher = 1;
					}
					voltage_start_time = time_us_64();
				}
			break;
		
		case STATE_IGNITION:
			if ( !gpio_get(IGNITION_PIN) ) {
				system_state = STATE_AFTER_IGNITION;
				contact_close_tm = time_us_64();
				ignition_led_time = time_us_64();
				ignition_led_state = 0;
				break;
			}

			// Break if voltage is unstable
			if ( analog_cnt < MAX_ANALOG_READING ) { 
				break;
			}

			manageRKRegulator(current_voltage_interval);

			if ( is_reg_opened ) {
				read_uart();
				manageCamRegulator(current_voltage_interval);
			}else{
				
			}

		break;
		
		case STATE_AFTER_IGNITION:
		
			if ( gpio_get(IGNITION_PIN) ) {
				// Kontak tekrar açıldı
				
				// Regulator kapalıysa veya ekcalismadakika nedeniyle kapatıldıysa aç
				if (!is_reg_opened || closed_by_ekcalisma) {
					printf("Kontak tekrar açıldı, regulatorler açılıyor...\n");
					openRK();
					log_base(REG_OPEN_BLCHAR);
					
					// Ekcalismadakika nedeniyle kapatıldıysa sıfırla
					if (closed_by_ekcalisma) {
						closed_by_ekcalisma = 0;
						printf("Ekcalisma nedeniyle kapatılan cihaz kontak açılarak tekrar başlatıldı.\n");
					}
				}
				
				// Poweroff gönderilmişse sıfırla
				if (poweroff_sent) {
					poweroff_sent = 0;
				}
				
				system_state = STATE_IGNITION;
				led_c_set(&leds[FLASH_LED_POS], 0);
				break;
			}

			uint64_t kalancalisma;

			manageRKRegulator(current_voltage_interval);
			if ( is_reg_opened ) {
				read_uart();
				manageCamRegulator(current_voltage_interval);
			}else{
				
			}

			if ( !is_reg_opened) {
				break;
			}	
			
			// Change ignition led state every 1 second
			if ( tm_passed_us(time_us_64(), ignition_led_time) > 1 * 1000 * 1000) {
				ignition_led_state = !ignition_led_state;
				ignition_led_time = time_us_64();
				kalancalisma = ((ekcalismadakika * 60 * 1000000) - (time_us_64() - contact_close_tm)) / 1000000;
				printf("kalancalisma(%llu)\n", kalancalisma);
			}
	
			// If voltage is low
			// if ( (current_voltage_interval < 3 ) || ( current_voltage_interval == 5) ) {
			// 	voltage_start_time = time_us_64();
			// 	low_volt_routine();
			// 	voltage_LED_flasher = 1;
			// }
			// else{
			// 	voltage_LED_flasher = 0;
			// }

			// Kontak kapalıyken kalan çalışma süresi kontrolü
			uint64_t gecen_sure = tm_passed_us(time_us_64(), contact_close_tm);
			uint64_t ek_calisma_sure = ekcalismadakika * 60 * 1000 * 1000; // ek çalışma süresi mikrosaniye cinsinden
			uint64_t poweroff_bekleme_sure = (ekcalismadakika + 1) * 60 * 1000 * 1000; // poweroff sonrası bekleme süresi

			// Ek çalışma süresi içindeyiz
			if (gecen_sure < ek_calisma_sure) {
				// Normal çalışmaya devam et
			}
			else { 
				// Ek çalışma süresi doldu
				
				// Poweroff komutu henüz gönderilmediyse gönder
				if (!poweroff_sent) {
					poweroff_sent = 1;
					printf("poweroff(1)\n");
					led_c_set(&leds[FLASH_LED_POS], 1);
					printf("Ek çalışma süresi doldu. RK'ya poweroff komutu gönderildi.\n");
					printf("Regülatörler %d dakika sonra kapatılacak.\n", 1);
				}
				
				// Regülatörler henüz açıksa ve tam olarak ek çalışma + 1 dakika geçtiyse kapat
				if (is_reg_opened && gecen_sure >= poweroff_bekleme_sure) {
					printf("Poweroff sonrası 1 dakika bekleme süresi doldu. Regülatörler kapatılıyor.\n");
					closeRK();
					log_base(REG_CLOSE_EKCAL_BLCHAR);
					// Ekcalismadakika nedeniyle kapatıldığını belirtiyoruz
					closed_by_ekcalisma = 1;
				}
			}
			break;

		case STATE_RK_CLOSER:
		
			break;
		default:
			break;
		}
    }
	return 0;
}

void process_command(const char *command) {
	int pin, state;
	int incoming_value;
	char	name[64], value[64];
	if (sscanf(command, "%[^()](%s)", name, value) == 2) {
		if(strcasecmp(name+1, "gpioset")==0){
			if (sscanf(value, "%d,%d", &pin, &state) == 2) {
			int a = isLED(pin);
			if (a){
				led_c_set(&leds[a - 1], state);
				return;
			}
			if (gpio_get_function(pin) != GPIO_FUNC_SIO) {
				gpio_init(pin);
				gpio_set_dir(pin, GPIO_OUT);
			}
			printf("gpioset(%d,%d)\n", pin, state);
			gpio_put(pin, state);
			printf("GPIOSET EDILDI PIN DURUMU ==>>  (%d,%d)\n", pin, gpio_get(pin));
			}
		}
		else if(strcasecmp(name + 1, "ekcalismadakika") == 0){
			if (sscanf(value, "%d", &incoming_value) == 1) {
			ekcalismadakika= incoming_value;
			printf("ekcalismadakika(%d)\n",ekcalismadakika);
			}
		}
		else if(strcasecmp(name + 1, "getpin") == 0){
			if (sscanf(value, "%d", &pin) == 1) {
			printf("getpin(%d,%d)\n",pin, gpio_get(pin));
			}
		}
		else if(strcasecmp(name + 1, "getversion") == 0){
			printf("getversion(%s)\n",version);
		}
		else if(strcasecmp(name + 1, "closerk") == 0){
			closerk_process=1;
			closerk_timer = time_us_64();
			printf("---------- CLOSE RK GELDI--------\n");
			printf("CLOSERK_PROCESS(%d)\n",closerk_process);
			watchdog_update();
			//log_base(RESET_RK_BLCHAR);
		}
		else if(strcasecmp(name + 1, "heartbeat") == 0){
			
			printf("---------- HEARTBEAT --------\n");
			
	
		}
		else if(strcasecmp(name + 1, "rkboot") == 0){
			printf("__rkboot recieved.\r\n");
			sleep_ms(2000);
			closeRK();
			sleep_ms(1000);
			openRK();
		}
    	} else{
      	printf("parse failed command: %s\n",command);
    	}
}


int c;
int is_rk_sent_message = 0;
void read_uart(){
	if((time_us_64() - first10min_timer >  WAIT_FIRST_MIN * 60 * 1000 * 1000)  || is_rk_sent_message==1) {
		if(!poweroff_sent && (time_us_64() - last_message_time_from_rk > DEFAULT_RK_TMOUT_MIN * 60 * 1000 * 1000)) {
			gpio_put(12, 1);
			closerk_process=1;
			log_base(RESET_RK_BLCHAR);
			rk_alert = 1;
		}
	}

	if ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
      	last_message_time_from_rk = time_us_64();
		if (json_index < sizeof(received_json)) {
			if(c == 10 || c == 13) {
				received_json[json_index] = 0;
				json_index = 0;
				is_rk_sent_message=1;
				process_command(received_json);
				if(rk_alert == 1) {
					printf("rkalert(1)\n");
					rk_alert=0;
				}
			}
			received_json[json_index++] = c;
		} else {
			received_json[sizeof(received_json) - 1] = 0;
			json_index = 0;
		}
	}
}


void rk_close_handler() {
	if(!closerk_process){
		watchdog_update();
		return;
	}
	
	if(closerk_process==1){
		system_state = STATE_RK_CLOSER;
		if ( tm_passed_us(time_us_64(), closerk_timer) > ( 4 *  1000 * 1000 )) {
			poweroff_sent = 1;
			printf("poweroff(1)\n");
			closerk_process=2;
			closerk_timer = time_us_64();
			return;
		}	
	}
	if(closerk_process==2){
		if ( tm_passed_us(time_us_64(), closerk_timer) > ( 10 * 1000 * 1000 )) {
			closeRK();
			printf("--- RK KAPATILDI ---\n");
			closerk_process=3;
			closerk_timer = time_us_64();
			return;
		}
	}
	if(closerk_process==3){
		if ( tm_passed_us(time_us_64(), closerk_timer) > ( 4 * 1000 * 1000 )) {
			openRK();
			printf("--- RK ACILDI ---\n");
			closerk_process=4;
			closerk_timer = time_us_64();
			return;
		}
	}
	if(closerk_process==4){
		printf("RK KAPATIP ACMA ISLEMI TAMAMLANDI\n");
		system_state = STATE_IGNITION;
		closerk_process=0;
		return;
	}
    
}


void heartbeat_led() {

	if (heartbeat_led_count == 0) { 
		heartbeat_led_time = time_us_64();
		//led_c_set(&leds[FLASH_LED_POS], 0);
		gpio_put(13,0);
		heartbeat_led_count++;
	}
	if(heartbeat_led_count==1){
		if ( tm_passed_us(time_us_64(), heartbeat_led_time) > 10 * 1000 * 1000) {
			heartbeat_led_time = time_us_64();
			heartbeat_led_count++;
			gpio_put(13,1);

			//led_c_set(&leds[FLASH_LED_POS], 1);
			
		}
	}
	if(heartbeat_led_count==2){
		if ( tm_passed_us(time_us_64(), heartbeat_led_time) > 1 * 1000 * 1000) {
			heartbeat_led_time = time_us_64();
			heartbeat_led_count=0;
			gpio_put(13,0);

			//led_c_set(&leds[FLASH_LED_POS], 0);
		}
	}
}


void low_volt_routine() {
	if(STATE_BEFORE_IGNITION){
		return;
	}

	if (low_voltage_count == 0) { // Initial instance of low voltage
		low_voltage_tm = time_us_64();
		low_voltage_count++;
		printf("voltage(0)\n");
		log_base(48 + low_voltage_count);
		return;
	}
	if (low_voltage_count == 1) { // Makes sure that voltage is low for at least LOW_VOLTAGE_INITIAL_WAIT_S seconds
		if ( tm_passed_us(time_us_64(), low_voltage_tm) < ( LOW_VOLTAGE_INITIAL_WAIT_S * 1000 * 1000 )) {
			return;
		}
		low_voltage_tm = time_us_64();
		low_voltage_count++;
		printf("voltage(0)\n");
		log_base(48 + low_voltage_count);
		return;
	}	
	if (low_voltage_count == 2) { // The last couple checks for voltage
		if ( tm_passed_us(time_us_64(), low_voltage_tm) < ( LOW_VOLTAGE_INTERVAL_MS * 1000 )) {
			return;
		}
		low_voltage_tm = time_us_64();
		low_voltage_count++;
		printf("voltage(0)\n");
		log_base(48 + low_voltage_count);
		return;
	}
	if (low_voltage_count == 3) { // The last couple checks for voltage
		if (is_reg_opened) { // If voltage is still low, close the rk
			closeRK();
			log_base(REG_CLOSE_IGN_BLCHAR);
			low_voltage_count = 0;
			return;
		}
	}
	if (low_voltage_count == 5) { // The last couple checks for voltage
		if (is_reg_opened) { // If voltage is still low, close the rk
			closeRK();
		}
		if ( tm_passed_us(time_us_64(), low_voltage_tm) < ( 10 * 1000 * 1000 )) {
			return;
		}
		log_base(REG_CLOSE_IGN_BLCHAR);
		low_voltage_count = 0;
	}
	
	return;
}


void low_voltage_LED_flasher() {

	if ( flash_LED_controller != LOW_VOLT_CONTROLLED_FLASH) {
		return;
	}
	if ( !voltage_LED_flasher ) {
		led_c_set(&leds[FLASH_LED_POS],0);
		return;
	}

	if ( tm_passed_us(time_us_64(), flash_led_start_time) > ( RAPID_FLASH_PERIOD_MS * 1000 )) {
		flash_led();
		flash_led_start_time = time_us_64();
	}
}