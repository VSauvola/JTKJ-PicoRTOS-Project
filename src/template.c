
#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "tkjhat/sdk.h"

// Default stack size for the tasks. It can be reduced to 1024 if task is not using lot of memory.
#define DEFAULT_STACK_SIZE 2048 
#define BUFFER_SIZE 100


//Add here necessary states
enum state { WAITING=1, READ_SENSOR, NEW_MSG, UPDATE };
enum state programState = WAITING;

uint8_t txbuf[BUFFER_SIZE];//tx bufferi
char rxbuf[BUFFER_SIZE];//tulo bufferi
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

    //senssorin luku
static void sensorTask(void *arg){
    (void)arg;
    
    float ax, ay, az, gx, gy, gz, t;

    // Start collection data here. Infinite loop. 
    //TODO nappi?
    
    while (1){
        if(programState == READ_SENSOR){
            //TODO VALMIIKSI
            if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t) == 0) {
                if (gx == 250 | gx == -250) {
                    sprintf(txbuf,".");
                    usb_serial_print(txbuf);
                } else if (gy == 250 | gy == -250){
                    sprintf(txbuf,"-");
                    usb_serial_print(txbuf);
                }
                
            } else {
                printf("Failed to read imu data\n");
            }
            //käydään buf alkiot läpi. jos 2 peräkkäistä välilyöntiä->tilanmuutos
            for (int i = 0; i<BUFFER_SIZE; i++){
                if (txbuf[i] == " " && txbuf[i-1] == " " && i > 0){
                    sprintf(txbuf,"\n");
                    usb_serial_print(txbuf);                    
                    txbuf[BUFFER_SIZE] = NULL;
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
    while(1){
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT){// I have received a character
            if (c == '\r') continue; // ignore CR, wait for LF if (ch == '\n') { line[len] = '\0';
            if (c == '\n'){
                // terminate and process the collected line
                rxbuf[index] = '\0'; 
                //printf("__[RX]:\"%s\"__\n", rxbuf); //Print as debug in the output
                // vaihdetaan tila UPDATEksi 
                index = 0;
                vTaskDelay(pdMS_TO_TICKS(100)); // Wait for new message
            }
            else if(index < BUFFER_SIZE - 1){
                rxbuf[index++] = (char)c;
            }
            else { //Overflow: print and restart the buffer with the new character. 
                rxbuf[BUFFER_SIZE - 1] = '\0';
                printf("__[RX]:\"%s\"__\n", rxbuf);
                index = 0; 
                rxbuf[index++] = (char)c; 
            }
        }
        else {
            vTaskDelay(pdMS_TO_TICKS(100)); // Wait for new message
        }
    }
}

//näytön päivitys
static void printTask(void *arg){
    (void)arg;
    while (1){
        // luetaaan rxbufferista merkit, piirretään ne näytölle ja jätetään näkyviin --> vilkutetaan merkit ledillä --> sammutetaan näyttö
        tight_loop_contents(); // Modify with application code here.
        vTaskDelay(pdMS_TO_TICKS(2000));
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
    (void)arg;

    while (1){
        //buzzer_play_tone(freg, kesto); x-kertaa
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
                sprintf(buf," ");
                usb_serial_print(buf);
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
//buzzer
    init_buzzer();
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
    TaskHandle_t sensorHandle, msgHandle, usbHandle, printHandle, musicHandle, switchHandle = NULL;
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

