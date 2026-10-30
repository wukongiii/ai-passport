#pragma once
#include "app_strings.h"
/* Shared numeric errors from ESP-IDF / HTTP; no platform dependency. Unknown
 * codes retain their number in the caller's UI. Never infer a precise cause
 * from ESP_FAIL alone. */
static inline const char *app_error_label(int error){
    switch(error){
    case 0:return "";
    case -1:return TXT_ERROR_REQUEST;
    case 0x101:return TXT_AI_CHECK_MEMORY;
    case 0x102:return TXT_ERROR_ARGUMENT;
    case 0x103:return TXT_AI_CHECK_NETWORK;
    case 0x104:return TXT_ERROR_SIZE;
    case 0x107:return TXT_AI_CHECK_TIMEOUT;
    case 0x108:return TXT_AI_CHECK_RESPONSE;
    case 0x7002:return TXT_ERROR_CONNECT_TIMEOUT;
    case 400:case 422:return TXT_ERROR_ARGUMENT;
    case 401:case 403:return TXT_AI_CHECK_AUTH;
    case 404:return TXT_AI_CHECK_ROUTE;
    case 408:case 504:return TXT_AI_CHECK_TIMEOUT;
    case 429:return TXT_AI_CHECK_LIMIT;
    default:return error>=500&&error<=599 ? TXT_ERROR_SERVICE : TXT_ERROR_GENERIC;
    }
}
