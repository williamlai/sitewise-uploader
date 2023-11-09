#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include "esp_random.h"

#include "cJSON.h"

#include "sitewise.h"

/**
 * Fill UUID 128-bit into buffer.
 *
 * @param[in] uuidbuf
 */
static void createUUID128(char uuidbuf[33])
{
    char *p = uuidbuf;
    uint8_t uuid[16];

    esp_fill_random(uuid, 16);
    for (int i = 0; i < 16; i++)
    {
        p += sprintf(p, "%02x", uuid[i]);
    }
}

int Sitewise_printEntriesAsJson(char *payloadBuffer, size_t payloadBufferSize, Entry_t *entriesArray, size_t entriesLen)
{
    int result = SITEWISE_ERROR_NONE;
    cJSON *root = cJSON_CreateObject();
    cJSON *entries = cJSON_AddArrayToObject(root, "entries");

    for (size_t entriesIndex = 0; entriesIndex < entriesLen; entriesIndex++)
    {
        Entry_t *pEntry = &(entriesArray[entriesIndex]);

        char uuid[33];
        createUUID128(uuid);

        cJSON *entry = cJSON_CreateObject();
        cJSON_AddItemToArray(entries, entry);

        cJSON *entryId = cJSON_AddStringToObject(entry, "entryId", uuid);
        cJSON *assetId = cJSON_AddStringToObject(entry, "assetId", pEntry->assetId);
        cJSON *propertyId = cJSON_AddStringToObject(entry, "propertyId", pEntry->propertyId);
        cJSON *propertyValues = cJSON_AddArrayToObject(entry, "propertyValues");

        for (size_t propertyValuesIndex = 0; propertyValuesIndex < pEntry->propertyValuesLen; propertyValuesIndex++)
        {
            PropertyValue_t *pPropertyValue = &(pEntry->propertyValues[propertyValuesIndex]);

            cJSON *propertyValue = cJSON_CreateObject();
            cJSON_AddItemToArray(propertyValues, propertyValue);

            cJSON *value = cJSON_AddObjectToObject(propertyValue, "value");
            switch(pPropertyValue->type)
            {
                case PROPERTY_VALUE_TYPE_BOOLEAN:
                {
                    cJSON *booleanValue = cJSON_AddBoolToObject(value, "booleanValue", pPropertyValue->booleanValue);
                    break;
                }
                case PROPERTY_VALUE_TYPE_DOUBLE:
                {
                    cJSON *doubleValue = cJSON_AddNumberToObject(value, "doubleValue", pPropertyValue->doubleValue);
                    break;
                }
                case PROPERTY_VALUE_TYPE_INTEGER:
                {
                    cJSON *integerValue = cJSON_AddNumberToObject(value, "integerValue", pPropertyValue->integerValue);
                    break;
                }
                case PROPERTY_VALUE_TYPE_STRING:
                {
                    cJSON *stringValue = cJSON_AddStringToObject(value, "stringValue", pPropertyValue->stringValue);
                    break;
                }
                default:
                    break;
            }
            cJSON *timestamp = cJSON_AddObjectToObject(propertyValue, "timestamp");
            cJSON *timeInSeconds = cJSON_AddNumberToObject(timestamp, "timeInSeconds", pPropertyValue->timeInSeconds);
            cJSON *offsetInNanos = cJSON_AddNumberToObject(timestamp, "offsetInNanos", 0);
            cJSON *quality = cJSON_AddStringToObject(propertyValue, "quality", "GOOD");
        }
    }

    cJSON_PrintPreallocated(root, payloadBuffer, payloadBufferSize, 0);

    cJSON_Delete(root);

    return result;
}