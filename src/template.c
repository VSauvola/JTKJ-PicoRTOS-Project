
#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tkjhat/sdk.h"
#include <tusb.h>

// Default stack size for the tasks. It can be reduced to 1024 if task is not using lot of memory.
#define DEFAULT_STACK_SIZE 2048 
#define BUFFER_SIZE 100
#define CDC_ITF_TX      1

//Add here necessary states
enum state { WAITING=1, READ_SENSOR, NEW_MSG, UPDATE, MUSIC};
enum state programState = WAITING;

char txbuf[BUFFER_SIZE];     //tx bufferi = 10*4+5 = 45
char rxbuf[BUFFER_SIZE];        //tulo bufferi
volatile bool sw2_pressed = false;

static void btn_fxn(uint gpio, uint32_t eventMask) {
/*
Keskeytyskäsittelijän määrittely
vaihtaa tilaan READ_SENSOR kun painetaan -> aloitetaan luku
*/

    programState = READ_SENSOR;
}

/*
TASKIEN MÄÄRITTELY TODO
*/

//sensorin luku
static void sensorTask(void *arg){
    (void)arg;
    
    float ax, ay, az, gx, gy, gz, t;
    
    while (1){
        if(programState == READ_SENSOR){
            //TODO VALMIIKSI
            if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t) == 0) {
                if (gx == 250 | gx == -250) {

                    snprintf(txbuf,BUFFER_SIZE, ".");


                    rgb_led_write(255, 0, 255);     //vihreä LED
                    vTaskDelay(pdMS_TO_TICKS(100));
                    stop_rgb_led();
                    vTaskDelay(pdMS_TO_TICKS(100));

                } else if (gy == 250 | gy == -250){

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
            for (int i = 0; i<BUFFER_SIZE; i++){
                if (txbuf[i] == " " && txbuf[i-1] == " " && i > 0//jos 2 peräkkäistä väl. ja (...) ->lähetys
                    && tud_cdc_n_connected(CDC_ITF_TX)){//jos tudi linjoilla->ykok
                    sprintf(txbuf,"\n");

                    tud_cdc_n_write(CDC_ITF_TX, txbuf, strlen(txbuf));
                    tud_cdc_n_write_flush(CDC_ITF_TX);  //lähetys                
                    txbuf[BUFFER_SIZE] = NULL;

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


//usb viestintä
static void msgTask(void *arg){
    (void)arg;

    size_t index = 0;
    while (1) {
        if (programState == WAITING) {
            programState = NEW_MSG;

            int c = getchar_timeout_us(0);
            if (c != PICO_ERROR_TIMEOUT){
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

//näytön päivitys
static void printTask(void *arg){
    (void)arg;
    init_display();
    init_rgb_led();
    
    while (1){

        if (programState == UPDATE) {
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

            
            int b1, b2, b3 = 0;

            for (int i = 0; i < BUFFER_SIZE; i++ ){//käydään läpi koko rxbufferi
                //jos bufferissa välilyönti, lisätään välilyöntilaskuriin
                if (rxbuf[i] == " "){
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
            vTaskDelay(pdMS_TO_TICKS(800));



            programState = WAITING;
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
// LED
static void ledtask(void *arg) {
    (void)arg;

    init_led();     // valkoinen LED
    
    while (1) {
        if (programState == UPDATE) {
            for (int i = 0; i < BUFFER_SIZE; i++) {
                if (rxbuf[i] = "."){
                    toggle_led();
                    vTaskDelay(pdMS_TO_TICKS(100));
                    toggle_led();
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                else if (rxbuf[i] = "-"){
                    toggle_led();
                    vTaskDelay(pdMS_TO_TICKS(200));
                    toggle_led();
                    vTaskDelay(pdMS_TO_TICKS(100));                   
                }
            }
        vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }
}

//usb pinon ylläpito
static void usbTask(void *arg){
    (void)arg;

    while (1) {
        tud_task();              // With FreeRTOS wait for events
                                 // Do not add vTaskDelay. 
    }
}

//musiikin soitto
static void musicTask(void *arg){
    // miten ja missä musiikki aloitetaan?
    (void)arg;
    init_buzzer();

    while (1){
        if (programState = MUSIC) {
            buzzer_play_tone(330, 500);
            buzzer_play_tone(277, 500);
            buzzer_play_tone(294, 500);
            buzzer_play_tone(330, 1500);
            buzzer_play_tone(440, 500);
            buzzer_play_tone(494, 1500);
            buzzer_play_tone(330, 250);
            buzzer_play_tone(554, 2000);
            buzzer_play_tone(440, 1000);
            buzzer_play_tone(370, 1500);
            buzzer_play_tone(494, 250);
            buzzer_play_tone(440, 1000);
            buzzer_play_tone(415, 1000);
            buzzer_play_tone(440, 2000);
            buzzer_turn_off();

            programState = WAITING;
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

//napin tarkkailuun
static void switchTask(void *arg){
    (void)arg;

    while (1){
        if (programState == READ_SENSOR){
            sw2_pressed = gpio_get(SW2_PIN);
            if (sw2_pressed == true){
                sprintf(txbuf," ");
                usb_serial_print(txbuf);
            }
            
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
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

//keskeytyskäsittelijä nappi 1

    gpio_init(BUTTON1);
    gpio_set_dir(BUTTON1, GPIO_IN);
    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_RISE, true, btn_fxn);
    // IMU 
    if (init_ICM42670() == 0) {
        //printf("ICM-42670P initialized successfully!\n");
        if (ICM42670_start_with_default_values() != 0){
            //printf("ICM-42670P could not initialize accelerometer or gyroscope");
        }

    } else {
        printf("Failed to initialize ICM-42670P.\n");
    }
    TaskHandle_t sensorHandle, msgHandle, usbHandle, printHandle, ledHandle, musicHandle, switchHandle = NULL;
    // Create the tasks with xTaskCreate

    //senssorin luku
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
    result = xTaskCreate(musicTask, "music", DEFAULT_STACK_SIZE, NULL, 2, &musicHandle);
    if(result != pdPASS) {
        printf("music Task creation failed\n");
        return 0;
    }
//napin tarkkailuun
    result = xTaskCreate(switchTask, "switch", DEFAULT_STACK_SIZE, NULL, 2, &switchHandle);

    if(result != pdPASS) {
        printf("switch Task creation failed\n");
        return 0;
    }

    // Start the scheduler (never returns)
    vTaskStartScheduler();

    // Never reach this line.
    return 0;
}
