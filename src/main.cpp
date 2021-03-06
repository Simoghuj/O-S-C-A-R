#define GRIDUI_LAYOUT_DEFINITION
#include "layout.hpp"           //  pro grafické rozhraní
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/periph_ctrl.h"
#include "driver/gpio.h"
#include "driver/pcnt.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "defines.hpp"
#include "utils.hpp"
#include "init.hpp"
#include "uart.hpp"
#include "driver.hpp"
#include "bluetooth.hpp"         //  pro grafické rozhraní
#include "gridui.h"         //  pro grafické rozhraní
#include "rbprotocol.h"     //  pro grafické rozhraní
#include "rbwebserver.h"    //  pro grafické rozhraní
#include "rbwifi.h"         //  pro grafické rozhraní

using namespace rb;
/*
static void initGridUi() {
    using namespace gridui;
    // Initialize WiFi
    WiFi::startAp("Oskar95","oskaroskar");     //esp vytvoří wifi sít
    // WiFi::connect("Jmeno wifi", "Heslo");    //připojení do místní sítě

    // Initialize RBProtocol
    auto *protocol = new Protocol("burda", "Oskar", "Compiled at " __DATE__ " " __TIME__, [](const std::string& cmd, rbjson::Object* pkt) {
        UI.handleRbPacket(cmd, pkt);
    });
    protocol->start();
    // Start serving the web page
    rb_web_start(80);
    // Initialize the UI builder
    UI.begin(protocol);
    // Build the UI widgets. Positions/props are set in the layout, so most of the time,
    // you should only set the event handlers here.
    auto builder = gridui::Layout.begin();
    //builder.MotorSpeed.min(MOTOR_SPEED_MIN);
    //builder.MotorSpeed.max(MOTOR_SPEED_MAX);
    builder.StartStopButton.onPress([](Button &){
        start_stop = true;
    });

    builder.StartStopButton.onRelease([](Button &){
        start_stop = false;
    });
    builder.MotorSpeed.onChanged([](Slider &s) {
        motor_speed = int(MOTOR_SPEED_COEFICIENT * s.value());
        printf("motor_speed: %f -> %d\n", s.value(), motor_speed);
    });

     builder.StopSensitivity.onChanged([](Slider &s) {
        motor_stop_sensitivity = int(s.value());
        printf("stop sensitivity:%f -> %d\n",s.value(), motor_stop_sensitivity);
    });

    builder.IRun.onChanged([](Slider &s) {
        i_run = int(s.value());
        printf("I_Run / 32:%f -> %d\n",s.value(), i_run);
    });

    // Commit the layout. Beyond this point, calling any builder methods on the UI is invalid.
    builder.commit();
}*/

    Uart drivers_uart {
        DRIVERS_UART,
        Uart::config_t {
            .baud_rate = 750000,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .rx_flow_ctrl_thresh = 122,
            .use_ref_tick = false
        },
        Uart::pins_t {
            .pin_txd = DRIVERS_UART_TXD,
            .pin_rxd = DRIVERS_UART_RXD,
            .pin_rts = UART_PIN_NO_CHANGE,
            .pin_cts = UART_PIN_NO_CHANGE
        },
        Uart::buffers_t {
            .rx_buffer_size = DRIVERS_UART_BUF_SIZE,
            .tx_buffer_size = 0,
            .event_queue_size = 0
        }
    };

    gpio_num_t opt[4] = {
            OPT_IN_3,
            OPT_IN_2,
            OPT_IN_1,
            OPT_IN_0
    };

    Driver driver[4] = {
        {drivers_uart, DRIVER_0_ADDRES, DRIVER_0_ENABLE},
    //initDriver(driver0, 8, 0);
    //vTaskDelay(500/portTICK_PERIOD_MS);
        {drivers_uart, DRIVER_1_ADDRES, DRIVER_1_ENABLE },
    //initDriver(driver1, 8, 0);
    //vTaskDelay(500/portTICK_PERIOD_MS);
        { drivers_uart, DRIVER_2_ADDRES, DRIVER_2_ENABLE },
    //initDriver(driver2, 8, 0);
    //vTaskDelay(500/portTICK_PERIOD_MS);
        { drivers_uart, DRIVER_3_ADDRES, DRIVER_3_ENABLE },
    };

    static int drvstat[4] = {0, 0, 0, 0};


    static void initDriver(Driver &driver, const int iRun, const int iHold)
    {
        driver.init();
        vTaskDelay(100 / portTICK_PERIOD_MS);
        uint32_t data = 0;
        int result = driver.get_PWMCONF(data);
        if (result != 0)
        {
            printf("PWMCONF driveru %d : ERROR  %d\n", driver.address(), result);
        }
        else{
            printf("PWMCONF driveru %d =  %08X\n", driver.address(), data);
            vTaskDelay(100 / portTICK_PERIOD_MS);

            result = driver.get_DRV_STATUS(data);}
        if (result != 0)
            printf("DRV_STATUS driveru %d : ERROR  %d\n", driver.address(), result);
        else{
            printf("DRV_STATUS driveru  %d =  %08X\n", driver.address(), data);
            vTaskDelay(100 / portTICK_PERIOD_MS);

            result = driver.read_pinState(data);}
        if (result != 0)
            printf("PIN_STATUS driveru %d : ERROR  %d\n", driver.address(), result);
        else
            printf("PIN_STATUS driveru  %d =  %08X\n", driver.address(), data);
        vTaskDelay(100 / portTICK_PERIOD_MS);

    driver.set_speed(0);                      // otáčení motoru se nastavuje zápisem rychlosti do driveru přes Uart
    driver.set_IHOLD_IRUN (8, 16);             // proud IHOLD (při stání) =8/32, IRUN (při běhu)= 8/32 (8/32 je minimum, 16/32 je maximum pro dluhodobější provoz)
    driver.enable();                          //zapnutí mptoru
    vTaskDelay(300 / portTICK_PERIOD_MS);     //doba stání pro nastavení automatiky driveru
    driver.set_IHOLD_IRUN (iRun, iHold);             //proud IHOLD =0, IRUN = 8/32 (při stání je motor volně otočný)
}




 void optcontrol(void){
    for (int i = 0; i < 4; i++){

        static int stat[4] = {0, 0, 0, 0};

        if ((gpio_get_level(opt[i]) == 1) && (stat[i] != 1))
        {
            if(drvstat[i] == 1){
                driver[i].set_speed(MOTOR_SPEED_COEFICIENT * 0.25);
            }
            else if(drvstat[i] == 2){
                driver[i].set_speed(MOTOR_SPEED_COEFICIENT * -0.25);
            } 
            //printf("slow> %d", i);
            stat[i] = 1;
            }

        if((gpio_get_level(opt[i]) != 1) && (stat[i] == 1)){
                vTaskDelay(1500 / portTICK_PERIOD_MS);
                driver[i].set_speed(0);
                drvstat[i] = 0;
                //printf("stopped due to> %d", i);
                stat[i] = 0;
            }
        }
}



extern "C" void app_main(void)
{
    gpio_set_level(VCC_IO_0, 1); // zapnuti napajeni do driveru0
    gpio_set_level(VCC_IO_1, 1); // zapnuti napajeni do driveru1
    gpio_set_level(VCC_IO_2, 1); // zapnuti napajeni do driveru2
    gpio_set_level(VCC_IO_3, 1); // zapnuti napajeni do driveru3

    gpio_set_level(GPIO_NUM_32, 1); // zapnuti siloveho napajeni do driveru
    printf("Simple Motor \n\tbuild %s %s\n", __DATE__, __TIME__);
    check_reset();
    iopins_init();
    gpio_set_level(VCC_IO_0, 0);              // reset driveru
    gpio_set_level(VCC_IO_1, 0);
    gpio_set_level(VCC_IO_2, 0);
    gpio_set_level(VCC_IO_3, 0);

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(VCC_IO_0, 1);              //zapíná VCCIO driveru
    gpio_set_level(VCC_IO_1, 1);
    gpio_set_level(VCC_IO_2, 1);
    gpio_set_level(VCC_IO_3, 1);
    nvs_init();                             //inicializace pro zápis do flash paměti

  //  initGridUi();
    /*
    Driver driver0 { drivers_uart, DRIVER_0_ADDRES, DRIVER_0_ENABLE };
    //initDriver(driver0, 8, 0);
    //vTaskDelay(500/portTICK_PERIOD_MS);
    Driver driver1 { drivers_uart, DRIVER_1_ADDRES, DRIVER_1_ENABLE };
    //initDriver(driver1, 8, 0);
    //vTaskDelay(500/portTICK_PERIOD_MS);
    Driver driver2 { drivers_uart, DRIVER_2_ADDRES, DRIVER_2_ENABLE };
    //initDriver(driver2, 8, 0);
    //vTaskDelay(500/portTICK_PERIOD_MS);
    Driver driver3 { drivers_uart, DRIVER_3_ADDRES, DRIVER_3_ENABLE };
    //initDriver(driver3, 8, 0);
    */

    // INIT DRIVERS 
    for (int i = 0; i < 4; i++){
        initDriver(driver[i], 8, 0);
        vTaskDelay(500/portTICK_PERIOD_MS);
    }

    vTaskDelay(3000 / portTICK_PERIOD_MS);


    bt();

    while(true){

        optcontrol();
        
        switch (result[0])
            {
            case 'j':
                driver[0].set_speed(-motorspeed);
                drvstat[0] = 1;
                result[0] = ' ';
            break;

            case 'k':
                driver[0].set_speed(0);
                drvstat[0] = 0;
                result[0] = ' ';
                break;

            case 'l':
                driver[0].set_speed(motorspeed);
                drvstat[0] = 2;
                result[0] = ' ';
                break;

            case 'd':
                driver[1].set_speed(-motorspeed);
                drvstat[1] = 1;
                result[0] = ' ';
                break;

            case 'e':
                driver[1].set_speed(0);
                drvstat[1] = 0;
                result[0] = ' ';
                break;
                
            case 'f':
                driver[1].set_speed(motorspeed);
                drvstat[1] = 2;
                result[0] = ' ';
                break;

            case 'g' :
                driver[2].set_speed(-motorspeed);
                drvstat[2] = 1;
                result[0] = ' ';
                break;

            case 'h':
                driver[2].set_speed(0);
                drvstat[2] = 0;
                result[0] = ' ';
                break;
                
            case 'i':
                driver[2].set_speed(motorspeed);
                drvstat[2] = 2;
                result[0] = ' ';
                break;

            case 'a':
                driver[3].set_speed(-motorspeed);
                drvstat[3] = 1;
                result[0] = ' ';
                break;

            case 'b':
                driver[3].set_speed(0);
                drvstat[3] = 0;
                result[0] = ' ';
                break;

            case 'c':
                driver[3].set_speed(motorspeed);
                drvstat[3] = 2;
                result[0] = ' ';
                break;

            case 'o':
                for (int i = 0; i < 4;i++){
                    driver[i].set_speed(0);
                    drvstat[i] = 0;
                }
                result[0] = ' ';
                break;
            
            case 'p':                                   //disable stepper
                for(int i = 0; i<4; i++){
                    driver[i].set_speed(0);                      
                    driver[i].set_IHOLD_IRUN (0, 0);
                }
                result[0] = ' ';
                break;
            case 'q':
                for(int i = 0; i<4; i++){                    
                    driver[i].set_IHOLD_IRUN (16, 25);
                }
                result[0] = ' ';
                break;

            default:
                if((atoi(result) <= 100) && (atoi(result) != 0) && (motorspeed != atoi(result))){
                    motorspeed = atoi(result);
                    motorspeed = motorspeed / 20;
                    motorspeed = MOTOR_SPEED_COEFICIENT * motorspeed;

                    for (int i = 0; i < 4;i++){
                    //int recived = driver[i].read_speed(a);
                    //ESP_LOGI(SPP_TAG," %d:: a: %X recived: %d",i ,  a, recived);
                        if(drvstat[i] == 1){
                            //ESP_LOGI(SPP_TAG,"motorspeed: %f", motorspeed);
                            driver[i].set_speed(-motorspeed);
                        }

                        else if(drvstat[i] == 2){
                            driver[i].set_speed(motorspeed);
                        }
                    }
                    
                }
                for(int i = 0; i < sizeof(result); i++){
                    result[i] = ' ';
                }

                
                    break;
            }

        vTaskDelay(10/portTICK_PERIOD_MS);
    }
}

