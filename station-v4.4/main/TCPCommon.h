#pragma once 
#include <stdint.h>
#include "esp_heap_caps.h"
#include <string>
#include "lwip/err.h"
#include "lwip/tcpbase.h"
#define H4AT_DEBUG 2
#define H4AT_TLS    1
#ifdef LWIP_ALTCP
#undef LWIP_ALTCP
#endif
#ifdef LWIP_ALTCP_TLS
#undef LWIP_ALTCP_TLS
#endif
#ifdef LWIP_ALTCP_TLS_MBEDTLS
#undef LWIP_ALTCP_TLS_MBEDTLS
#endif
#define LWIP_ALTCP  1
#define LWIP_ALTCP_TLS  1
#define LWIP_ALTCP_TLS_MBEDTLS  1


#if H4AT_TLS
#define MQTT_PORT   8883
extern std::string  MQTT_CERT;
#else
#define MQTT_PORT   1883
#endif
// #ifdef __cplusplus
// extern "C"
// {
// #endif
uint32_t    _HAL_freeHeap(uint32_t caps=MALLOC_CAP_DEFAULT);
uint32_t    _HAL_maxHeapBlock(uint32_t caps=MALLOC_CAP_DEFAULT);
// #ifdef __cplusplus
// }
// #endif

#if H4AT_DEBUG
    #define H4AT_PRINTF(...) printf(__VA_ARGS__)
    template<int I, typename... Args>
    void H4AT_PRINT(const char* fmt, Args... args) {
        #ifdef ARDUINO_ARCH_ESP32
        if (H4AT_DEBUG >= I) H4AT_PRINTF(std::string(std::string("H4AT:%d: H=%u M=%u S=%u ")+fmt).c_str(),I,_HAL_freeHeap(),_HAL_maxHeapBlock(),uxTaskGetStackHighWaterMark(NULL),args...);
        #else
        if (H4AT_DEBUG >= I) H4AT_PRINTF(std::string(std::string("H4AT:%d: H=%u M=%u ")+fmt).c_str(),I,_HAL_freeHeap(),_HAL_maxHeapBlock(),args...);
        #endif
    }
    #define H4AT_PRINT1(...) H4AT_PRINT<1>(__VA_ARGS__)
    #define H4AT_PRINT2(...) H4AT_PRINT<2>(__VA_ARGS__)
    #define H4AT_PRINT3(...) H4AT_PRINT<3>(__VA_ARGS__)
    #define H4AT_PRINT4(...) H4AT_PRINT<4>(__VA_ARGS__)

    // template<int I>
    // void H4AT_dump(const uint8_t* p, size_t len) { if (H4AT_DEBUG >= I) dumphex(p,len); }
    #define H4AT_DUMP1(p,l) H4AT_dump<1>((p),l)
    #define H4AT_DUMP2(p,l) H4AT_dump<2>((p),l)
    #define H4AT_DUMP3(p,l) H4AT_dump<3>((p),l)
    #define H4AT_DUMP4(p,l) H4AT_dump<4>((p),l)
#else
    #define H4AT_PRINTF(...)
    #define H4AT_PRINT1(...)
    #define H4AT_PRINT2(...)
    #define H4AT_PRINT3(...)
    #define H4AT_PRINT4(...)

    #define H4AT_DUMP2(...)
    #define H4AT_DUMP3(...)
    #define H4AT_DUMP4(...)
#endif

// #ifdef __cplusplus
// extern "C"
// {
// #endif
class LwIPCoreLocker {
    static volatile int         _locks;
                    bool        _locked=false;
    public:
        LwIPCoreLocker();
                    void        unlock();
        ~LwIPCoreLocker();
                    bool        locked() { return _locked; }
                    void        lock();
};

enum tcp_state getTCPState(struct altcp_pcb *conn, bool tls);

void _raw_error(void *arg, err_t err);
err_t _raw_recv(void *arg, struct altcp_pcb *tpcb, struct pbuf *p, err_t err);
err_t _raw_sent(void* arg,struct altcp_pcb *tpcb, u16_t len);

void _tcp_write(altcp_pcb* pcb, uint8_t* data, uint16_t len);
void dumphex(const uint8_t* mem, size_t len);

// #ifdef __cplusplus
// }
// #endif