/*

AUTHORS: Valtteri Sauvola ja Miia Korhonen


LEDIT:
-pun: '-'
-valk: '.'
-vihr: lähetys ok
-sin: välilyöntinappi painettu

LYHENTEET:
-KELA! = koeta esittää lyhyemmin asiasi!
-YKOK! = yhteyskokeilu ok!
-VALA! = vastaanottamisen aikana lähetys aloitettu!
-27EMT! = 27 ensimmäistä merkkiä tulostettiin!

TOIMINTA:
vas. nappi aloittaa merkkien lukemisen
    -1. painallus ei lyö välilyöntiä
    -1. painalluksen jälkeen vas. nappi kirjoittaa välilyönnin
    -2 peräkkäistä vas. napin painallusta lähettää tulkitut merkit
oik. nappi aloittaa musiikin soiton

'.' saadaan pyöräyttämällä pituussuunnassa
'-' saadaan pyöräyttämällä leveyssuunnassa

*/

#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include "tkjhat/sdk.h"
#include <tusb.h>
#include "usbSerialDebug/helper.h"

#define DEFAULT_STACK_SIZE  2048 
#define BUFFER_SIZE         93     //saadaan 9+9 5 merkin mittaista kirjainta
#define SMALL_BUFFER_SIZE   16
#define CDC_ITF_TX          1
#define INPUT_BUFFER_SIZE   256

//tilat
enum state {WAITING=1, READ_SENSOR, UPDATE};
static enum state programState = WAITING;

char txbuf[BUFFER_SIZE];        //lähetysbufferi
char rxbuf[BUFFER_SIZE];        //vastaanottobufferi

static char line[BUFFER_SIZE];



float ax, ay, az, gx, gy, gz, t;
uint8_t mark_counter = 0;   //lasketaan merkkejä add_to_txbuf
static size_t rx_index = 0; //vastaanoton käyttöön

volatile bool button1_pressed = false;  //mussiikki
volatile bool button2_pressed = false;  //välilyönti
volatile bool led_task_vaihees = false;
volatile bool buffer_overflow = false;

void music(void);
void check_and_put_char(void);

void check_and_put_char(){
    int laskuri = 0;
    memset(rxbuf, 0, BUFFER_SIZE);
    //vähän raskasta toimintaa, mutta menee se näinkin...
    for (uint8_t i = 0; i < BUFFER_SIZE - 1; i++){
        if (line[i] == ' ' || line[i] == '.' || line[i] == '-' || line[i] == '\0'){ //tarkastetaan, että laillinen merkki->ei roskaa
            rxbuf[laskuri++] = line[i];
        }
    }
    memset(line, 0, BUFFER_SIZE);
    laskuri = 0;
}


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
        mark_counter = 0;
    }
}

//TASKIEN MÄÄRITTELY

// sensorin luku
static void sensorTask(void *arg){
    (void)arg;
    
    while (1){
        if(programState == READ_SENSOR){

            if (ICM42670_read_sensor_data(&ax, &ay, &az, &gx, &gy, &gz, &t) == 0) {

                if (gx > 225 || gx < -225) {    //piste

                    add_to_txbuf('.');  //lisää kivasti txbufferiin
                    rgb_led_write(50, 50, 50);     //valk LED
                    vTaskDelay(pdMS_TO_TICKS(100));
                    rgb_led_write(0,0,0);
                    vTaskDelay(pdMS_TO_TICKS(100));

                } else if (gy > 225 || gy < -225){  //viiva

                    add_to_txbuf('-');
                    rgb_led_write(255, 0, 0);     //pun LED
                    vTaskDelay(pdMS_TO_TICKS(100));
                    rgb_led_write(0,0,0);
                    vTaskDelay(pdMS_TO_TICKS(100));

                }
                
            } else {
                usb_serial_print("Failed to read imu data\n");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}



// näytön päivitys
static void printTask(void *arg){
    (void)arg;
    init_display();
    clear_display();

    while (1){
        
        if (programState == UPDATE) {

            // luetaaan rxbufferista merkit
            // piirretään ne näytölle ja jätetään näkyviin --> vilkutetaan merkit ledillä --> sammutetaan näyttö
            
            //väliaikaiset bufferit
            char buf1[SMALL_BUFFER_SIZE];
            char buf2[SMALL_BUFFER_SIZE];
            char buf3[SMALL_BUFFER_SIZE];
            char buf4[SMALL_BUFFER_SIZE];
            char buf5[SMALL_BUFFER_SIZE];
            char buf6[SMALL_BUFFER_SIZE];
            char ybuf1[SMALL_BUFFER_SIZE];  //elihhäs ylivuotavat pikkumerkit
            char ybuf2[SMALL_BUFFER_SIZE];
            char ybuf3[SMALL_BUFFER_SIZE];
            //laitetaan aina kaikkiin välilyönnit, niin näytöllä näyttää hyvältä :)
            memset(buf1, ' ', SMALL_BUFFER_SIZE);
            memset(buf2, ' ', SMALL_BUFFER_SIZE);
            memset(buf3, ' ', SMALL_BUFFER_SIZE);
            memset(buf4, ' ', SMALL_BUFFER_SIZE);
            memset(buf5, ' ', SMALL_BUFFER_SIZE);
            memset(buf6, ' ', SMALL_BUFFER_SIZE);
            //ylivuodoille
            memset(ybuf1, ' ', SMALL_BUFFER_SIZE);
            memset(ybuf2, ' ', SMALL_BUFFER_SIZE);
            memset(ybuf3, ' ', SMALL_BUFFER_SIZE);


            int n = 0;
            int b1 = 0; 
            int b2 = 0; 
            int b3 = 0;
            // toinen näytös
            int b4 = 0; 
            int b5 = 0; 
            int b6 = 0;
            //ylivuodoille
            int y1 = 0;
            int y2 = 0;
            int y3 = 0;

            for (int i = 0; i < BUFFER_SIZE; i++ ){ //käydään läpi koko rxbufferi
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

                else if (n < 12 && n >= 9 && rxbuf[i] != 0){
                    buf4[b4++] = rxbuf[i];
                }
                else if (n < 15 && n >= 12 && rxbuf[i] != 0){
                    buf5[b5++] = rxbuf[i];
                }                
                else if (n < 18 && n >= 15 && rxbuf[i] != 0){
                    buf6[b6++] = rxbuf[i];
                }
                //ylivuoto, ei kiva
                else if (n < 21 && n >= 18 && rxbuf[i] != 0){
                    ybuf1[y1++] = rxbuf[i];
                }
                else if (n < 24 && n >= 21 && rxbuf[i] != 0){
                    ybuf2[y2++] = rxbuf[i];
                }
                else if (n < 27 && n >= 24 && rxbuf[i] != 0){
                    ybuf3[y3++] = rxbuf[i];
                }                
                else {
                    break;
                }

                
            }
            buf1[b1] = 0;
            buf2[b2] = 0;
            buf3[b3] = 0;
            buf4[b4] = 0;
            buf5[b5] = 0;
            buf6[b6] = 0;
            //yli
            ybuf1[y1] = 0;
            ybuf2[y2] = 0;
            ybuf3[y3] = 0;

            clear_display();
            //1. näytös
            write_text_xy(12, 16, buf1);
            write_text_xy(8, 32, buf2);
            write_text_xy(8, 48, buf3);


            if (buf4[0] != 0){  //katsellaan, josko tarvitaan toista näytöllistä
                vTaskDelay(pdMS_TO_TICKS(5000));
                clear_display();
                write_text_xy(12, 16, buf4);
                write_text_xy(8, 32, buf5);
                write_text_xy(8, 48, buf6);                
            }
            if (ybuf1[0] != 0){ //katsellaan, josko tarvitaan kolmatta näytöllistä ylivuodoille
                vTaskDelay(pdMS_TO_TICKS(5000));
                clear_display();
                write_text_xy(12, 16, ybuf1);
                write_text_xy(8, 32, ybuf2);
                write_text_xy(8, 48, ybuf3);
                // jos on asiat näin huonosti, niin laitetaan lähettäjälle viestiä, että vain viestin alkupää näytettiin
                tud_cdc_n_write(CDC_ITF_TX, (uint8_t const *) "27EMT!\n", 7);   //27 ensimmäistä merkkiä tulostettiin! = 27EMT!
                tud_cdc_n_write_flush(CDC_ITF_TX);
                               
            }
            while (led_task_vaihees){
                vTaskDelay(pdMS_TO_TICKS(50));
            }
            clear_display();
            memset(rxbuf, 0, BUFFER_SIZE);
            buffer_overflow = false;
            programState = WAITING;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// LED
static void ledtask(void *arg) {
    (void)arg;
    
    while (1) {
        if (programState == UPDATE) {
            led_task_vaihees = true;

            for (int i = 0; i < BUFFER_SIZE; i++) {
                if (rxbuf[i] == '.') {
                    rgb_led_write(50, 50, 50);     // valkoinen LED
                    vTaskDelay(pdMS_TO_TICKS(200));
                    rgb_led_write(0,0,0);
                    vTaskDelay(pdMS_TO_TICKS(250));
                }
                else if (rxbuf[i] == '-') {
                    rgb_led_write(255, 0, 0);     // pun LED
                    vTaskDelay(pdMS_TO_TICKS(500));
                    rgb_led_write(0,0,0);
                    vTaskDelay(pdMS_TO_TICKS(250));                   
                }
                else if (rxbuf[i] == ' '){
                    vTaskDelay(pdMS_TO_TICKS(250)); 
                }
            }
            led_task_vaihees = false;
            buffer_overflow = false;
            clear_display(); 
            memset(rxbuf, 0, BUFFER_SIZE);
            programState = WAITING; 
                
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// usb-pinon ylläpito
static void usbTask(void *arg){
    (void)arg;

    while (1) {
        tud_task();
    }
}


// uusi switch task!!!
static void switchTask(void *arg){
    (void)arg;
    
    while (1) {

        if (button2_pressed){
            button2_pressed = false;
            //jos halutaan keskeyttää saapuvan viestin katselu
            if (programState == UPDATE){
                programState = WAITING;
            //vastaanottamisen aikana lähetys aloitettu == VALA
                tud_cdc_n_write(CDC_ITF_TX, (uint8_t const *) "VALA\n", 4); //keskeytetty vastaannottajan toimesta
                tud_cdc_n_write_flush(CDC_ITF_TX);
                
            }

            else if (programState == WAITING){
                //noteeraus
                rgb_led_write(0,0,255);
                vTaskDelay(pdMS_TO_TICKS(100));
                rgb_led_write(0,0,0);
                programState = READ_SENSOR;
            }
            else if (programState == READ_SENSOR){
                
                //noteeraus
                rgb_led_write(0,0,255);
                vTaskDelay(pdMS_TO_TICKS(100));
                rgb_led_write(0,0,0);  

                add_to_txbuf(' ');
                //käydään txbuff läpi ja etsitään välilyönnit
                for (int i = 1; i < BUFFER_SIZE; i++){
                    if (txbuf[i] == ' ' && txbuf[i-1] == ' '){  //jos 2 peräkkäistä väl. ja (...) ->lähetys

                        while (!tud_mounted() || !tud_cdc_n_connected(1)){  //jos tudi linjoilla->ykok 
                                vTaskDelay(pdMS_TO_TICKS(50));
                        }
                        usb_serial_flush();  
                        
                        if(tud_cdc_n_connected(CDC_ITF_TX)){
                            add_to_txbuf('\n');
                            tud_cdc_n_write(CDC_ITF_TX, txbuf, strlen(txbuf));
                            tud_cdc_n_write_flush(CDC_ITF_TX);
                        }
                        

                        /*if (usb_serial_connected()){
                            usb_serial_print("pitäisi olla lähtenyt:\n");
                            usb_serial_print(txbuf);
                            usb_serial_flush();
                        }*/
                        memset(txbuf, 0, BUFFER_SIZE);
                        mark_counter = 0;
                        rgb_led_write(0, 255, 0);     //vihreä LED = lähetys ok
                        vTaskDelay(pdMS_TO_TICKS(100));
                        rgb_led_write(0, 0, 0);

                        programState = WAITING;
                        break; //pitäisi pysäyttää loop
                    }
                }

            } 
            
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }  
}


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

// musiikkitaski
static void musicTask(void *arg){
    (void)arg;

    while(1){
        if (button1_pressed){
            button1_pressed = false;    //muutetaan nappi
            music();    //soitto
        }
    vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void button_callback(uint gpio, uint32_t eventMask){
    //tarkastetaan callbackistä, kumpi nappi painettiin
    if (gpio == BUTTON1){   //musiikki
        button1_pressed = true; //vaihetaan trueksi nappipaskassa takasin
    }
    else if (gpio == BUTTON2){  //välilyönti
        button2_pressed = true;
    }
}

int main() {

    stdio_init_all();
    init_hat_sdk();
    sleep_ms(500);

    
    // OMAT ALUSTUKSET
    //bufferit
    memset(txbuf, 0, BUFFER_SIZE);
    memset(rxbuf, 0, BUFFER_SIZE);
    memset(line, 0, sizeof(line));

    //napit
    gpio_init(BUTTON1);
    gpio_init(BUTTON2);
    gpio_set_dir(BUTTON1, GPIO_IN);
    gpio_set_dir(BUTTON2, GPIO_IN);
    gpio_set_irq_enabled_with_callback(BUTTON1, GPIO_IRQ_EDGE_RISE, true, &button_callback);    //musiikki
    gpio_set_irq_enabled(BUTTON2, GPIO_IRQ_EDGE_RISE, true);    //välilyönti

    //IMU
    if (init_ICM42670() == 0) {
        if (ICM42670_start_with_default_values() != 0){
        }
    } else {
        usb_serial_print("Failed to initialize ICM-42670P.\n");
    }

    //rgb_led
    init_rgb_led();
    rgb_led_write(0,0,0);

    //buzzer
    init_buzzer();

    TaskHandle_t sensorHandle = NULL;
    TaskHandle_t usbHandle = NULL;
    TaskHandle_t printHandle = NULL;
    TaskHandle_t ledHandle = NULL;
    TaskHandle_t switchHandle = NULL;
    TaskHandle_t musicHandle = NULL;

    //sensorin luku
    BaseType_t result = xTaskCreate(sensorTask, "sensor", DEFAULT_STACK_SIZE, NULL, 2, &sensorHandle);
    if(result != pdPASS) {
        usb_serial_print("sensor Task creation failed\n");
        return 0;
    }

    //näytön päivitys
    result = xTaskCreate(printTask, "print", DEFAULT_STACK_SIZE, NULL, 2, &printHandle);
    if(result != pdPASS) {
        usb_serial_print("print Task creation failed\n");
        return 0;
    }

    //LED
    result = xTaskCreate(ledtask, "led", DEFAULT_STACK_SIZE, NULL, 2, &ledHandle);
    if(result != pdPASS) {
        usb_serial_print("print Task creation failed\n");
        return 0;
    }

    //usb-pinon ylläpito
    result = xTaskCreate(usbTask, "usb", DEFAULT_STACK_SIZE, NULL, 2, &usbHandle);
    if(result != pdPASS) {
        usb_serial_print("usb Task creation failed\n");
        return 0;
    }

    //musiikin soitto
        result = xTaskCreate(musicTask, "music", DEFAULT_STACK_SIZE, NULL, 2, &musicHandle);
        if(result != pdPASS) {
            usb_serial_print("music Task creation failed\n");
            return 0;
        }
    
    //napin tarkkailuun taski
    result = xTaskCreate(switchTask, "switch", DEFAULT_STACK_SIZE, NULL, 2, &switchHandle);
    if(result != pdPASS) {
        usb_serial_print("switch Task creation failed\n");
        return 0;
    }

    usb_serial_flush();

    #if (configNUMBER_OF_CORES > 1)
        vTaskCoreAffinitySet(usbHandle, 1u << 0);
    #endif

    tusb_init();
    usb_serial_init();
    vTaskStartScheduler();
    return 0;
}


void tud_cdc_rx_cb(uint8_t itf){
//ei haluta ottaa mitään vastaan, kun vanhat kesken
    if (!led_task_vaihees && !buffer_overflow){ //eli katsotaan, että leditaski ei ole käynnissä
        
        uint8_t buf[CFG_TUD_CDC_RX_BUFSIZE+1];
        memset(buf, 0, sizeof(buf));        

        //luethan dattaa
        uint32_t count = tud_cdc_n_read(itf, buf, sizeof(buf));

        //kahtellaan josko sitä dattaa ois saatavilla
        if (itf == 1) {
            //ruethan käsithelemään dattaa
            
            buf[count] = 0; //lyödään nollaterminator
            

            for (uint32_t i = 0; i < count; i++){

                char c = (char)buf[i];
                
                if (c == '\n'){ //päästään viestin loppuun
                    line[rx_index] = '\0';
                    rx_index = 0;   //nollataan 

                    check_and_put_char();

                    //kuittaus
                    tud_cdc_n_write(itf, (uint8_t const *) "YKOK!\n", 6);   //yhteyskokeilu ok! == YKOK!
                    tud_cdc_n_write_flush(itf);

                    programState = UPDATE;

                    return;
                }

                else if (rx_index < BUFFER_SIZE - 1) {
                    line[rx_index++] = c;
                }
                else{
                    //jos täynnä, jätetään koko homma ja rinttiin vaan
                    buffer_overflow = true; //ei haluta lukea enää, jos owerflow->tyhjä näyttö
                    line[BUFFER_SIZE - 1] = '\0';
                    rx_index = 0;                 
                    //vasta-asemalle ilmoitus, että mitään ei otettu vastaan
                    tud_cdc_n_write(itf, (uint8_t const *) "KELA!\n", 6);   //koeta esittää lyhyemin asiasi! == KELA!
                    tud_cdc_n_write_flush(itf);
                    
                    programState = UPDATE;
                    return;
                }
            } 
        }
    }
    else {
        // kun ei haluta ottaa merkkejä vastaan, tyhjennetään itf roskikseen, jotta seuraavat viestit ei mankeloidu
        while (tud_cdc_n_available(itf)){
            char roskis[SMALL_BUFFER_SIZE];
            tud_cdc_n_read(itf, roskis, SMALL_BUFFER_SIZE);
        }
        return;

    }   
}
