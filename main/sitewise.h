#ifndef _SITEWISE_H_
#define _SITEWISE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

#define MAX_SITEWISE_PROPERTY_VALUE_SIZE 10

#define SITEWISE_ERROR_NONE         (0)
#define SITEWISE_ERROR_CJSON        (-1)

#define PROPERTY_VALUE_TYPE_BOOLEAN (0)
#define PROPERTY_VALUE_TYPE_DOUBLE  (1)
#define PROPERTY_VALUE_TYPE_INTEGER (2)
#define PROPERTY_VALUE_TYPE_STRING  (3)

typedef struct PropertyValue
{
    int type;
    union
    {
        bool booleanValue;
        double doubleValue;
        int integerValue;
        char *stringValue;    
    };
    long timeInSeconds;
} PropertyValue_t;

typedef struct Entry
{
    char *assetId;
    char *propertyId;

    size_t propertyValuesLen;
    PropertyValue_t propertyValues[MAX_SITEWISE_PROPERTY_VALUE_SIZE];
} Entry_t;

/**
 *  Given a array of entries, print them into JSON format into a buffer.
 *
 * @param[in] payloadBuffer The JSON string buffer
 * @param[in] payloadBufferSize Buffer size of the JSON string buffer
 * @param[in] entriesArray Array of entries
 * @param[in] entriesLen Length of the entries array
 * @return 0 on success, non-zero value otherwise
 */
int Sitewise_printEntriesAsJson(char *payloadBuffer, size_t size, Entry_t *entriesArray, size_t entriesLen);

#ifdef __cplusplus
}
#endif

#endif /* _SITEWISE_H_ */