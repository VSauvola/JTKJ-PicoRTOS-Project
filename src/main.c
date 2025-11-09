
// TODO:
// - led alkuarvoon false
// - näyttö alussa pois päältä
// - debug tekstit näkyviin (tilat, missä kohti ollaan menossa)
//     --> tulostus konsoli-ikkunalle (System_printf(..) + System_flush(..) tai printf(..))

#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include "tkjhat/sdk.h"
#include <tusb.h>
#include "/home/student/jtkj-projects/JTKJ-PicoRTOS-Project/libs/usb-serial-debug/include/usbSerialDebug/helper.h"
#include "../../../.freertos/include/projdefs.h"


// Default stack size for the tasks. It can be reduced to 1024 if task is not using lot of memory.
#define DEFAULT_STACK_SIZE  2048 
#define BUFFER_SIZE         49
#define CDC_ITF_TX          1

//Add here necessary states
enum state { WAITING=1, READ_SENSOR, NEW_MSG, UPDATE};
enum state programState = WAITING;

char txbuf[BUFFER_SIZE];     //tx bufferi = 10*4+5 = 45
char rxbuf[BUFFER_SIZE];        //tulo bufferi
volatile bool sw2_pressed = false;
/*
static void btn_fxn(uint gpio, uint32_t eventMask) {

Keskeytyskäsittelijän määrittely
vaihtaa tilaan READ_SENSOR kun painetaan -> aloitetaan luku


    programState = READ_SENSOR;
}
*/
/*
TASKIEN MÄÄRITTELY
*/
// sensorin luku
static void sensorTask(void *arg){
    (void)arg;
    
    float ax, ay, az, gx, gy, gz, t;
    
    while (1){
        if(programState == READ_SENSOR){
            //TODO VALMIIKSI
            if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t) == 0) {
                if (gx > 240 || gx < -240) {

                    snprintf(txbuf,BUFFER_SIZE, ".");


                    rgb_led_write(255, 0, 255);     //vihreä LED
                    vTaskDelay(pdMS_TO_TICKS(100));
                    stop_rgb_led();
                    vTaskDelay(pdMS_TO_TICKS(100));

                } else if (gy > 240 || gy < -240){

                    snprintf(txbuf,BUFFER_SIZE, "-");


                    rgb_led_write(255, 0, 255);     //vihreä LED
                    vTaskDelay(pdMS_TO_TICKS(100));
                    stop_rgb_led();
                    vTaskDelay(pdMS_TO_TICKS(100));

                }
                
            } else {
                printf("Failed to read imu data\n");
            }
            //käydään buf alkiot läpi. jos 2 peräkkäistä välilyöntiä->tilanmuutos
            for (int i = 1; i<BUFFER_SIZE; i++){
                if (txbuf[i] == ' ' && txbuf[i-1] == ' '//jos 2 peräkkäistä väl. ja (...) ->lähetys
                    && tud_cdc_n_connected(CDC_ITF_TX)){//jos tudi linjoilla->ykok
                    sprintf(txbuf,"\n");

                    tud_cdc_n_write(CDC_ITF_TX, txbuf, strlen(txbuf));
                    tud_cdc_n_write_flush(CDC_ITF_TX);  //lähetys                
                    memset(txbuf, 0, BUFFER_SIZE); //copilot

                    rgb_led_write(255, 0, 255);     //vihreä LED = lähetys ok
                    vTaskDelay(pdMS_TO_TICKS(100));
                    stop_rgb_led();
                    vTaskDelay(pdMS_TO_TICKS(100));

                    programState = WAITING;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}


// usb-viestintä
static void msgTask(void *arg){
    (void)arg;

    size_t index = 0;
    while (1) {
        if (programState == WAITING) {
            

            int c = getchar_timeout_us(0);
            if (c != PICO_ERROR_TIMEOUT){
                programState = NEW_MSG;
                if (c == '\r') continue; 
                if (c == '\n'){
                    rxbuf[index] = '\0'; 
                    //printf("__[RX]:\"%s\"__\n", rxbuf);
                    index = 0;
                    programState = UPDATE;  // vaihdetaan tila UPDATEksi
                    vTaskDelay(pdMS_TO_TICKS(100)); // Wait for new message
                }
                else if(index < BUFFER_SIZE - 1){
                    rxbuf[index++] = (char)c;
                }
                else {
                    rxbuf[BUFFER_SIZE - 1] = '\0';
                    //printf("__[RX]:\"%s\"__\n", rxbuf);
                    index = 0; 
                    rxbuf[index++] = (char)c;
                    programState = UPDATE;  // vaihdetaan tila UPDATEksi 
                }
            }
            else {
                vTaskDelay(pdMS_TO_TICKS(100)); // Wait for new message
            }
        }
    }
}

// näytön päivitys
static void printTask(void *arg){
    (void)arg;
    
    while (1){
        
        if (programState == UPDATE) {
            clear_display();

            rgb_led_write(255, 255, 0);     //sininen LED
            vTaskDelay(pdMS_TO_TICKS(200));
            stop_rgb_led();

            // luetaaan rxbufferista merkit
            // piirretään ne näytölle ja jätetään näkyviin --> vilkutetaan merkit ledillä --> sammutetaan näyttö
            int n = 0;
            //väliaikaiset bufferit
            char buf1[] = "         ";
            char buf2[] = "         ";
            char buf3[] = "         ";
            char buf4[] = "         ";

            
            int b1 = 0; 
            int b2 = 0; 
            int b3 = 0;

            for (int i = 0; i < BUFFER_SIZE; i++ ){//käydään läpi koko rxbufferi
                //jos bufferissa välilyönti, lisätään välilyöntilaskuriin
                if (rxbuf[i] == ' '){
                    n++;
                }
                //tallennetaan väliaikaisiin buffereihin n-arvon perusteella dataa
                if (n < 2 && n >= 0 && rxbuf[i] != 0){
                    buf1[i] = rxbuf[i];
                    b1++;
                }
                else if (n < 4 && n >= 2 && rxbuf[i] != 0){
                    buf2[i-b1] = rxbuf[i];
                    b2++;
                }
                else if (n < 6 && n >= 4 && rxbuf[i] != 0){
                    buf3[i-b2-b1] = rxbuf[i];
                    b3++;
                }
                else if (n < 8 && n >= 6 && rxbuf[i] != 0){
                    buf4[i-b3-b2-b1] = rxbuf[i];
                }
                
            }
            
            write_text_xy(8, 16, buf1);
            write_text_xy(8, 32, buf2);
            write_text_xy(8, 48, buf3);
            write_text_xy(8, 64, buf4);
            //vTaskDelay(pdMS_TO_TICKS(800));
            //clear_display();

            
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
// LED
static void ledtask(void *arg) {
    (void)arg;
    
    while (1) {
        if (programState == UPDATE) {
            for (int i = 0; i < BUFFER_SIZE; i++) {
                if (rxbuf[i] = '.') {
                    rgb_led_write(50, 50, 50);     // valkoinen LED
                    vTaskDelay(pdMS_TO_TICKS(500));
                    stop_rgb_led();
                    vTaskDelay(pdMS_TO_TICKS(200));
                }
                else if (rxbuf[i] = '-') {
                    rgb_led_write(50, 50, 50);     // valkoinen LED
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    stop_rgb_led();
                    vTaskDelay(pdMS_TO_TICKS(200));                   
                }
            }
            programState = WAITING;      
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// usb-pinon ylläpito
static void usbTask(void *arg){
    (void)arg;

    while (1) {
        tud_task();              // With FreeRTOS wait for events
                                 // Do not add vTaskDelay. 
    }
}


// napin tarkkailuun
static void switchTask(void *arg){
    (void)arg;

    while (1){
        if (programState == READ_SENSOR){
            sw2_pressed = gpio_get(SW2_PIN);
            if (sw2_pressed == true){
                sprintf(txbuf," ");
                //usb_serial_print(txbuf);
            }
            
        }
        vTaskDelay(pdMS_TO_TICKS(50));  //lyhyt aika, jotta nähdään paremmin
    }
}

// musiikin soitto
static void music(uint gpio, uint32_t eventMask){
    buzzer_play_tone(330, 500);
    buzzer_play_tone(277, 500);
    buzzer_play_tone(294, 500);
    buzzer_play_tone(330, 1500);
    buzzer_play_tone(440, 500);
    buzzer_play_tone(494, 1500);
    buzzer_play_tone(330, 250);
    buzzer_play_tone(554, 1500);
    buzzer_play_tone(440, 1000);
    buzzer_play_tone(370, 1500);
    buzzer_play_tone(494, 250);
    buzzer_play_tone(440, 1000);
    buzzer_play_tone(415, 1000);
    buzzer_play_tone(440, 1500);
    buzzer_turn_off();
}

int main() {

    stdio_init_all();
    // Uncomment this lines if you want to wait till the serial monitor is connected
    /*while (!stdio_usb_connected()){
        sleep_ms(10);
    }*/ 
    init_hat_sdk();
    sleep_ms(300); //Wait some time so initialization of USB and hat is done.


//OMAT ALUSTUKSET
    //näyttö
    init_display();

    //napit
    gpio_init(BUTTON1);
    gpio_init(BUTTON2);
    gpio_set_dir(BUTTON1, GPIO_IN);
    gpio_set_dir(BUTTON2, GPIO_IN);
    //gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_RISE, true, &btn_fxn);//välilyön
    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_RISE, true, &music);

    // IMU 
    if (init_ICM42670() == 0) {
        //_print("ICM-42670P initialized successfully!\n");
        if (ICM42670_start_with_default_values() != 0){
           //usb_serial_print("ICM-42670P could not initialize accelerometer or gyroscope");
        }
    } else {
        //usb_serial_print("Failed to initialize ICM-42670P.\n");
    }

    //rgb led
    init_rgb_led();

    //buzzer
    init_buzzer();


    if (init_ICM42670() == 0) {
        //printf("ICM-42670P initialized successfully!\n");
        if (ICM42670_start_with_default_values() != 0){
            //printf("ICM-42670P could not initialize accelerometer or gyroscope");
        }

    } else {
        printf("Failed to initialize ICM-42670P.\n");
    }

    TaskHandle_t sensorHandle = NULL;
    TaskHandle_t msgHandle = NULL; 
    TaskHandle_t usbHandle = NULL;
    TaskHandle_t printHandle = NULL;
    TaskHandle_t ledHandle = NULL;
    TaskHandle_t switchHandle = NULL;

    //sensorin luku
    BaseType_t result = xTaskCreate(sensorTask, "sensor", DEFAULT_STACK_SIZE, NULL, 2, &sensorHandle);
    if(result != pdPASS) {
        printf("sensor Task creation failed\n");
        return 0;
    }
    //usb viestintä
    result = xTaskCreate(msgTask, "msg", DEFAULT_STACK_SIZE, NULL, 2, &msgHandle);
    if(result != pdPASS) {
        printf("msg Task creation failed\n");
        return 0;
    }
    //näytön päivitys
    result = xTaskCreate(printTask, "print", DEFAULT_STACK_SIZE, NULL, 2, &printHandle);
    if(result != pdPASS) {
        printf("print Task creation failed\n");
        return 0;
    }
    //LED
    result = xTaskCreate(ledtask, "led", DEFAULT_STACK_SIZE, NULL, 2, &ledHandle);
    if(result != pdPASS) {
        printf("print Task creation failed\n");
        return 0;
    }
    //usb pinon ylläpito
    result = xTaskCreate(usbTask, "usb", DEFAULT_STACK_SIZE, NULL, 2, &usbHandle);
    if(result != pdPASS) {
        printf("usb Task creation failed\n");
        return 0;
    }
    //musiikin soitto
        //result = xTaskCreate(musicTask, "music", DEFAULT_STACK_SIZE, NULL, 2, &musicHandle);
        //if(result != pdPASS) {
            //printf("music Task creation failed\n");
            //return 0;
        //}
    
    //napin tarkkailuun
    result = xTaskCreate(switchTask, "switch", DEFAULT_STACK_SIZE, NULL, 2, &switchHandle);
    if(result != pdPASS) {
        printf("switch Task creation failed\n");
        return 0;
    }
    //tusb init
    tusb_init();
    //usb_serial_init();

    // Start the scheduler (never returns)
    vTaskStartScheduler();

    // Never reach this line.
    return 0;
}
