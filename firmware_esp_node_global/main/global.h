
#ifndef GLOBAL_H
#define GLOBAL_H

//funciones, variables, estrucuturuas que compartiran los disitntos archivos. 

#include<driver/uart.h>
#include<driver/gpio.h>

#define TRUE 1
#define FALSE 0
#define UART_MAIN UART_NUM_1

#define UART_TX_PIN  17
#define UART_RX_PIN  16

// IP del broker MQTT, se llenara cuando la Raspberry la mande
extern char broker_ip[64];


/**
 * este archivo .h contendra variables, macros, colas, etc, que seran necesario o que compartiran varios archivos .c  
*/

//estrucutra que contendra el puerto de UART esot sera ideal para cunado estemos menjando diferentes puerto de UART 

typedef struct {
    uart_port_t NUM_PORT;
}task_uart_port_t;

extern task_uart_port_t global_uart;



//cola que controlara el flujo de la ifnromacion entre los diferners archivos 
extern QueueHandle_t flow_data_queue; 



#endif

