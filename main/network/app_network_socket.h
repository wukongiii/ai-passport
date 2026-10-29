#pragma once
#include <stdbool.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "lwip/sockets.h"
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

/* Dual-stack HTTP sockets report IPv4 destinations as mapped IPv6 addresses.
 * Check the complete address without truncating it to sockaddr_in. */
static inline bool app_net_address_is_setup(const struct sockaddr_storage *address,
                                           socklen_t length) {
    static const unsigned char setup_ip[4]={192,168,4,1};
    if(length<sizeof(address->ss_family))return false;
    if(address->ss_family==AF_INET && length>=sizeof(struct sockaddr_in)){
        const struct sockaddr_in *v4=(const struct sockaddr_in *)address;
        return !memcmp(&v4->sin_addr,setup_ip,sizeof(setup_ip));
    }
    if(address->ss_family==AF_INET6 && length>=sizeof(struct sockaddr_in6)){
        static const unsigned char mapped_prefix[12]={0,0,0,0,0,0,0,0,0,0,255,255};
        const struct sockaddr_in6 *v6=(const struct sockaddr_in6 *)address;
        const unsigned char *ip=(const unsigned char *)&v6->sin6_addr;
        return !memcmp(ip,mapped_prefix,sizeof(mapped_prefix)) &&
               !memcmp(ip+12,setup_ip,sizeof(setup_ip));
    }
    return false;
}
