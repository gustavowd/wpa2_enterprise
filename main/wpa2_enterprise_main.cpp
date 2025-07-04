/* WiFi Connection Example using WPA2 Enterprise
 *
 * Original Copyright (C) 2006-2016, ARM Limited, All Rights Reserved, Apache 2.0 License.
 * Additions Copyright (C) Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD, Apache 2.0 License.
 *
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_eap_client.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "jsoncpp/value.h"
#include "jsoncpp/json.h"

#include "esp_firebase/app.h"
#include "esp_firebase/rtdb.h"

#include "firebase_config.h"

#include <iostream>

using namespace ESPFirebase;

/* The examples use simple WiFi configuration that you can set via
   project configuration menu.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"

   You can choose EAP method via project configuration according to the
   configuration of AP.
*/
#define EXAMPLE_WIFI_SSID "UTFPR-SERVIDOR"
#define EXAMPLE_EAP_METHOD CONFIG_EXAMPLE_EAP_METHOD

#define EXAMPLE_EAP_ID "gustavo"
#define EXAMPLE_EAP_USERNAME "gustavo"
#define EXAMPLE_EAP_PASSWORD "password"

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* esp netif object representing the WIFI station */
static esp_netif_t *sta_netif = NULL;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

static const char *TAG = "example";

/* CA cert, taken from wpa2_ca.pem
   Client cert, taken from wpa2_client.crt
   Client key, taken from wpa2_client.key

   The PEM, CRT and KEY file were provided by the person or organization
   who configured the AP with wpa2 enterprise.

   To embed it in the app binary, the PEM, CRT and KEY file is named
   in the component.mk COMPONENT_EMBED_TXTFILES variable.
*/
#ifdef CONFIG_EXAMPLE_VALIDATE_SERVER_CERT
extern uint8_t ca_pem_start[] asm("_binary_ca_pem_start");
extern uint8_t ca_pem_end[]   asm("_binary_ca_pem_end");
#endif /* CONFIG_EXAMPLE_VALIDATE_SERVER_CERT */

#ifdef CONFIG_EXAMPLE_EAP_METHOD_TLS
extern uint8_t client_crt_start[] asm("_binary_wpa2_client_crt_start");
extern uint8_t client_crt_end[]   asm("_binary_wpa2_client_crt_end");
extern uint8_t client_key_start[] asm("_binary_wpa2_client_key_start");
extern uint8_t client_key_end[]   asm("_binary_wpa2_client_key_end");
#endif /* CONFIG_EXAMPLE_EAP_METHOD_TLS */

#if defined CONFIG_EXAMPLE_EAP_METHOD_TTLS
esp_eap_ttls_phase2_types TTLS_PHASE2_METHOD = CONFIG_EXAMPLE_EAP_METHOD_TTLS_PHASE_2;
#endif /* CONFIG_EXAMPLE_EAP_METHOD_TTLS */

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    }
}

static void initialise_wifi(void)
{
/*
#ifdef CONFIG_EXAMPLE_VALIDATE_SERVER_CERT
    unsigned int ca_pem_bytes = ca_pem_end - ca_pem_start;
#endif*/ /* CONFIG_EXAMPLE_VALIDATE_SERVER_CERT */

/*#ifdef CONFIG_EXAMPLE_EAP_METHOD_TLS
    unsigned int client_crt_bytes = client_crt_end - client_crt_start;
    unsigned int client_key_bytes = client_key_end - client_key_start;
#endif*/ /* CONFIG_EXAMPLE_EAP_METHOD_TLS */

    ESP_ERROR_CHECK(esp_netif_init());
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_WIFI_SSID,
            //.password = "123654abcd",
        },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    #if 1
    ESP_ERROR_CHECK( esp_eap_client_set_identity((uint8_t *)EXAMPLE_EAP_ID, strlen(EXAMPLE_EAP_ID)) );
    #endif

//#ifdef CONFIG_EXAMPLE_VALIDATE_SERVER_CERT
//    ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_set_ca_cert(ca_pem_start, ca_pem_bytes) );
//#endif /* CONFIG_EXAMPLE_VALIDATE_SERVER_CERT */

/*#ifdef CONFIG_EXAMPLE_EAP_METHOD_TLS
    ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_set_cert_key(client_crt_start, client_crt_bytes,\
    		client_key_start, client_key_bytes, NULL, 0) );
#endif*/ /* CONFIG_EXAMPLE_EAP_METHOD_TLS */

//#if defined CONFIG_EXAMPLE_EAP_METHOD_PEAP || CONFIG_EXAMPLE_EAP_METHOD_TTLS
#if 1
    ESP_ERROR_CHECK( esp_eap_client_set_username((uint8_t *)EXAMPLE_EAP_USERNAME, strlen(EXAMPLE_EAP_USERNAME)) );
    ESP_ERROR_CHECK( esp_eap_client_set_password((uint8_t *)EXAMPLE_EAP_PASSWORD, strlen(EXAMPLE_EAP_PASSWORD)) );
#endif
//#endif /* CONFIG_EXAMPLE_EAP_METHOD_PEAP || CONFIG_EXAMPLE_EAP_METHOD_TTLS */

/*#if defined CONFIG_EXAMPLE_EAP_METHOD_TTLS
    ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_set_ttls_phase2_method(TTLS_PHASE2_METHOD) );
    ESP_LOGI(TAG, "TLS phase %d", TTLS_PHASE2_METHOD);
#endif*/ /* CONFIG_EXAMPLE_EAP_METHOD_TTLS */

    #if 1
    ESP_ERROR_CHECK( esp_wifi_sta_enterprise_enable() );
    #endif
    ESP_ERROR_CHECK( esp_wifi_start() );
}

static void wpa2_enterprise_example_task(void *pvParameters)
{
    esp_netif_ip_info_t ip;
    memset(&ip, 0, sizeof(esp_netif_ip_info_t));
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    while(1) {
        esp_netif_get_ip_info(sta_netif, &ip);
        if (ip.ip.addr != 0) {
            break;
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "IP:" IPSTR, IP2STR(&ip.ip));
    ESP_LOGI(TAG, "MASK:" IPSTR, IP2STR(&ip.netmask));
    ESP_LOGI(TAG, "GW:" IPSTR, IP2STR(&ip.gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");

    // Config and Authentication
    user_account_t account = {USER_EMAIL, USER_PASSWORD};

    FirebaseApp app = FirebaseApp(API_KEY);
    app.loginUserAccount(account);
    
    RTDB db = RTDB(&app, DATABASE_URL);

    while (1) {
        vTaskDelay(2000 / portTICK_PERIOD_MS);

        // We can put a json str directly at /person1
        std::string json_str = R"({"name": "Gustavo", "age": 47, "random_float": 8.56})";
        db.putData("/person1", json_str.c_str()); 

        // We can parse the json_str and access the members and edit them
        Json::Value data;
        Json::Reader reader; 
        reader.parse(json_str, data);

        std::string madjid_name = data["name"].asString();  // convert value to primitives (read jsoncpp docs for more of these)

        data["name"] = "John Doe";
        data["age"] = 22;
        data["random_float"] = 4.44;
        
        // put json object directly
        db.putData("/person2", data);

        // Edit person2 data in the database by patching
        data["age"] = 23;
        db.patchData("person2", data);


        Json::Value root = db.getData("person1"); // retrieve person3 from database, set it to "" to get entire database

        Json::FastWriter writer;
        std::string person1_string = writer.write(root);  // convert it to json string

        ESP_LOGI("MAIN", "person1 as json string: \n%s", person1_string.c_str());

        // You can also print entire Json Value object with std::cout with converting to string 
        // you cant print directly with printf or LOGx because Value objects can have many type. << is overloaded and can print regardless of the type of the Value
        std::cout << root << std::endl;

        // print the members (Value::Members is a vector)
        Json::Value::Members members = root.getMemberNames();  
        for (const auto& member : members)
        {
            std::cout << member << ", ";
        }
        std::cout << std::endl;

        //db.deleteData("person1"); // delete person3
    }
}


extern "C" void app_main(void)
{
    ESP_ERROR_CHECK( nvs_flash_init() );
    initialise_wifi();
    xTaskCreate(&wpa2_enterprise_example_task, "wpa2_enterprise_example_task", 4096, NULL, 5, NULL);
}
