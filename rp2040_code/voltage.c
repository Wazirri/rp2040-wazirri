#define voltage_c
#include "common.h"
#include "voltage.h"

// Dış değişken bildirimleri
extern int closed_by_ekcalisma; // common.h'de tanımlı olan ekcalismadakika ile kapanma durumu bayrağı



float convert_adc_to_voltage(uint16_t adc_value, uint16_t adc_range, float vref) {
    return (float) adc_value / adc_range * vref * 11;
}



#define FLUCTUATION_THRESHOLD 0.05  // %5 değişim eşiği
int fluctuation_count = 0;
uint64_t last_fluctuation_time = 0; // Son dalgalanma zamanı (mikrosaniye cinsinden)

int get_ADC1() {
    adc_select_input(1);
    
    // Önce eski değeri toplamdan çıkarıyoruz.
    analog_avg0 -= analog_buffer[analog_idx];
    
    // ADC'yi 32 kez okuyup stabilize ediyoruz.
    for (int i = 0; i < 32; i++) {
        adc_read();
    }
    int new_reading = adc_read();
    
    // Yeni okuma değerini buffer'a ekleyip toplamı güncelliyoruz.
    analog_buffer[analog_idx] = new_reading;
    analog_avg0 += analog_buffer[analog_idx];
    
    // Dairesel buffer indeksini güncelle.
    analog_idx = (analog_idx + 1) % MAX_ANALOG_READING;
    if (analog_cnt < MAX_ANALOG_READING) {
        analog_cnt++;
    }
    
    int64_t current_avg = analog_avg0 / analog_cnt;
    
    // Dalgalanma kontrolü: önceki okuma ile karşılaştırma yapılıyor.
    static int previous_reading = 0;
    uint64_t now = time_us_64();
    if (previous_reading != 0) {
        double change_percentage = ((double)(new_reading - previous_reading)) / previous_reading;
        if (change_percentage > FLUCTUATION_THRESHOLD || change_percentage < -FLUCTUATION_THRESHOLD) {
            fluctuation_count++;
            last_fluctuation_time = now;  // Dalgalanma tespit edildiğinde zamanı güncelle.
        }
    } else {
        // İlk okuma olduğunda last_fluctuation_time'ı başlatıyoruz.
        last_fluctuation_time = now;
    }
    previous_reading = new_reading;
    
    return current_avg;
}


void checkFluctuation() {
    uint64_t now = time_us_64();
    
    // Eğer dalgalanma sayısı 3'ün üzerine çıkmışsa, abs_cam_voltage_checker() tetiklenir.
    if (fluctuation_count > 3) {
        printf("Fluctuation detected (%d times). Triggering abs_cam_voltage_checker...\n", fluctuation_count);
        fluctuation_count = 0;
    
        // Dalgalanma gerçekleştiğinde last_fluctuation_time güncellenmiş durumda.
    }
    
    // Eğer 10 saniyedir dalgalanma olmuyorsa, değişkenleri sıfırlayıp gpio 23'ü HIGH yap.
    if ((now - last_fluctuation_time) >= (10ULL * 1000000)) { // 10 saniye = 10,000,000 mikro saniye
        printf("No fluctuation for 10 seconds. Resetting fluctuation variables and setting GPIO 23 to HIGH.\n");
        fluctuation_count = 0;
        gpio_put(23, 1);
        last_fluctuation_time = now;  // Bu bloğun tekrar çalışmasını engellemek için zamanı güncelle.
    }
}


/**
 * Finds the interval of the voltage value.
 * @param voltage The voltage to check
 * @param abs_min The minimum voltage
 * @param abs_max The maximum voltage
 * @param low_min The minimum voltage for low battery
 * @param low_high_th The threshold voltage for low-high battery
 * @param high_min The minimum voltage for high battery
 * @param safety_extra To inhibit rapid reboots around low_min and high_min
 * @return The interval of the voltage. 1 < abs_min < 2 < low_min < 3 < low_min + safety_extra < 4 < low_high_th < 5 < high_min < 6 < high_min + safety_extra < 7  < abs_max < 8
 *  */
 int voltageInterval(float voltage, double abs_min,double abs_low_min_cam, double abs_max, double low_min, 
    double abs_high_min, double abs_high_min_cam, double low_high_th, double high_min,double safety_extra) {

    if ( voltage < abs_min ) {
        return 1;
    }
    if ( voltage < abs_low_min_cam ) {
        return 10;
    }
    //for 12v
    if ( voltage < low_min ) {
        return 2;
    }
    if ( voltage < safety_extra + low_min ) {
        return 3;
    }
    // Valid range for low battery voltage.
    if ( voltage < low_high_th ) {//*100 hiçbir zaman girmemesi için
        return 4;
    }

    if ( voltage < abs_high_min ) {
        return 11;
    }
    if ( voltage < abs_high_min_cam ) {
        return 12;
    }

    //for 24v
    if ( voltage < high_min ) {
        return 5;
    }
    // Valid range for high battery voltage.
    if ( voltage < safety_extra + high_min ) {
        return 6;
    }
    if ( voltage < abs_max ) {
        return 7;
    }


    // Voltage exceeds maximum.
    return 8;
}




int regulatorIsOff = 0; // 0: regülatör açık, 1: regülatör kapalı
uint64_t last_low_voltage_time_rk = 0;
void manageRKRegulator(int current_voltage_interval) {
    // Statik değişkenler ile regülatörün durumunu ve son düşük voltaj anını takip ediyoruz.
    uint64_t now = time_us_64();
    
    // Regülatör zaten kapalıysa, sadece belirli voltaj aralıklarında yeniden açmayı deneyelim
    if(regulatorIsOff){
        if (current_voltage_interval == 4 || current_voltage_interval == 7){
            goto l1;
        }else
            return;
    }
    
    // Kontak durumuna göre voltaj kontrolü
    if (system_state == STATE_IGNITION) {
        // Kontak açık - mevcut davranışı koru
        if (current_voltage_interval == 1 || current_voltage_interval == 11) {
            if (!regulatorIsOff) {
                printf("low_voltage_rk(1) - kontak açık\n");
                closeRK();
                led_c_set(&leds[FLASH_LED_POS], 1);
                regulatorIsOff = 1;
                is_reg_opened = 0; // is_reg_opened değişkeni ile senkronize et
            }
            last_low_voltage_time_rk = now;
        } else {
l1:
            // Regülatörlerin tekrar açılması için kontrol
            // Eğer ekcalismadakika süresi dolduğu için kapatıldıysa, sadece kontak açıldığında açılabilir
            if (regulatorIsOff && (now - last_low_voltage_time_rk >= 5000000UL) && !closed_by_ekcalisma) {
                printf("Voltaj normale döndü, regülatörler açılıyor...\n");
                openRK();
                led_c_set(&leds[FLASH_LED_POS], 0);
                regulatorIsOff = 0;
                is_reg_opened = 1; // is_reg_opened değişkeni ile senkronize et
            } else if (regulatorIsOff && closed_by_ekcalisma) {
                // Ekcalismadakika süresi dolduğu için kapatıldıysa, sadece log yazdır
                printf("Voltaj normale dönse de ekcalisma süresi dolduğu için regulator açılmayacak. Kontak açılması gerekiyor.\n");
            }
        }
    } else if (system_state == STATE_AFTER_IGNITION){
        // Kontak kapalı - voltage kontrolü
        // Eğer current_voltage_interval < 3 veya current_voltage_interval == 5 ise regülatörleri kapat
        if (current_voltage_interval < 3 || current_voltage_interval == 5) {
            if (!regulatorIsOff) {
                printf("low_voltage_rk(1) - kontak kapalı (current_voltage_interval: %d)\n", current_voltage_interval);
                closeRK();
                led_c_set(&leds[FLASH_LED_POS], 1);
                regulatorIsOff = 1;
                is_reg_opened = 0; // is_reg_opened değişkeni ile senkronize et
            }
            last_low_voltage_time_rk = now;
        } 
    }
}


int camregulatorIsOff = 0;
uint64_t last_low_voltage_time = 0;
void manageCamRegulator(int current_voltage_interval) {
    // Statik değişkenler ile regülatörün durumunu ve son düşük voltaj anını takip ediyoruz.
    uint64_t now = time_us_64();
    
    // Kamera regülatörü zaten kapalıysa, sadece belirli voltaj aralıklarında yeniden açmayı deneyelim
    if(camregulatorIsOff){
        if (current_voltage_interval == 4 || current_voltage_interval == 7){
            goto l1;
        }else
            return;
    }
    
    // Kontak durumuna göre voltaj kontrolü
    if (system_state == STATE_IGNITION || system_state == STATE_AFTER_IGNITION) {
        // Kontak açık - mevcut davranışı koru
        if (current_voltage_interval == 10 || current_voltage_interval == 12) {
            // Voltaj belirlenen düşük eşik altında:
            // Regülatör kapalı değilse kapatıyoruz.
            if (!camregulatorIsOff) {
                printf("low_voltage_cam(1) - kontak açık\n");
                sleep_ms(1000);
                gpio_put(23,0);
                led_c_set(&leds[FLASH_LED_POS], 1);
                camregulatorIsOff = 1;
                cam_reg_closed = 1; // cam_reg_closed değişkeni ile senkronize et
            }
            last_low_voltage_time = now;
        } else {
l1:
            if (camregulatorIsOff && (now - last_low_voltage_time >= 5000000UL)) {
                gpio_put(23,1);
                led_c_set(&leds[FLASH_LED_POS], 1);
                camregulatorIsOff = 0;
                cam_reg_closed = 0; // cam_reg_closed değişkeni ile senkronize et
            }
        }
    } else {
        // Kontak kapalı - 11V eşiğini kullan
        if (current_voltage_interval <= 2 || current_voltage_interval == 10) { // Interval 1, 10 ve 2 için (11V'nin altındaki tüm voltajlar için)
            if (!camregulatorIsOff) {
                printf("low_voltage_cam(1) - kontak kapalı (<11V)\n");
                sleep_ms(1000);
                gpio_put(23,0);
                led_c_set(&leds[FLASH_LED_POS], 1);
                camregulatorIsOff = 1;
                cam_reg_closed = 1; // cam_reg_closed değişkeni ile senkronize et
            }
            last_low_voltage_time = now;
        } else {
            if (camregulatorIsOff && (now - last_low_voltage_time >= 5000000UL)) {
                gpio_put(23,1);
                led_c_set(&leds[FLASH_LED_POS], 1);
                camregulatorIsOff = 0;
                cam_reg_closed = 0; // cam_reg_closed değişkeni ile senkronize et
            }
        }
    }
}
