//libereias estandares
#include<string.h>

//drivers
#include<driver/uart.h>
// #include<driver/uart_types.h>

//esp log
#include<esp_log.h>
#include<esp_err.h>

//librerias propias
#include<uart_lib.h>
#include<global.h>
#include <driver/uart_vfs.h>



esp_err_t uart_init(uart_port_t uart_sel, int BoudRate, uart_word_length_t uart_data_length, uart_parity_t uart_parity, uart_stop_bits_t uart_stop_bits, int TX, int RX){


    if(uart_is_driver_installed(uart_sel)){
        uart_driver_delete(uart_sel);
    }


    uart_config_t cfg ={
        .baud_rate = BoudRate,
        .data_bits = uart_data_length,
        .parity = uart_parity,
        .stop_bits = uart_stop_bits,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk=UART_SCLK_DEFAULT,
    };

    // esp_err_t ret;

    ESP_ERROR_CHECK(uart_param_config(uart_sel, &cfg));


    ESP_ERROR_CHECK(uart_set_pin(uart_sel, TX, RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_ERROR_CHECK(uart_driver_install(uart_sel, BUFFER*2, BUFFER*2, 20, &uart_event, 0));
    uart_vfs_dev_use_driver(uart_sel);

    //vericiafamos que se pudo conectar 

    uart_write_bytes(uart_sel, "UART inicializado correctamente\n", 32);


    return ESP_OK;
}

void task_uart(void *params){

    
    task_uart_port_t *current_port = (task_uart_port_t*)params; 

    uart_event_t event;
    uint8_t *buffer = (uint8_t*)malloc(BUFFER);
    uint8_t input[BUFFER];
    int index_input=0;

    while(1){
        if(xQueueReceive(uart_event, (void*)&event, portMAX_DELAY)){
            
            switch(event.type){

                case UART_DATA : {

                    int len = uart_read_bytes(current_port->NUM_PORT, buffer,event.size, portMAX_DELAY);

                    for(int i = 0; i < len ; i++){

                        char c = buffer[i];

                        if((c >= 'a' && c<='z') || (c >= 'A' && c<='Z') || (c>=32 && c<=63) || (c=='_')){

                            uart_write_bytes(current_port->NUM_PORT, (const char*)&c, sizeof(c));
                            
                            //guardamos los datos dentro del buffer que estara guardando toda la plabra hasta que entre enter en el teclado 

                            if(index_input < BUFFER-1){
                                input[index_input++]=c;
                            }
                        }


                        else if(c == '\n' || c=='\r'){
                            // en este punto el usuadio ya
                            
                            input[index_input] ='\0'; //terminamos la cande que entro 
                            
                            if(index_input == 0){
                                // enter en vacío, ignorar
                                break;
                            }

                            char *data_flow = (char*)malloc(strlen((char*)input) + 1);
                            strcpy(data_flow, (char*)input);

                            //espera si la cola esta llega
                            xQueueSend(flow_data_queue, &data_flow, portMAX_DELAY);

                            //volvemos a resetear los valores
                            index_input =0;
                            memset(input,0, BUFFER);
                            break; //terminamos con el ciclo 
                        }

                        else if(c=='\b'){
                            
                            if(index_input > 0){
                                index_input--;
                                uart_write_bytes(current_port->NUM_PORT,"\b \b", 3);
                            }
                            
                        }

                    }



                }break;

                case UART_BUFFER_FULL :{

                    xQueueReset(uart_event);
                    uart_flush(current_port->NUM_PORT);
                }break;

                case UART_FIFO_OVF:{
                    xQueueReset(uart_event);
                    uart_flush(current_port->NUM_PORT);
                }break;


                default : {
                    // ESP_LOGE(TAG, "tipo de evento %d", event.type);
                    //mandamos por UART 
                    char default_err[64];
                    int len = snprintf(default_err, sizeof(default_err), "ERROR: tipo de error -> %d\n", event.type);
                    uart_write_bytes(current_port->NUM_PORT, default_err, len);
                }break;


            }


        }
    }




}

