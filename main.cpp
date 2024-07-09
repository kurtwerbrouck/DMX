#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <bits/stdc++.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"

#include "nvs_flash.h"

#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_sntp.h"

#include "esp_system.h"
#include "esp_log.h"

#include "lwip/apps/sntp.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/ip_addr.h"

#include <sys/time.h>

#include "led_strip.h"

using namespace std;

#define SSID "telenet-7D56AC7"
#define PWD  "rX26prmuBdwr"

static EventGroupHandle_t s_wifi_event_group;  
#define WIFI_CONNECTED_BIT          BIT0
#define WIFI_FAIL_BIT               BIT1

int s_retry_num                     = 0;

#define PORT                        6454

#define EXAMPLE_ESP_MAXIMUM_RETRY  (3)

static const char *TAG_WIFI         = "STA";
static const char *TAG              = "UDP";

TaskHandle_t  Led_Strip_th          = NULL;
QueueHandle_t Led_Strip_queue       = NULL;

led_strip_handle_t led_strip;
led_strip_handle_t led_strip2;

struct Led_Strip_Data
{
  char data[18+510];
  uint16_t lenght;
  led_strip_handle_t LED_STRIP_HANDLE;
};
struct Led_Strip_Data LED_STRIP_DATA;
struct Led_Strip_Data LED_STRIP_Data;

void Led_strip_data_update(void *pvParam)
{
  struct Led_Strip_Data LED_STRIP_DATA_RECEIVED;
 
  ESP_LOGI("task", "started");
  BaseType_t rc;
  while (true)
  {
    rc = xQueueReceive(Led_Strip_queue,&LED_STRIP_DATA_RECEIVED,portMAX_DELAY);
    if (rc == pdPASS) 
    { 
      if (LED_STRIP_DATA_RECEIVED.data[9] == 0x50 )
      {       
        int dmx_start = 18;
        //ESP_LOGI(TAG, "%04x", LED_STRIP_DATA_RECEIVED.lenght);
        for (int i=0;i<LED_STRIP_DATA_RECEIVED.lenght;i++) //for (int i=0;i<169;i++)
        {
          led_strip_set_pixel(LED_STRIP_DATA_RECEIVED.LED_STRIP_HANDLE,i,LED_STRIP_DATA_RECEIVED.data[dmx_start+0],LED_STRIP_DATA_RECEIVED.data[dmx_start+1],LED_STRIP_DATA_RECEIVED.data[dmx_start+2]);
          dmx_start = dmx_start + 3;
        }
        led_strip_refresh(LED_STRIP_DATA_RECEIVED.LED_STRIP_HANDLE);//(led_strip);
      }  
    }
  }
  vTaskDelete(NULL);
}

static void udp_server_task(void *pvParameters)
{
    char rx_buffer[18+510];
    char addr_str[128];
    int addr_family = (int)pvParameters;
    int ip_protocol = 0;
    struct sockaddr_in6 dest_addr;

    while (1)
    {
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(PORT);
        ip_protocol = IPPROTO_IP;

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0)
        {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created");

        // Set timeout
        struct timeval timeout;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

        int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0)
        {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        }
        ESP_LOGI(TAG, "Socket bound, port %d", PORT);

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t socklen = sizeof(source_addr);

        while (1)
        { 
          char* rx_buffer2 = new char[18+510];
          //ESP_LOGI(TAG, "Waiting for data");
          int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

          if (len > 0) 
          {
              rx_buffer[len] = 0; // Null-terminate
             
              switch (rx_buffer[14])
              {
                case 0x00:  LED_STRIP_Data.LED_STRIP_HANDLE = led_strip;
                            break;
                case 0x01:  LED_STRIP_Data.LED_STRIP_HANDLE = led_strip2;
                            break;
                default:    break;
              }
              
              char16_t lenght = 0x0000;
              lenght = rx_buffer[16];
              lenght = lenght << 8;
              lenght = (lenght & 0xff00) | (rx_buffer[17] & 0x00ff);
              LED_STRIP_Data.lenght = lenght/3;

              //ESP_LOGI(TAG, "Received %d bytes from %s:", len,rx_buffer);
              //ESP_LOGI(TAG, "%x %x", rx_buffer[8],rx_buffer[9]);
              //ESP_LOGI(TAG, "%x %x", rx_buffer[14],rx_buffer[15]);
              //ESP_LOGI(TAG, "%x %x %04x", rx_buffer[16],rx_buffer[17],lenght);

              memcpy(LED_STRIP_Data.data, rx_buffer,sizeof(rx_buffer));
              xQueueOverwrite(Led_Strip_queue,&LED_STRIP_Data);
            
              // Get the sender's ip address as string
              inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
              ESP_LOGI(TAG, "Received %d bytes from %s:", len,addr_str);
              
              //sendto(sock, rx_buffer, len, 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
          }
          else
          {
              ESP_LOGI(TAG, "Did not received data");
          }
          delete rx_buffer2;
        }

        if (sock != -1)
        {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) 
    {
      esp_wifi_connect();
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) 
    {
      if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) 
      {
        esp_wifi_connect();
        s_retry_num++;
        ESP_LOGI(TAG_WIFI, "retry to connect to the AP");
      } 
      else 
      {
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
      }
        ESP_LOGI(TAG_WIFI,"connect to the AP fail");
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) 
    {
      ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
      ESP_LOGI(TAG_WIFI, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
      s_retry_num = 0;
      xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
  s_wifi_event_group = xEventGroupCreate();
  esp_netif_init();
  esp_event_loop_create_default();
  esp_netif_create_default_wifi_sta();

  #define CONFIG_ESP32_WIFI_DYNAMIC_RX_BUFFER_NUM 16

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;

  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

  wifi_config_t wifi_config = {};
  strcpy((char*) wifi_config.sta.ssid,(const char*) SSID);
  strcpy((char*) wifi_config.sta.password,(const char*) PWD);
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
  esp_wifi_start();
  
  ESP_LOGI(TAG_WIFI,"start"); 

  EventBits_t bits = xEventGroupWaitBits( s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

  if (bits & WIFI_CONNECTED_BIT) 
  {
      wifi_ap_record_t ap_info;
      memset(&ap_info,0,sizeof(ap_info));
      esp_wifi_sta_get_ap_info(&ap_info);
      //ESP_LOGI(TAG_WIFI, "RSSI: %d", ap_info.rssi);
      //ESP_LOGI(TAG_WIFI, "Country (CC): %s", (char *)ap_info.country.cc); // otherwise compiler wasn't happy
      //ESP_LOGI(TAG_WIFI, "Channel: %d", ap_info.primary);
      ESP_LOGI(TAG_WIFI, "SSID: %s",ap_info.ssid);

      TickType_t xLastWakeTime;
      xLastWakeTime = xTaskGetTickCount();
      xTaskDelayUntil(&xLastWakeTime,1500/portTICK_PERIOD_MS);
      
      xTaskCreatePinnedToCore(&udp_server_task, "udp_server", 4096, (void *)AF_INET, 15, NULL, 0);
      xTaskCreatePinnedToCore(&Led_strip_data_update, "Led_strip_data_update",  4096,NULL, 15, NULL,1);
  } 
  else if (bits & WIFI_FAIL_BIT) 
  {
    ESP_LOGI(TAG_WIFI, "Failed to connect to SSID");
  } 
  else 
  {
    ESP_LOGE(TAG_WIFI, "UNEXPECTED EVENT");
  }          
}

extern "C" void app_main(void)
{
    Led_Strip_queue = xQueueCreate(1,sizeof(LED_STRIP_DATA));

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
   
    //vTaskDelete(NULL);

    led_strip_config_t strip_config = 
    {
      .strip_gpio_num = 2, // The GPIO that connected to the LED strip's data line
      .max_leds = 170, // The number of LEDs in the strip,
      .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
      .led_model = LED_MODEL_WS2812, // LED strip model
      .flags = {.invert_out = false}, // whether to invert the output signal (useful when your hardware has a level inverter)
    };

    led_strip_rmt_config_t rmt_config = {
    #if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
        .rmt_channel = 0,
    #else
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 256,
        .flags = {.with_dma = false},
    #endif
    };

    led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);

    led_strip_config_t strip_config2 = 
    {
      .strip_gpio_num = 4, // The GPIO that connected to the LED strip's data line
      .max_leds = 170, // The number of LEDs in the strip,
      .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
      .led_model = LED_MODEL_WS2812, // LED strip model
      .flags = {.invert_out = false}, // whether to invert the output signal (useful when your hardware has a level inverter)
    };

    led_strip_rmt_config_t rmt_config2 = {
    #if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
        .rmt_channel = 0,
    #else
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 256,
        .flags = {.with_dma = false},
    #endif
    };

    led_strip_new_rmt_device(&strip_config2, &rmt_config2, &led_strip2);

    led_strip_clear (led_strip2); 
    led_strip_set_pixel (led_strip2, 1,'\xff',0,0 );
    led_strip_refresh(led_strip2);
    led_strip_clear (led_strip); 
    led_strip_set_pixel (led_strip, 1,0,255,0 );
    led_strip_refresh(led_strip);
    wifi_init_sta();
    vTaskDelete(NULL);
}