/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "led_strip.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "TCPCommon.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/altcp.h"
#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"

#include "lwip/ip_addr.h"

#include "mbedtls/debug.h"

void connectMQTT();

/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      "xxxxxxxx"
#define EXAMPLE_ESP_WIFI_PASS      "xxxxxxxx"
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

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

static const char *TAG = "wifi station";

/* Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO CONFIG_BLINK_GPIO

static uint8_t s_led_state = 0;

#ifdef CONFIG_BLINK_LED_RMT
static led_strip_t *pStrip_a;

static void blink_led(void)
{
    /* If the addressable LED is enabled */
    if (s_led_state) {
        /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
        pStrip_a->set_pixel(pStrip_a, 0, 16, 16, 16);
        /* Refresh the strip to send data */
        pStrip_a->refresh(pStrip_a, 100);
    } else {
        /* Set all LED off to clear all pixels */
        pStrip_a->clear(pStrip_a, 50);
    }
}

static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink addressable LED!");
    /* LED strip initialization with the GPIO and pixels number*/
    pStrip_a = led_strip_init(CONFIG_BLINK_LED_RMT_CHANNEL, BLINK_GPIO, 1);
    /* Set all LED off to clear all pixels */
    pStrip_a->clear(pStrip_a, 50);
}

#elif CONFIG_BLINK_LED_GPIO

static void blink_led(void)
{
    /* Set the GPIO level according to the state (LOW or HIGH)*/
    gpio_set_level(BLINK_GPIO, s_led_state);
}

static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink GPIO LED!");
    gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

#endif


static int s_retry_num = 0;

bool wifiConnected = false;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
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

extern "C" void wifi_init_sta(void)
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
        // .sta = {
        //     // .ssid = EXAMPLE_ESP_WIFI_SSID,
        //     // .password = EXAMPLE_ESP_WIFI_PASS,
        //     /* Setting a password implies station will connect to all security modes including WEP/WPA.
        //      * However these modes are deprecated and not advisable to be used. Incase your Access point
        //      * doesn't support WPA2, these mode can be enabled by commenting below line */
	    //  .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
	    //  .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        // },
    };
    strcpy((char*) wifi_config.sta.ssid, EXAMPLE_ESP_WIFI_SSID);
    strcpy((char*) wifi_config.sta.password, EXAMPLE_ESP_WIFI_PASS);
    wifi_config.sta.threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD;
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
        wifiConnected = true;
        ESP_LOGI(TAG, "Connecting to MQTT");
        connectMQTT();
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

err_t _tcp_connected(void* arg, altcp_pcb* tpcb, err_t err){
#if H4AT_DEBUG
    auto ip_ = altcp_get_ip(tpcb,0);
    const char* ip_str = ip4addr_ntoa(&(ip_->u_addr.ip4));
    H4AT_PRINT1("_tcp_connected %p p=%p e=%d IP=%s:%d\n",arg,tpcb,err,ip_str,altcp_get_port(tpcb,0));
#endif
    altcp_recv(tpcb, &_raw_recv);
    altcp_sent(tpcb, &_raw_sent);

    // Connect to MQTT
    uint8_t data[] = {0x10, 0x20, 0x00, 0x04, 0x4d, 0x51, 0x54, 0x54, 0x04, 0x06, 0x46, 0x50, 0x00, 0x08, 0x75, 0x69,
                      0x69, 0x6e, 0x70, 0x75, 0x74, 0x73, 0x00, 0x02, 0x4d, 0x51, 0x00, 0x06, 0x44, 0x42, 0x43, 0x42,
                      0x33, 0x34};
    _tcp_write(tpcb, data, sizeof(data));
    return ERR_OK;
}


struct altcp_pcb* pcb;
void connectMQTT() {
    ip_addr_t ip = IPADDR4_INIT_BYTES(192,168,100,4);
    H4AT_PRINT2("_connect p=%p state=%d\n",pcb, pcb ? getTCPState(pcb, H4AT_TLS) : -1);
    H4AT_PRINT4("ip %s port %d\n", ipaddr_ntoa(&ip), MQTT_PORT);
    LwIPCoreLocker lock;
#if LWIP_ALTCP
    altcp_allocator_t allocator {altcp_tcp_alloc, nullptr};
#if H4AT_TLS
    // H4AT_PRINT1("_URL.secure=%d\ttls_mode=%d\n", _URL.secure, _tls_mode);
    // if (_URL.secure && _tls_mode != H4AT_TLS_NONE){
    H4AT_PRINT1("Setting the secure config PCB=%p\n", pcb); // ENSURE YOU'VE CLOSED THE PREVIOUS CONNECTION by rq->close() if didn't receive onConnectFail/onDisconnect() callbacks.
    // secure.
    altcp_tls_config *_tlsConfig;
    // auto &ca_cert = _keys[H4AT_TLS_CA_CERTIFICATE];

    _tlsConfig = altcp_tls_create_config_client((uint8_t*) MQTT_CERT.data(), MQTT_CERT.length()+1);
    H4AT_PRINT2("ONE WAY TLS _tlsConfig=%p\n", _tlsConfig);
    if (_tlsConfig) {
        allocator = altcp_allocator_t{altcp_tls_alloc, _tlsConfig};
    }
    else {
        H4AT_PRINT1("INVALID TLS CONFIGURATION\n");
        // _notify(H4AT_BAD_TLS_CONFIG);
        return;
    }
    // } else {
    //     H4AT_PRINT1("SETTING TCP CHANNEL\n");
    //     allocator = altcp_allocator_t {altcp_tcp_alloc, nullptr};
    // }
#endif
#else
#endif
    pcb = altcp_new_ip_type(&allocator, IPADDR_TYPE_ANY);
    if (!pcb) {
        H4AT_PRINT1("NO PCB ASSIGNED!\n");
        // _notify(H4AT_ERR_NO_PCB);
        return;
    }
    altcp_arg(pcb, 0);
    altcp_err(pcb, &_raw_error);
    altcp_connect(pcb, &ip, MQTT_PORT,(altcp_connected_fn)&_tcp_connected);
}

// extern "C" void CPPFunc() {
//     connectMQTT();
// }

extern "C" void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    /* Configure the peripheral according to the LED type */
    configure_led();
#ifdef MBEDTLS_DEBUG_C
	mbedtls_debug_set_threshold(2);
#endif
    bool mqttConnecting = false;
    while (1) {
        ESP_LOGI(TAG, "Turning the LED %s!", s_led_state == true ? "ON" : "OFF");
        blink_led();
        /* Toggle the LED state */
        s_led_state = !s_led_state;
        vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);

        if (wifiConnected && !mqttConnecting) {
            mqttConnecting = true;
            // ESP_LOGI(TAG, "WiFi Connected, Connecting to MQTT!");
            // connectMQTT();
            // CPPFunc();
            
        }
    }
}