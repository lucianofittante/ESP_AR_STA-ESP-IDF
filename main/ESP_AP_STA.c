
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>


#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "esp_spiffs.h"
#include "esp_mac.h"
#include "cJSON.h"
#include "driver/gpio.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "lwip/err.h"
#include "lwip/sys.h"





#define EXAMPLE_ESP_WIFI_SSID      "pruebas"
#define EXAMPLE_ESP_WIFI_PASS      "123456789"
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY
#define QUERY_MAX_LEN 256


#define RED "ESP"
#define PASS ""

#define CONFIGURAR 27
#define BUTTON 14



#if CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif
#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1


static const char *TAG = "ESP";


static int s_retry_num = 0;
int val;

uint8_t global_ssid[32]={0};
uint8_t global_password[64]={0};

bool led_state;
bool led_blink;


    void pin_config()
    {
        gpio_set_pull_mode(CONFIGURAR, GPIO_PULLUP_ONLY);
    }
    esp_err_t init_spiffs(void)
    {
        esp_err_t ret;

        ESP_LOGI(TAG, "Initializing SPIFFS");

        esp_vfs_spiffs_conf_t conf = {
            .base_path = "/storage",
            .partition_label = NULL,
            .max_files = 5,
            .format_if_mount_failed = true
        };

        ret = esp_vfs_spiffs_register(&conf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%d)", ret);
            return ret;
        }

        size_t total = 0, used = 0;
        ret = esp_spiffs_info(NULL, &total, &used);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get SPIFFS info (%d)", ret);
            return ret;
        }

        ESP_LOGI(TAG, "SPIFFS - Total: %d, Used: %d", total, used);

        esp_err_t ret2 = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret2);  

        ESP_ERROR_CHECK(nvs_flash_init());
        ESP_ERROR_CHECK(esp_netif_init());
        

        return ESP_OK;
    }
    void print_file_content(const char *filename) {
        ESP_LOGI(TAG, "Opening file %s", filename);
        FILE *file = fopen(filename, "rb");
        if (file == NULL) {
            ESP_LOGE(TAG, "Failed to open file for reading");
            return;
        }

        fseek(file, 0, SEEK_END);
        size_t size = ftell(file);
        fseek(file, 0, SEEK_SET);

        char *buffer = malloc(size + 1);
        if (buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory");
            fclose(file);
            return;
        }

        fread(buffer, 1, size, file);
        buffer[size] = '\0';

        fclose(file);

        ESP_LOGI(TAG, "File content:");
        ESP_LOGI(TAG, "%s", buffer);

        free(buffer);
    }
    esp_err_t read_password()
    {

        FILE *file = fopen("/storage/config.json", "rb");
        if (file == NULL)
        {
            ESP_LOGE(TAG, "Error al abrir el archivo config.json");
            return ESP_FAIL;
        }

        fseek(file, 0, SEEK_END);
        size_t json_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        char *json_content = (char *)malloc(json_size + 1);
        if (json_content == NULL)
        {
            ESP_LOGE(TAG, "Error al reservar memoria para el contenido del JSON");
            fclose(file);
            return ESP_FAIL;
        }

        fread(json_content, 1, json_size, file);
        json_content[json_size] = '\0';

        fclose(file);

        cJSON *json = cJSON_Parse(json_content);
        
        if (json == NULL)
        {
            ESP_LOGE(TAG, "Error al analizar el JSON");
            free(json_content);
            return ESP_FAIL;
        }

        cJSON *red_json = cJSON_GetObjectItem(json, "Red");
        cJSON *pass_json = cJSON_GetObjectItem(json, "Pass");

        if (red_json == NULL || pass_json == NULL || !cJSON_IsString(red_json) || !cJSON_IsString(pass_json))
        {
            ESP_LOGE(TAG, "Error al obtener los valores de Red y Pass del JSON");
            cJSON_Delete(json);
            free(json_content);
            return ESP_FAIL;
        }

        // Convertir las cadenas de caracteres de red y contraseña en matrices de bytes
        memcpy(global_ssid, red_json->valuestring, sizeof(global_ssid));
        memcpy(global_password, pass_json->valuestring, sizeof(global_password));

        cJSON_Delete(json);
        free(json_content);

        return ESP_OK;
    }
    static void wifi_event_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data){
        if (event_id == WIFI_EVENT_AP_STACONNECTED) {
            wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
            ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                    MAC2STR(event->mac), event->aid);
        } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
            wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
            ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
                    MAC2STR(event->mac), event->aid);
        }
    }
    void wifi_init_softap(void)
    {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_ap();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                            ESP_EVENT_ANY_ID,
                                                            &wifi_event_handler,
                                                            NULL,
                                                            NULL));

        wifi_config_t wifi_config = {
            .ap = {
                .ssid = RED,
                .ssid_len = strlen(RED),
                .channel = 1,
                .password = PASS,
                .max_connection = 4,
    #ifdef CONFIG_ESP_WIFI_SOFTAP_SAE_SUPPORT
                .authmode = WIFI_AUTH_WPA3_PSK,
                .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
    #else 
                .authmode = WIFI_AUTH_WPA2_PSK,
    #endif
                .pmf_cfg = {
                        .required = true,
                },
            },
        };
        if (strlen(PASS) == 0) {
            wifi_config.ap.authmode = WIFI_AUTH_OPEN;
        }

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
                RED, PASS, 1);
    }
    static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
        if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
            if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
                esp_wifi_connect();
                s_retry_num++;
                ESP_LOGI(TAG, "retry to connect to the AP");
            } else {
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            }
            ESP_LOGI(TAG,"connect to the AP fail");
        } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
            s_retry_num = 0;
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
    void wifi_init_sta()
    {
        s_wifi_event_group = xEventGroupCreate();

        ESP_ERROR_CHECK(esp_netif_init());

        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        esp_event_handler_instance_t instance_any_id;
        esp_event_handler_instance_t instance_got_ip;
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                            ESP_EVENT_ANY_ID,
                                                            &event_handler,
                                                            NULL,
                                                            &instance_any_id));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                            IP_EVENT_STA_GOT_IP,
                                                            &event_handler,
                                                            NULL,
                                                            &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = {0}, //global_ssid,
            .password = {0}, //global_password,
            .threshold = {
                .authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD
            },
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
        }
        };


        // Copia el contenido de global_ssid y global_password a las matrices en wifi_config
        memcpy(wifi_config.sta.ssid, global_ssid, sizeof(global_ssid));
        memcpy(wifi_config.sta.password, global_password, sizeof(global_password));

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
        ESP_ERROR_CHECK(esp_wifi_start() );

        ESP_LOGI(TAG, "wifi_init_sta finished.");

        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                pdFALSE,
                pdFALSE,
                portMAX_DELAY);

        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                    global_ssid, global_password);
        } else if (bits & WIFI_FAIL_BIT) {
            ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                    EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
            ESP_LOGW(TAG, "Fallo modo estacion, pasando a AP");
        } else {
            ESP_LOGE(TAG, "UNEXPECTED EVENT");
        }

    
    }
    esp_err_t read_index_html(char **html_content, size_t *html_size){

        FILE *file = fopen("/storage/index.html", "rb");
        if (file == NULL)
        {
            ESP_LOGE(TAG, "Error al abrir el archivo index.html");
            return ESP_FAIL;
        }

        fseek(file, 0, SEEK_END);
        *html_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        *html_content = (char *)malloc(*html_size + 1);
        if (*html_content == NULL)
        {
            ESP_LOGE(TAG, "Error al reservar memoria para el contenido del archivo");
            fclose(file);
            return ESP_FAIL;
        }

        fread(*html_content, 1, *html_size, file);
        (*html_content)[*html_size] = '\0';

        fclose(file);

        return ESP_OK;
    }
    esp_err_t read_config_html(char **html_content, size_t *html_size)
    {
        FILE *file = fopen("/storage/redconfig.html", "rb");
        if (file == NULL)
        {
            ESP_LOGE(TAG, "Error al abrir el archivo config.html");
            return ESP_FAIL;
        }

        fseek(file, 0, SEEK_END);
        *html_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        *html_content = (char *)malloc(*html_size + 1);
        if (*html_content == NULL)
        {
            ESP_LOGE(TAG, "Error al reservar memoria para el contenido del archivo");
            fclose(file);
            return ESP_FAIL;
        }

        fread(*html_content, 1, *html_size, file);
        (*html_content)[*html_size] = '\0';

        fclose(file);

        return ESP_OK;
    }
    esp_err_t read_style(char **html_content, size_t *html_size)
    {
        FILE *file = fopen("/storage/style.css", "rb");
        if (file == NULL)
        {
            ESP_LOGE(TAG, "Error al abrir el archivo style.css");
            return ESP_FAIL;
        }

        fseek(file, 0, SEEK_END);
        *html_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        *html_content = (char *)malloc(*html_size + 1);
        if (*html_content == NULL)
        {
            ESP_LOGE(TAG, "Error al reservar memoria para el contenido del archivo");
            fclose(file);
            return ESP_FAIL;
        }

        fread(*html_content, 1, *html_size, file);
        (*html_content)[*html_size] = '\0';

        fclose(file);

        return ESP_OK;
    }
    esp_err_t read_codejs(char **html_content, size_t *html_size)
    {
        FILE *file = fopen("/storage/code.js", "rb");
        if (file == NULL)
        {
            ESP_LOGE(TAG, "Error al abrir el archivo style.css");
            return ESP_FAIL;
        }

        fseek(file, 0, SEEK_END);
        *html_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        *html_content = (char *)malloc(*html_size + 1);
        if (*html_content == NULL)
        {
            ESP_LOGE(TAG, "Error al reservar memoria para el contenido del archivo");
            fclose(file);
            return ESP_FAIL;
        }

        fread(*html_content, 1, *html_size, file);
        (*html_content)[*html_size] = '\0';

        fclose(file);

        return ESP_OK;
    }
    esp_err_t root_handler(httpd_req_t *req){

        char *html_content;
        size_t html_size;
        esp_err_t ret = read_index_html(&html_content, &html_size);
        if (ret != ESP_OK)
        {
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }

        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, html_content, html_size);

        free(html_content);

        return ESP_OK;}
    esp_err_t root_config(httpd_req_t *req){
        char *html_content;
        size_t html_size;
        esp_err_t ret = read_config_html(&html_content, &html_size);
        if (ret != ESP_OK)
        {
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }

        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, html_content, html_size);

        free(html_content);

        return ESP_OK;}
    esp_err_t root_style_handler(httpd_req_t *req) {
        char *html_content;
        size_t html_size;
        esp_err_t ret = read_style(&html_content, &html_size);
        if (ret != ESP_OK) {
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }

        httpd_resp_set_type(req, "text/css");
        httpd_resp_send(req, html_content, html_size);

        free(html_content);

        return ESP_OK;
    }
    esp_err_t root_codejs_handler(httpd_req_t *req) {
        char *html_content;
        size_t html_size;
        esp_err_t ret = read_codejs(&html_content, &html_size);
        if (ret != ESP_OK) {
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }

        httpd_resp_set_type(req, "text/css");
        httpd_resp_send(req, html_content, html_size);

        free(html_content);

        return ESP_OK;
    }
    void print_wifi_mode(void)
    {
        wifi_mode_t mode;
        if (esp_wifi_get_mode(&mode) == ESP_OK)
        {
            switch (mode)
            {
            case WIFI_MODE_NULL:
                ESP_LOGI(TAG, "Wi-Fi mode: NULL");
                break;
            case WIFI_MODE_STA:
                ESP_LOGI(TAG, "Wi-Fi mode: Station");
                break;
            case WIFI_MODE_AP:
                ESP_LOGI(TAG, "Wi-Fi mode: Access Point");
                break;
            case WIFI_MODE_APSTA:
                ESP_LOGI(TAG, "Wi-Fi mode: Station + Access Point");
                break;
            default:
                ESP_LOGI(TAG, "Wi-Fi mode: Unknown");
                break;
            }
        }
        else
        {
            ESP_LOGE(TAG, "Failed to get Wi-Fi mode");
        }
    }
    esp_err_t save_config_to_file(const char *ssid, const char *password)
    {
        ESP_LOGI(TAG, "Guardando configuración en el archivo");

        cJSON *root = cJSON_CreateObject();
        if (root == NULL) {
            ESP_LOGE(TAG, "Error creando objeto JSON");
            return ESP_FAIL;
        }

        cJSON_AddStringToObject(root, "Red", ssid);
        
        // Limpiar la cadena de la contraseña de caracteres no imprimibles
        char cleaned_password[strlen(password) + 1]; // +1 para el carácter nulo final
        char *cleaned_ptr = cleaned_password;
        for (const char *ptr = password; *ptr; ptr++) {
            if (isprint((unsigned char)*ptr)) {
                *cleaned_ptr++ = *ptr;
            }
        }
        *cleaned_ptr = '\0';

        cJSON_AddStringToObject(root, "Pass", cleaned_password);

        char *json_str = cJSON_Print(root);
        if (json_str == NULL) {
            ESP_LOGE(TAG, "Error convirtiendo JSON a cadena");
            cJSON_Delete(root);
            return ESP_FAIL;
        }

        FILE *file = fopen("/storage/config.json", "w");
        if (file == NULL) {
            ESP_LOGE(TAG, "Error abriendo archivo config.json para escritura");
            free(json_str);
            cJSON_Delete(root);
            return ESP_FAIL;
        }

        fprintf(file, "%s", json_str);

        fclose(file);
        free(json_str);
        cJSON_Delete(root);

        ESP_LOGI(TAG, "Configuración guardada correctamente");

        return ESP_OK;
    }
    // La siguiente funcion es el llamdado para guardar la contraseña
    esp_err_t config_handler(httpd_req_t *req)
    {

        char buf[100];
        int ret, remaining = req->content_len;
        char ssid[32] = "";
        char password[64] = "";
        char *delim = "&=";

        while (remaining > 0) {
            if ((ret = httpd_req_recv(req, buf, sizeof(buf))) <= 0) {
                if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                    return ESP_OK;
                }
                return ESP_FAIL;
            }

            char *token = strtok(buf, delim);
            while (token != NULL) {
                if (strcmp(token, "red") == 0) {
                    token = strtok(NULL, delim); // Saltar "="
                    strncpy(ssid, token, sizeof(ssid) - 1);
                    ssid[sizeof(ssid) - 1] = '\0'; // Asegurar terminación nula
                } else if (strcmp(token, "contrasena") == 0) {
                    token = strtok(NULL, delim); // Saltar "="
                    strncpy(password, token, sizeof(password) - 1);
                    password[sizeof(password) - 1] = '\0'; // Asegurar terminación nula
                }
                token = strtok(NULL, delim); // Avanzar al siguiente par clave-valor
            }

            remaining -= ret;
        }

        ESP_LOGI(TAG, "Red recibida: %s", ssid);
        ESP_LOGI(TAG, "Contraseña recibida: %s", password);

        save_config_to_file(ssid, password); // Llamar a save_config_to_file con los valores recibidos

        return ESP_OK;
    }
    void iniciar_wifi()
    {  
        bool configurar = gpio_get_level(CONFIGURAR) == 1; // Verificar si el pin de configuración está en alto (1)

        if (!configurar)
        {
            wifi_init_softap();
        }
        else
        {
            wifi_init_sta();
        }
        
    }
    void imprimired(){

        char ssid_str[sizeof(global_ssid) + 1]; // +1 for null terminator
        char password_str[sizeof(global_password) + 1]; // +1 for null terminator

    // Convertir global_ssid y global_password a cadenas
        snprintf(ssid_str, sizeof(ssid_str), "%s", global_ssid);
        snprintf(password_str, sizeof(password_str), "%s", global_password);

    // Imprimir las cadenas en el registro de eventos
        ESP_LOGE(TAG, "red %s", ssid_str);
        ESP_LOGE(TAG, "pass %s", password_str);

    }

    void http_server_task(void *pvParameter)
    {
            httpd_handle_t server = NULL;  
            httpd_config_t config = HTTPD_DEFAULT_CONFIG();
     
            config.max_uri_handlers = 16;

            if (httpd_start(&server, &config) == ESP_OK)
            {
                wifi_mode_t mode;
                esp_err_t err = esp_wifi_get_mode(&mode); 

                if(err == ESP_OK){
                    
                            httpd_uri_t root = {
                            .uri = "/",
                            .method = HTTP_GET,
                            .handler = root_handler,
                            .user_ctx = NULL};
                        httpd_register_uri_handler(server, &root);

                        // Registrar el manejador para config.html
                        httpd_uri_t config_uri = {
                            .uri = "/configurar",
                            .method = HTTP_GET,
                            .handler = root_config,
                            .user_ctx = NULL};
                        httpd_register_uri_handler(server, &config_uri);
            
                        httpd_uri_t root_st = {
                            .uri = "/style.css",
                            .method = HTTP_GET,
                            .handler = root_style_handler,
                            .user_ctx = NULL
                        };
                        httpd_register_uri_handler(server, &root_st);
                                
                        httpd_uri_t post_uri = {
                            .uri = "/guardar",
                            .method = HTTP_POST,
                            .handler = config_handler,
                            .user_ctx = NULL
                        };
                        httpd_register_uri_handler(server, &post_uri);
                                                                    
                        httpd_uri_t root_codejs = {
                            .uri = "/code.js",
                            .method = HTTP_GET,
                            .handler = root_codejs_handler,
                            .user_ctx = NULL
                        };
                        httpd_register_uri_handler(server, &root_codejs);

            }
                      while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
            }

    }


    void app_main(void)
    {
        
        // INICIALIZACION GLOBAL DE LA ESP

        pin_config();    
        init_spiffs();    
        read_password();    
        iniciar_wifi();

    // Crear tarea para el núcleo 1
     xTaskCreatePinnedToCore(&http_server_task, "http_server_task", 4096, NULL, 5, NULL, 1);

        vTaskDelay(pdMS_TO_TICKS(100));
    }


