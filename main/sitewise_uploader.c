#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "sdkconfig.h"

#include "lwip/err.h"
#include "lwip/apps/sntp.h"

#include "aws_sig_v4_signing.h"

#include "dht11.h"
#include "sitewise.h"

static const char *TAG = "sitewise_uploader";

/**
 * Whenever the thread dht11_read_task collects enough samples, it'll enqueue entries into this queue.
 * Then sitewise_upload_task dequeue from this queue and upload the entries.
 */
static QueueHandle_t entriesQueue = NULL;

/* Payload buffer of the HTTP request*/
static char http_payload[4096];

/* Receiving buffer for the response of the HTTP request. */
static char recv_buffer[2048];

/**
 * Send a HTTP POST request to the RESTful API: 
 *      https://docs.aws.amazon.com/iot-sitewise/latest/APIReference/API_BatchPutAssetPropertyValue.html
 * 
 * @param[in] entriesArray The entries to be uploaded
 * @param[in] entriesLen The length of the entries
 */
static void do_http_post(Entry_t *entriesArray, size_t entriesLen)
{
    struct timeval tv;
    time_t nowtime;
    struct tm *nowtm = NULL;
    char amz_date[32];
    char date_stamp[32];
    size_t payload_len = 0;

    // https://docs.aws.amazon.com/iot-sitewise/latest/APIReference/API_BatchPutAssetPropertyValue.html
    esp_http_client_config_t config = {
        .host = "data.iotsitewise." CONFIG_AWS_DEFAULT_REGION ".amazonaws.com",
        .path = "/properties",
        .query = "",
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
        .disable_auto_redirect = true,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");

    gettimeofday(&tv, NULL);
    nowtime = tv.tv_sec;
    nowtm = localtime(&nowtime);

    strftime(amz_date, sizeof amz_date, "%Y%m%dT%H%M%SZ", nowtm);
    strftime(date_stamp, sizeof date_stamp, "%Y%m%d", nowtm);

    aws_sig_v4_context_t sigv4_context;
    aws_sig_v4_config_t sigv4_config = {
        .service_name = "iotsitewise",
        .region_name = CONFIG_AWS_DEFAULT_REGION,
        .access_key = CONFIG_AWS_ACCESS_KEY,
        .secret_key = CONFIG_AWS_SECRET_KEY,
        .host = "data.iotsitewise." CONFIG_AWS_DEFAULT_REGION ".amazonaws.com",
        .method = "POST",
        .path = "/properties",
        .query = "",
        .signed_headers = "content-type",
        .canonical_headers = "content-type:application/json\n",
    };

    Sitewise_printEntriesAsJson(http_payload, sizeof(http_payload), entriesArray, entriesLen);
    payload_len = strlen(http_payload);
    // printf("%s\r\n", http_payload);

    sigv4_config.payload = http_payload;
    sigv4_config.payload_len = payload_len;
    sigv4_config.amz_date = amz_date;
    sigv4_config.date_stamp = date_stamp;
    char *auth_header = aws_sig_v4_signing_header(&sigv4_context, &sigv4_config);

    esp_http_client_set_post_field(client, http_payload, payload_len);
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_header(client, "X-Amz-Date", amz_date);

    esp_err_t err = esp_http_client_open(client, strlen(http_payload));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    } else {
        int wlen = esp_http_client_write(client, http_payload, strlen(http_payload));
        if (wlen < 0) {
            ESP_LOGE(TAG, "Write failed");
        }
        int content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0) {
            ESP_LOGE(TAG, "HTTP client fetch headers failed");
        } else {
            int data_read = esp_http_client_read_response(client, recv_buffer, sizeof(recv_buffer));
            if (data_read >= 0) {
                int statusCode = esp_http_client_get_status_code(client);
                uint64_t contentLength = esp_http_client_get_content_length(client);
                ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %" PRIu64, statusCode, contentLength);
                printf("%s\r\n", recv_buffer);
                // ESP_LOG_BUFFER_HEX(TAG, recv_buffer, strlen(recv_buffer));
            } else {
                ESP_LOGE(TAG, "Failed to read response");
            }
        }
    }
    esp_http_client_cleanup(client);
}

static void dht11_read_task(void *pvParameters)
{
    QueueHandle_t queue = (QueueHandle_t)pvParameters;
    Entry_t dht11Entries[2];
    Entry_t *pTemperatureEntry = &(dht11Entries[0]);
    Entry_t *pHumidityEntry = &(dht11Entries[1]);
    int dataCount = 0;
    int temperature = 0;
    int humidity = 0;
    struct timeval tv;

    pTemperatureEntry->assetId = CONFIG_SITEWISE_ASSET_ID;
    pTemperatureEntry->propertyId = CONFIG_SITEWISE_TEMPERATURE_PROPERTY_ID;
    pTemperatureEntry->propertyValuesLen = 0;

    pHumidityEntry->assetId = CONFIG_SITEWISE_ASSET_ID;
    pHumidityEntry->propertyId = CONFIG_SITEWISE_HUMIDITY_PROPERTY_ID;
    pHumidityEntry->propertyValuesLen = 0;

    while (1)
    {
        if (DHT11_read(CONFIG_DHT11_GPIO, &temperature, &humidity) == DHT11_ERROR_NONE)
        {
            gettimeofday(&tv, NULL);

            pTemperatureEntry->propertyValues[dataCount].type = PROPERTY_VALUE_TYPE_DOUBLE;
            pTemperatureEntry->propertyValues[dataCount].doubleValue = (double)temperature;
            pTemperatureEntry->propertyValues[dataCount].timeInSeconds = (long)(tv.tv_sec);

            pHumidityEntry->propertyValues[dataCount].type = PROPERTY_VALUE_TYPE_DOUBLE;
            pHumidityEntry->propertyValues[dataCount].doubleValue = (double)humidity;
            pHumidityEntry->propertyValues[dataCount].timeInSeconds = (long)(tv.tv_sec);

            dataCount++;
            pTemperatureEntry->propertyValuesLen = dataCount;
            pHumidityEntry->propertyValuesLen = dataCount;
            ESP_LOGI(TAG, "Collect %dth sample: T:%d H:%d", dataCount, temperature, humidity);

            if (dataCount == MAX_SITEWISE_PROPERTY_VALUE_SIZE)
            {
                /* Now we collect enough data points. We enqueue the entries.*/
                if (xQueueSend(queue, dht11Entries, portMAX_DELAY) == pdTRUE)
                {
                    ESP_LOGI(TAG, "Enqueued DHT11 data samples");
                }
                else
                {
                    ESP_LOGE(TAG, "Failed to enqueue DHT11 data samples");
                }

                dataCount = 0;
            }
        }
        else
        {
            ESP_LOGE(TAG, "Failed to read from DHT11");
        }

        vTaskDelay(CONFIG_MEASUREMENT_INTERVAL_S * 1000 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}

static void sitewise_upload_task(void *pvParameters)
{
    QueueHandle_t queue = (QueueHandle_t)pvParameters;
    Entry_t entriesArray[2];

    while (1)
    {
        if (xQueueReceive(queue, &entriesArray, portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGI(TAG, "Dequeued DHT11 data samples, and sending them to sitewise");
            do_http_post(entriesArray, 2);
        }
    }
    vTaskDelete(NULL);
}

void sitewise_uploader_start(void)
{
    /* Create a queue with capacity of 10 elements. Each element has size of 2 Entry_t */
    entriesQueue = xQueueCreate(10, sizeof(Entry_t) * 2);

    xTaskCreate(dht11_read_task, "sitewise_upload_task", 4096, entriesQueue, 5, NULL);

    xTaskCreate(sitewise_upload_task, "sitewise_upload_task", 8192, entriesQueue, 5, NULL);
}
