/* HTTPS GET Example using plain mbedTLS sockets
 *
 * Contacts the howsmyssl.com API via TLS v1.2 and reads a JSON
 * response.
 *
 * Adapted from the ssl_client1 example in mbedtls.
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

#include "https_request.h"

/* Constants that aren't configurable in menuconfig */
#define WEB_SERVER "v1.hitokoto.cn"
#define WEB_PORT "443"
#define WEB_URL "https://v1.hitokoto.cn/?c=f&charset=utf-8&encode=text"

static const char *TAG = "example";

static const char *REQUEST = "GET " WEB_URL " HTTP/1.0\r\n"
                             "Host: " WEB_SERVER "\r\n"
                             "User-Agent: esp-idf/1.0 esp32\r\n"
                             "Connection: close\r\n"
                             "\r\n";

/* Root cert for howsmyssl.com, taken from server_root_cert.pem

   The PEM file was extracted from the output of this command:
   openssl s_client -showcerts -connect www.howsmyssl.com:443 </dev/null

   The CA root cert is the last cert given in the chain of certs.

   To embed it in the app binary, the PEM file is named
   in the component.mk COMPONENT_EMBED_TXTFILES variable.
*/
extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
extern const uint8_t server_root_cert_pem_end[] asm("_binary_server_root_cert_pem_end");

void https_get_task(void *pvParameters)
{
    char buf[512];
    int ret, len, i, j = 0, k = 0;

    static unsigned int state = 0;

    while (1)
    {
        /* Wait for the callback to set the CONNECTED_BIT in the
           event group.
        */
        xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT,
                            false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "Connected to AP");
        esp_tls_cfg_t cfg = {
            .cacert_pem_buf = server_root_cert_pem_start,
            .cacert_pem_bytes = server_root_cert_pem_end - server_root_cert_pem_start,
        };

        struct esp_tls *tls = esp_tls_conn_http_new(WEB_URL, &cfg);

        if (tls != NULL)
        {
            ESP_LOGI(TAG, "Connection established...");
        }
        else
        {
            ESP_LOGE(TAG, "Connection failed...");
            goto exit;
        }

        size_t written_bytes = 0;
        do
        {
            ret = esp_tls_conn_write(tls,
                                     REQUEST + written_bytes,
                                     strlen(REQUEST) - written_bytes);
            if (ret >= 0)
            {
                ESP_LOGI(TAG, "%d bytes written", ret);
                written_bytes += ret;
            }
            else if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
            {
                ESP_LOGE(TAG, "esp_tls_conn_write  returned 0x%x", ret);
                goto exit;
            }
        } while (written_bytes < strlen(REQUEST));

        ESP_LOGI(TAG, "Reading HTTP response...");

        do
        {
            len = sizeof(buf) - 1;
            bzero(buf, sizeof(buf));
            ret = esp_tls_conn_read(tls, (char *)buf, len);

            if (ret == MBEDTLS_ERR_SSL_WANT_WRITE || ret == MBEDTLS_ERR_SSL_WANT_READ)
                continue;

            if (ret < 0)
            {
                ESP_LOGE(TAG, "esp_tls_conn_read  returned -0x%x", -ret);
                break;
            }

            if (ret == 0)
            {
                ESP_LOGI(TAG, "connection closed");
                break;
            }

            len = ret;
            ESP_LOGD(TAG, "%d bytes read", len);
            /* Print response directly to stdout as it is read */
            //for(int i = 0; i < len; i++) {
            //putchar(buf[i]);
            //}
            for (i = 0; i < len; i++)
            {
                switch (state)
                {
                case 0:
                    if (buf[i] == '\r')
                        state = 1;
                    break;
                case 1:
                    if (buf[i] == '\n')
                        state = 2;
                    else
                        state = 0;
                    break;

                case 2:
                    if (buf[i] == '\r')
                        state = 3;
                    else
                        state = 0;
                    break;
                case 3:
                    if (buf[i] == '\n')
                    {
                        state = 4;
                        j = i + 1;
                        k = 0;
                    }
                    else
                        state = 0;
                    break;
                }
            }
            if (state == 4)
            {
                ESP_LOGI(TAG, "Find str");
                strrpc(buf + j, "，", "");
                strrpc(buf + j, "。", "");
                printf("%s", buf + j);

                for (i = j; i < len; i += 3)
                {
                    //printf("0x%02X ",*(buf+i));
                    if (k < 64)
                    { //GBstrbuf[k++] = *(buf+i);
                        Utf8ToGb2312(buf + i, GBstrbuf + k);
                    }
                    k += 2;
                }
                state = 0;
            }
        } while (1);

    exit:
        esp_tls_conn_delete(tls);
        putchar('\n'); // JSON output doesn't have a newline at end

        static int request_count;
        ESP_LOGI(TAG, "Completed %d requests", ++request_count);

        for (int countdown = 5; countdown >= 0; countdown--)
        {
            ESP_LOGI(TAG, "%d...", countdown);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        ESP_LOGI(TAG, "Starting again!");
    }
}
