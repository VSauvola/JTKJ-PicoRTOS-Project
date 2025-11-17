
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
#include "usbSerialDebug/helper.h"
//#include "../../../.freertos/include/projdefs.h"


// Default stack size for the tasks. It can be reduced to 1024 if task is not using lot of memory.
#define DEFAULT_STACK_SIZE  2048 
#define BUFFER_SIZE         48
#define SMAL_BUFFER_SIZE    16
#define CDC_ITF_TX          1
#define INPUT_BUFFER_SIZE   256



//Add here necessary states
enum state { WAITING=1, READ_SENSOR, NEW_MSG, UPDATE};
static enum state programState = WAITING;//lisätty static alkuun

char txbuf[BUFFER_SIZE];     //tx bufferi = 10*4+5 = 45
char rxbuf[BUFFER_SIZE];        //tulo bufferi
static char line[INPUT_BUFFER_SIZE];
    
static size_t rx_index = 0;

float ax, ay, az, gx, gy, gz, t;
uint8_t mark_counter = 0;//lasketaan merkkejä
//static size_t val_counter = 0; //toimisko size_t?

//yehdäänpä näille hemmetin napeillekkin sitten taski:(
volatile bool button1_pressed = false;//mussiikki
volatile bool button2_pressed = false;//välilyönti

void music(void);
//txbuffiin lisäysfunktio
void add_to_txbuf(char c){
    if (programState != READ_SENSOR){
        return;
    }
    if (mark_counter < BUFFER_SIZE - 1){
        txbuf[mark_counter++] = c;
        txbuf[mark_counter] = '\0';
    }
    else {
        printf("txbuf ylivuoto!\nAloitetaan alusta:(");
        mark_counter = 0;
    }
}

//TASKIEN MÄÄRITTELY

// sensorin luku
static void sensorTask(void *arg){
    (void)arg;
    
    //float ax, ay, az, gx, gy, gz, t;//mainiin?
    
    while (1){
        if(programState == READ_SENSOR){
            //printf("readSensor tilassa sensortask!\n");
            //TODO VALMIIKSI
            if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t) == 0) {
                //printf("%.2f, %.2f\n", gx, gy);// ei koskaan päästä?
                if (gx > 210 || gx < -210) {
                    //ongelmana, että pelkkä sprintf kirjoittaa koko paskan yli
                    
                    //printf("merkkilaskuri: %i", mark_counter);
                    //printf(" . tunnistettu!\n");
                    //snprintf(txbuf,BUFFER_SIZE, ".");
                    add_to_txbuf('.');//lisää kivasti txbufferiin
                    //printf("txbuf=%s\n", txbuf);

                    

                    rgb_led_write(0, 255, 0);     //vihreä LED
                    vTaskDelay(pdMS_TO_TICKS(100));
                    //stop_rgb_led();
                    rgb_led_write(0,0,0);
                    vTaskDelay(pdMS_TO_TICKS(100));

                } else if (gy > 210 || gy < -210){
                    
                    //printf("merkkilaskuri: %i", mark_counter);
                    //printf(" - tunnistettu!\n");
                    //snprintf(txbuf,BUFFER_SIZE, "-");
                    add_to_txbuf('-');
                    //printf("txbuf=%s\n", txbuf);

                    rgb_led_write(0, 255, 0);     //vihreä LED
                    vTaskDelay(pdMS_TO_TICKS(100));
                    rgb_led_write(0,0,0);
                    vTaskDelay(pdMS_TO_TICKS(100));

                }
                
            } else {
                printf("Failed to read imu data\n");
            }
            //käydään buf alkiot läpi. jos 2 peräkkäistä välilyöntiä->tilanmuutos

            //välilyöntitaskiin
            
            /*for (int i = 1; i<BUFFER_SIZE; i++){
                printf("sensortask for-silmukka!\n");
                tud_cdc_n_write(CDC_ITF_TX, txbuf, strlen(txbuf));
                tud_cdc_n_write_flush(CDC_ITF_TX);
                if (txbuf[i] == ' ' && txbuf[i-1] == ' '//jos 2 peräkkäistä väl. ja (...) ->lähetys
                    && tud_cdc_n_connected(CDC_ITF_TX)){//jos tudi linjoilla->ykok
                    sprintf(txbuf,"\n");

                    tud_cdc_n_write(CDC_ITF_TX, txbuf, strlen(txbuf));
                    tud_cdc_n_write_flush(CDC_ITF_TX);  //lähetys                
                    memset(txbuf, 0, BUFFER_SIZE); //copilot

                    rgb_led_write(255, 0, 0);     //vihreä LED = lähetys ok
                    vTaskDelay(pdMS_TO_TICKS(100));
                    stop_rgb_led();
                    vTaskDelay(pdMS_TO_TICKS(100));

                    
                }
            programState = WAITING;//siirretty for-silmukan ulkopuolelle
            }*/
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}


// usb-viestintä  KORVATAAN CALLBACKILLÄ?!
/*static void msgTask(void *arg){
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
}*/

// näytön päivitys
static void printTask(void *arg){
    (void)arg;
    init_display();
    clear_display();
    while (1){
        
        if (programState == UPDATE) {
            //printf("printtask käynnissä!\n");
            // clear_display();

            //rgb_led_write(255, 0, 0);     //sininen LED
            //vTaskDelay(pdMS_TO_TICKS(200));
            //rgb_led_write(0, 0, 0); 
            //stop_rgb_led();

            // luetaaan rxbufferista merkit
            // piirretään ne näytölle ja jätetään näkyviin --> vilkutetaan merkit ledillä --> sammutetaan näyttö
            
            //väliaikaiset bufferit
            char buf1[SMAL_BUFFER_SIZE];
            char buf2[SMAL_BUFFER_SIZE];
            char buf3[SMAL_BUFFER_SIZE];
            memset(buf1, ' ', SMAL_BUFFER_SIZE);
            memset(buf2, ' ', SMAL_BUFFER_SIZE);
            memset(buf3, ' ', SMAL_BUFFER_SIZE);

            //char buf4[] = "         ";

            int n = 0;
            int b1 = 0; 
            int b2 = 0; 
            int b3 = 0;

            for (int i = 0; i < BUFFER_SIZE; i++ ){//käydään läpi koko rxbufferi
                //jos bufferissa välilyönti, lisätään välilyöntilaskuriin
                if (rxbuf[i] == ' '){
                    n++;
                }
                //tallennetaan väliaikaisiin buffereihin n-arvon perusteella dataa
                if (n < 3 && n >= 0 && rxbuf[i] != 0){
                    buf1[b1++] = rxbuf[i];
                }
                else if (n < 6 && n >= 3 && rxbuf[i] != 0){
                    buf2[b2++] = rxbuf[i];
                }
                else if (n < 9 && n >= 6 && rxbuf[i] != 0){
                    buf3[b3++] = rxbuf[i];
                    
                }
                else if (n >= 9 && rxbuf[i+1] != '\0'){
                    tud_cdc_n_write(CDC_ITF_TX, (uint8_t const *) "katkes!\n", 8);
                    tud_cdc_n_write_flush(CDC_ITF_TX);
                    break;
                }
                else {
                    break;
                }
                /*else if (n < 8 && n >= 6 && rxbuf[i] != 0){
                    buf4[i-b3-b2-b1] = rxbuf[i];
                }*/
                
            }
            buf1[b1] = 0;
            buf2[b2] = 0;
            buf3[b3] = 0;

            usb_serial_print("\nbuf1: ");
            usb_serial_print(buf1);
            usb_serial_print("\nbuf2: ");
            usb_serial_print(buf2);
            usb_serial_print("\nbuf3: ");
            usb_serial_print(buf3);
            usb_serial_flush();

            clear_display();
            write_text_xy(10, 16, buf1);
            write_text_xy(8, 32, buf2);
            write_text_xy(8, 48, buf3);
            //write_text_xy(8, 64, buf4);
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
            usb_serial_print("ledtask käynnissä!\n");
            for (int i = 0; i < BUFFER_SIZE; i++) {
                if (rxbuf[i] == '.') {
                    rgb_led_write(50, 50, 50);     // valkoinen LED
                    vTaskDelay(pdMS_TO_TICKS(250));
                    //stop_rgb_led();
                    rgb_led_write(0,0,0);
                    vTaskDelay(pdMS_TO_TICKS(250));
                }
                else if (rxbuf[i] == '-') {
                    rgb_led_write(50, 50, 50);     // valkoinen LED
                    vTaskDelay(pdMS_TO_TICKS(500));
                    //stop_rgb_led();
                    rgb_led_write(0,0,0);
                    vTaskDelay(pdMS_TO_TICKS(250));                   
                }
            }
            programState = WAITING; 
            clear_display();     
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


// napin tarkkailuun välilyönti
/*static void switchTask(void *arg){
    (void)arg;

    while (1){
        
        tilan pitäisi olla waiting ja siirtyä read sensor?->ei turhia välilyöntejä
        
        if (programState == WAITING){
            
            sw2_pressed = gpio_get(SW2_PIN);
            if (sw2_pressed == true){
                printf("välinappi painettu!\n");
                sprintf(txbuf," ");
                printf("TX: %s\n", txbuf);
                //usb_serial_print(txbuf);
                programState = READ_SENSOR;
                printf("switchtask lila->read_sensor\n");
            }
            
        }
        vTaskDelay(pdMS_TO_TICKS(50));  //lyhyt aika, jotta nähdään paremmin
    }
}*/

//uusi swich task!!!
static void switchTask(void *arg){
    (void)arg;
    
    while (1) {

        if (button2_pressed){
            button2_pressed = false;


            if (programState == WAITING){
                //noteeraus
                rgb_led_write(0,0,255);
                vTaskDelay(pdMS_TO_TICKS(100));
                rgb_led_write(0,0,0);
                programState = READ_SENSOR;
            }
            else if (programState == READ_SENSOR){
                //printf("välinappi painettu!\n");
                //noteeraus
                rgb_led_write(0,0,255);
                vTaskDelay(pdMS_TO_TICKS(100));
                rgb_led_write(0,0,0);  

                add_to_txbuf(' ');
                printf("txbuf=%s\n", txbuf);
                //sprintf(txbuf," ");
                //käydään txbuff läpi ja etsitään välilyönnit
                for (int i = 1; i < BUFFER_SIZE; i++){
                    if (txbuf[i] == ' ' && txbuf[i-1] == ' '){//jos 2 peräkkäistä väl. ja (...) ->lähetys
                    //&& tud_cdc_n_connected(CDC_ITF_TX)){//jos tudi linjoilla->ykok  

                        printf("lähetysehdot ok!");

                        while (!tud_mounted() || !tud_cdc_n_connected(1)){
                                vTaskDelay(pdMS_TO_TICKS(50));
                        }
                        usb_serial_flush();  
                        
                        if(tud_cdc_n_connected(CDC_ITF_TX)){
                            tud_cdc_n_write(CDC_ITF_TX, txbuf, strlen(txbuf));
                            tud_cdc_n_write_flush(CDC_ITF_TX);
                        }
                        

                        //sprintf(txbuf,"\n");
                        if (usb_serial_connected()){
                            usb_serial_print("pitäisi olla lähtenyt:\n");
                            usb_serial_print(txbuf);
                            usb_serial_flush();
                        }
                        memset(txbuf, 0, BUFFER_SIZE);
                        mark_counter = 0;
                        rgb_led_write(0, 255, 0);     //vihreä LED = lähetys ok
                        vTaskDelay(pdMS_TO_TICKS(100));
                        rgb_led_write(0, 0, 0);

                        programState = WAITING;
                        break; // pitäisi pysäyttää loop
                    }
                }

            } 
            
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }  
}
//keskeytys, joka lisää välilyönnin, jos ollaan read_sensor ja lähettää, jos 2 väliä

/*static void vali(void){
    if (programState == WAITING){
        programState = READ_SENSOR;
    }
    else if (programState == READ_SENSOR){
        printf("välinappi painettu!\n");
        
        add_to_txbuf(' ');
        printf("txbuf=%s\n", txbuf);
        //sprintf(txbuf," ");
        //käydään txbuff läpi ja etsitään välilyönnit
        for (int i = 1; i < BUFFER_SIZE; i++){
            if (txbuf[i] == ' ' && txbuf[i-1] == ' '){//jos 2 peräkkäistä väl. ja (...) ->lähetys
            //&& tud_cdc_n_connected(CDC_ITF_TX)){//jos tudi linjoilla->ykok  

                printf("lähetysehdot ok!");

                //sprintf(txbuf,"\n");//rivinvaihto loppuun
                tud_cdc_n_write(CDC_ITF_TX, txbuf, strlen(txbuf));
                tud_cdc_n_write_flush(CDC_ITF_TX);  //lähetys 
                printf("%s",txbuf);
                memset(txbuf, 0, BUFFER_SIZE); //copilot

                rgb_led_write(255, 0, 0);     //vihreä LED = lähetys ok
                //vTaskDelay(pdMS_TO_TICKS(100));
                stop_rgb_led();
                
                programState = WAITING;
            }
        }

    }

}*/

// musiikin soitto !NYT OK!
void music(){
    buzzer_play_tone(330, 500);
    buzzer_play_tone(277, 500);
    buzzer_play_tone(294, 500);
    buzzer_play_tone(330, 1250);
    buzzer_play_tone(440, 500);
    buzzer_play_tone(494, 1250);
    buzzer_play_tone(330, 250);
    buzzer_play_tone(554, 1500);
    buzzer_play_tone(440, 1000);
    buzzer_play_tone(370, 1250);
    buzzer_play_tone(494, 250);
    buzzer_play_tone(440, 1000);
    buzzer_play_tone(415, 1000);
    buzzer_play_tone(440, 1250);
    buzzer_turn_off();
}

//perkeleen helvetin musiikkitaski taas :)
static void musicTask(void *arg){
    (void)arg;

    while(1){
        if (button1_pressed){
            button1_pressed = false;
            music();

        }
    vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void button_callback(uint gpio, uint32_t eventMask){
    //tarkastetaan callbackistä, kumpi nappi painettiin
    if (gpio == BUTTON1){//musiikki
        button1_pressed = true;//vaihetaan trueksi nappipaskassa takasin
    }
    else if (gpio == BUTTON2){
        button2_pressed = true;
    }
}

int main() {

    stdio_init_all();
    // Uncomment this lines if you want to wait till the serial monitor is connected
    /*sleep_ms(500);
    while (!stdio_usb_connected()){
        sleep_ms(10);
    }*/
    //printf("testi\n");
    init_hat_sdk();
    //sleep_ms(300); //Wait some time so initialization of USB and hat is done.

    
//OMAT ALUSTUKSET
    //bufferit
    memset(txbuf, 0, BUFFER_SIZE);
    memset(rxbuf, 0, BUFFER_SIZE);
    //näyttö
    //init_display();
    //clear_display();
    //napit
    gpio_init(BUTTON1);
    gpio_init(BUTTON2);
    gpio_set_dir(BUTTON1, GPIO_IN);
    gpio_set_dir(BUTTON2, GPIO_IN);
    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_RISE, true, &button_callback);//musiikki
    gpio_set_irq_enabled(BUTTON2, GPIO_IRQ_EDGE_RISE, true);//väli

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
    rgb_led_write(0,0,0);
    //stop_rgb_led();
    //buzzer
    init_buzzer();



    TaskHandle_t sensorHandle = NULL;
    //TaskHandle_t msgHandle = NULL; 
    TaskHandle_t usbHandle = NULL;
    TaskHandle_t printHandle = NULL;
    TaskHandle_t ledHandle = NULL;
    TaskHandle_t switchHandle = NULL;
    TaskHandle_t musicHandle = NULL;

    //sensorin luku
    BaseType_t result = xTaskCreate(sensorTask, "sensor", DEFAULT_STACK_SIZE, NULL, 2, &sensorHandle);
    if(result != pdPASS) {
        printf("sensor Task creation failed\n");
        return 0;
    }
    //usb viestintä
    /*result = xTaskCreate(msgTask, "msg", DEFAULT_STACK_SIZE, NULL, 2, &msgHandle);
    if(result != pdPASS) {
        printf("msg Task creation failed\n");
        return 0;
    }*/
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
    
    //napin tarkkailuun taski
    result = xTaskCreate(switchTask, "switch", DEFAULT_STACK_SIZE, NULL, 2, &switchHandle);
    if(result != pdPASS) {
        printf("switch Task creation failed\n");
        return 0;
    }

    #if (configNUMBER_OF_CORES > 1)
        vTaskCoreAffinitySet(usbHandle, 1u << 0);
    #endif
    //tusb init
    tusb_init();
    usb_serial_init();

    // Start the scheduler (never returns)
    vTaskStartScheduler();

    // Never reach this line.
    return 0;
}

//callbackki tuleville hommille
/*void tud_cdc_rx_cb(uint8_t itf){   
    // allocate buffer for the data in the stack
    uint8_t buf[CFG_TUD_CDC_RX_BUFSIZE+1];
    // read the available data 
    uint32_t count = tud_cdc_n_read(itf, buf, sizeof(buf));



    // check if the data was received on the second cdc interface
    if (itf == 1) {
        // process the received data
        
        buf[count] = 0;// null-terminate the string
        
        strncpy(rxbuf, (char*)buf, BUFFER_SIZE - 1);//kopioidaan rxbuffiin
        rxbuf[BUFFER_SIZE - 1] = 0;
        
        programState = UPDATE;

        usb_serial_print("\nReceived on CDC 1:");
        usb_serial_print(rxbuf);

        // and echo back OK on CDC 1
        tud_cdc_n_write(itf, (uint8_t const *) "OK\n", 3);
        tud_cdc_n_write_flush(itf);
        }
}*/

void tud_cdc_rx_cb(uint8_t itf){
    // allocate buffer for the data in the stack
    uint8_t buf[CFG_TUD_CDC_RX_BUFSIZE+1];
    memset(buf, 0, sizeof(buf));
    char line[INPUT_BUFFER_SIZE];
    //memset(line, 0, sizeof(line));
    // size_t index = 0;
    // read the available data 
    uint32_t count = tud_cdc_n_read(itf, buf, sizeof(buf));

    // check if the data was received on the second cdc interface
    if (itf == 1) {
        // process the received data
        
        buf[count] = 0;// null-terminate the string
        
        for (uint32_t i = 0; i < count; i++){

            char c = (char)buf[i];

            if (c == '\n'){//päästään viestin loppuun
                line[rx_index] = '\0';
                rx_index = 0;//nollataan 
                memset(rxbuf, 0, BUFFER_SIZE);
                strncpy(rxbuf, line, BUFFER_SIZE - 1);//kopioidaan rxbuffiin
                rxbuf[BUFFER_SIZE - 1] = 0;
                
                usb_serial_print("\nReceived on CDC 1:");
                usb_serial_print(rxbuf);

                // and echo back OK on CDC 1
                tud_cdc_n_write(itf, (uint8_t const *) "OK\n", 3);
                tud_cdc_n_write_flush(itf);

                programState = UPDATE;

                return;
            }
            /*else if (c == ' '){
                val_counter++;
            }*/
            else if (rx_index < BUFFER_SIZE - 1) {//&& val_counter <= 9)
                line[rx_index++] = c;
            }
            else{
                //jos täybbä, jätetään koko homma ja rinttiin
                line[BUFFER_SIZE - 1] = '\0';
                rx_index = 0;
                //val_counter = 0;
                memset(rxbuf, 0, BUFFER_SIZE);
                strncpy(rxbuf, line, BUFFER_SIZE - 1);
                rxbuf[BUFFER_SIZE - 1] = 0;

                usb_serial_print("\nReceived on CDC 1:");
                usb_serial_print(rxbuf);
                
                // and echo back OK on CDC 1
                tud_cdc_n_write(itf, (uint8_t const *) "katkes!\n", 8);
                tud_cdc_n_write_flush(itf);
                
                programState = UPDATE;
                return;
            }
        } 
    }   
}