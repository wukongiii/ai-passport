#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "../app/app_preferences.h"
/* Wire protocol and persisted credential validation are independent of IDF/UI. */
enum { APP_NET_FORM_MAX=2048, APP_NET_RETRY_LIMIT=3, APP_NET_PORTAL_SECONDS=300 };
typedef struct { char ssid[33],password[65]; } app_net_credentials_t;
bool app_net_credentials_valid(const app_net_credentials_t *);
bool app_net_parse_credentials(const char *form,app_net_credentials_t *out);
bool app_net_patch_preferences(const char *form,const app_preferences_t *current,app_preferences_t *out);
bool app_net_parse_preferences(const char *form,app_preferences_t *out);
/* Strict form decoding, rejects truncation, duplicate keys and embedded NUL. */
bool app_net_form_value(const char *form,const char *key,char *out,size_t capacity);
/* Minimal captive DNS: one uncompressed A/IN question, fixed AP reply. */
size_t app_net_dns_response(const uint8_t *query,size_t length,uint8_t *out,size_t capacity);

/* Bounded retry/timing policy shared by the worker and finite host tests. */
bool app_net_schedule_retry(unsigned *attempts,int64_t now,int64_t *retry_at);
unsigned app_net_remaining_seconds(int64_t deadline,int64_t now);

enum { APP_NET_SCAN_MAX=16 };
typedef struct { char ssid[33]; int rssi; bool secured; } app_net_access_point_t;
typedef struct {
    app_net_access_point_t entries[APP_NET_SCAN_MAX];
    unsigned count,revision;
    bool scanning,attempted;
    int error;
} app_net_scan_t;
/* Merge repeated SSID/security pairs, retaining the strongest 16 networks. */
void app_net_scan_add(app_net_scan_t *,const app_net_access_point_t *);
typedef enum { APP_NET_ISSUE_GENERAL,APP_NET_ISSUE_AUTH,APP_NET_ISSUE_MISSING,APP_NET_ISSUE_SECURITY } app_net_issue_t;
app_net_issue_t app_net_issue(unsigned reason);

/* Insertion-order ring; selecting a saved SSID changes only the preferred slot.
 * Public snapshots deliberately omit passwords. */
enum { APP_NET_SAVED_MAX=10, APP_NET_HISTORY_VERSION=1 };
typedef struct {
    uint8_t version,count,next,preferred;
    app_net_credentials_t entries[APP_NET_SAVED_MAX];
} app_net_history_t;
typedef struct { char ssids[APP_NET_SAVED_MAX][33]; unsigned count,revision; } app_net_saved_t;
void app_net_history_init(app_net_history_t *);
bool app_net_history_valid(const app_net_history_t *);
int app_net_history_find(const app_net_history_t *,const char *ssid);
bool app_net_history_remember(app_net_history_t *,const app_net_credentials_t *);
void app_net_history_list(const app_net_history_t *,app_net_saved_t *);
