#ifndef WIFI_H
#define WIFI_H

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_event.h>
#include<esp_err.h>

//macros
#define ESP_MAX_RETRY 5 //intentara conectarse 5 veces antes de arrojar un error de conexion

//bits
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define WIFI_UPDATE BIT10
#define BREAK_UPDATE_WIFI BIT11
//mqtt
#define WIFI_CREDS_READY BIT12  
#define BROKER_RECEIVED BIT3  


extern EventGroupHandle_t s_wifi_event_group;

//podemos cambiar estos datos anteriores por esta nuva estrucutra?, podriamos hacer lago parecido con TCP, para que todo este en una estrucutra agrupado y mas limpio?

/**
 * 
 * @brief esta estrucutura guaradaremos infromacion de la conexion o del estado de la conexion 
 * 
 * ssid - nombre de la red a coenctarse @memberof esp_wifi_t
 * pswd - contrasenia de la red o de la cuenta @memberof esp_wifi_t   
 * user_name - usuario de la red para inicar sesion y conectarse a la red @memberof esp_wifi_t  
 * ip - ip otorgada al dispositivo por parte de la red @memberof esp_wifi_t 
 * type_connected - valor de 0 o 1, 0 - conexion en una red normal, 1- conexion en una red de empresa @memberof esp_wifi_t 
 * connected - 1 o 0 indicando que se pudo realizar la conexion y se eucneutra coinectado el ESP @memberof esp_wifi_t
 * 
*/
typedef struct{
    char *ssid;
    char *pswd;
    char *user_name;
    esp_ip4_addr_t *ip;
    int type_connected;
    int connected;
}esp_wifi_t;

extern esp_wifi_t esp_wifi;








//prototipos
void wifi_init_sta(void);

/**
 * @brief estblecera las credeniclaes de la red, en el cao que se requiera realizar de nuevo  
 * 
 * @param SSID un apuntador al nombre de la red 
 * @param PSDW un apuntador a la contrasenia de la red
 * 
 * @return ESP_OK indica que se pudo realizar el cambio de credenciales 
 * @return ESP_FAIL no se pudo estabelcer la credenciales 
 * 
 * @details lo que hara la funcion es, este va a recibir 2 datos, por lo que estos datos seran copiados en las varibales globales de SSID y PSWD que son los que se estaran usando
 * para conectarse a la red. 
 * 
 * 
*/

void wifi_stack_init(void);

/**
 * @brief loop de conexion, usa las credenciales en esp_wifi.ssid/pswd
 *        bloquea hasta conectarse exitosamente
 *        si falla, manda ERR:WIFI_FAIL por UART y espera nuevas credenciales
 */
void wifi_connect(void);


/**
 * @brief desconecta y vuelve a conectar con las credenciales actuales en esp_wifi.
 *        llamar despues de actualizar esp_wifi.ssid / pswd.
 */
void wifi_reconnect(void);



#endif