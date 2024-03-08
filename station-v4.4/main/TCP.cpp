#include "TCPCommon.h"
#include "lwip/tcpbase.h"
#include "lwip/altcp.h"
#include "lwip/tcp.h"
#include <exception>
#include <cmath>
#include <string.h> // memcpy

#if H4AT_TLS
std::string MQTT_CERT = R"(-----BEGIN CERTIFICATE-----
MIIDgTCCAmmgAwIBAgIUQcIf6OLWUzmZb2mPVcyOMG1HRSkwDQYJKoZIhvcNAQEL
BQAwUDELMAkGA1UEBhMCSk8xDjAMBgNVBAgMBUFtbWFuMQ4wDAYDVQQHDAVBbW1h
bjELMAkGA1UECgwCSDQxFDASBgNVBAMMC0hhbXphSGFqZWlyMB4XDTIzMDYxMDE1
MjIyMFoXDTI0MDYwOTE1MjIyMFowUDELMAkGA1UEBhMCSk8xDjAMBgNVBAgMBUFt
bWFuMQ4wDAYDVQQHDAVBbW1hbjELMAkGA1UECgwCSDQxFDASBgNVBAMMC0hhbXph
SGFqZWlyMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAxY9hJHHvhG+5
/+OaybsHkik4YfX58wa0NhdmVDZLI9Z9s251r+ztQC0VC6e5mwS+W5UcZ9WYjYbN
HVM3f+dzgwOn9wsYTPWWTfIso02Z7lXcjhKcVlK9Bq3UeOBX103fFsS2rCCxy/pt
9EXA+95E2Pjps1nDyGjjvjUGkhiIe9NXV2+1aAJfDK/pWBjlDv8eqnhvq3pj2MM4
negYxMO9sqjOjfUkNAEG5jcDlMhHEdWoL7HyaKf8ddv8xtoqrPK7XJZ/32jp2PJd
lZbH6y/7b42Dn3wIFiNbviIaoDnOtUqFJMzB9WY9u4t6be/IX2eFttTXo233pVOF
N+2GA8n3HQIDAQABo1MwUTAdBgNVHQ4EFgQUCdpwdS001EQzEqznEZdJYASEHoQw
HwYDVR0jBBgwFoAUCdpwdS001EQzEqznEZdJYASEHoQwDwYDVR0TAQH/BAUwAwEB
/zANBgkqhkiG9w0BAQsFAAOCAQEAjioxltqlpw+qkWd2uELPbPPw9ctHU0RIjxLW
/1gNkYeetSnFlVO4XcraxSlmDgk5jd1whwlO1MCi6nwW6LJAVOxX/WXrefjfVN53
r7aWMhqI3cvDOyY6ishy+iImRzzJVyla+viAvyYGPlOuL2kctNOysk20N6ZcYTdQ
DfbEr0WRFQEC5NTIdxV/864pHYA9NrG4RAcEEJsTR0yJpj4GdmN1kkJj5DCEt4I4
XyMIsWKOK7LJV2KP3y/kFrQo1cxq9FR6Mw0oSsbcyffBzaGRt0YJO06dYsPzp4k5
l1Lth94UiX4BIGiaXx4FXRqpAzLQMwEWCS1XyJVrppom8Vo3WQ==
-----END CERTIFICATE-----
)";
#endif
extern struct altcp_pcb* pcb;

enum tcp_state getTCPState(struct altcp_pcb *conn, bool tls) {
#if LWIP_ALTCP
    LwIPCoreLocker lock;
    if (conn) {
        if (tls){
            if (conn->inner_conn && conn->inner_conn->state) {
                auto inner_state = conn->inner_conn->state;
                struct tcp_pcb *pcb = (struct tcp_pcb *)inner_state;
                if (pcb)
                    return pcb->state;
            }
        } else {
            struct tcp_pcb *pcb = (struct tcp_pcb *)conn->state;
            if (conn->inner_conn) return (tcp_state)-1;
            if (pcb)
                return pcb->state;
        }
    }
    H4AT_PRINT1("GETSTATE %p NO CONN\n", conn);
    return CLOSED;
#else
    return conn->state;
#endif
}

void _raw_error(void *arg, err_t err){
    H4AT_PRINT1("_raw_error c=%p e=%d\n",arg,err);
    pcb=NULL;
}
err_t _raw_recv(void *arg, struct altcp_pcb *tpcb, struct pbuf *p, err_t err){
    H4AT_PRINT1("_raw_recv %p tpcb=%p p=%p err=%d data=%p tot_len=%d\n",arg,tpcb,p, err, p ? p->payload:NULL,p ? p->tot_len:0);
        if ((p == NULL || err!=ERR_OK) && pcb) {
            H4AT_PRINT1("Will Close!\n");
            return ERR_OK;
        }
        auto cpydata=static_cast<uint8_t*>(malloc(p->tot_len));
		pbuf_copy_partial(p,cpydata,p->tot_len,0); // instead of direct memcpy that only considers the first pbuf of the possible pbufs chain.
		// auto cpyflags=p->flags;
		auto cpylen=p->tot_len;
		H4AT_PRINT2("* p=%p * FREE DATA %p %d 0x%02x\n",p,p->payload,p->tot_len,p->flags);
		err=ERR_OK;
		// LwIPCoreLocker lock; // To ensure no data race between two threads, .
		// lock.unlock();
		dumphex(cpydata, cpylen);
		// rq->_handleFragment((const uint8_t*) cpydata,cpylen,cpyflags);

		free(cpydata);

    if (p) {
        altcp_recved(tpcb, p->tot_len);
		pbuf_free(p);
    }
    return err;
}

err_t _raw_sent(void* arg,struct altcp_pcb *tpcb, u16_t len){
    H4AT_PRINT2("_raw_sent %p pcb=%p len=%d\n",arg,tpcb,len);
    return ERR_OK;
}
void _tcp_write(struct altcp_pcb* pcb, uint8_t* data, uint16_t len){
    LwIPCoreLocker lock;
    uint8_t flags;
    size_t  sent=0;
    size_t  left=len;
    // dumphex(data,len);

    while(left){
        size_t available=altcp_sndbuf(pcb);
        auto qlen = altcp_sndqueuelen(pcb);
        if(available && qlen < TCP_SND_QUEUELEN){
            auto chunk=std::min(left,available);
            flags=TCP_WRITE_FLAG_COPY;
            if(left - chunk) flags |= TCP_WRITE_FLAG_MORE;
            H4AT_PRINT2("WRITE %p L=%d F=0x%02x LEFT=%d Q=%d\n",data+sent,chunk,flags,left,qlen);
            dumphex(data+sent, chunk);
            auto err = altcp_write(pcb, data+sent, chunk, flags);
            if(!err) {
                err=altcp_output(pcb);
                if(err) H4AT_PRINT1("ERR %d after output H=%u sb=%d Q=%d\n",err,_HAL_freeHeap(),altcp_sndbuf(pcb),altcp_sndqueuelen(pcb));
            }

            // lock.unlock();
            if (err) {
                H4AT_PRINT1("ERR %d after write H=%u sb=%d Q=%d\n", err, _HAL_freeHeap(), altcp_sndbuf(pcb), altcp_sndqueuelen(pcb));
            }
            else {
                sent+=chunk;
                left-=chunk;
            }
        } 
        else break;
    }
}

void dumphex(const uint8_t* mem, size_t len) {
    if(mem && len){ // no-crash sanity
        auto W=16;
        uint8_t* src;
        memcpy(&src,&mem,sizeof(uint8_t*));
        printf("Address: 0x%08X len: 0x%X (%d)", (ptrdiff_t)src, len, len);
        for(uint32_t i = 0; i < len; i++) {
            if(i % W == 0) printf("\n[0x%08X] 0x%08X %5u:  ", (ptrdiff_t)src, i,i);
            printf("%02X ", *src);
            src++;
            //
            if(i % W == W-1 || src==mem+len){
                size_t ff=W-((src-mem) % W);
                for(int p=0;p<(ff % W);p++) printf("   ");
                printf("  "); // stretch this for nice alignment of final fragment
                for(uint8_t* j=src-(W-(ff%W));j<src;j++) printf("%c", isprint(*j) ? *j:'.');
            }
        }
        printf("\n");
    }
}

uint32_t    _HAL_freeHeap(uint32_t caps){ return heap_caps_get_free_size(caps); }
uint32_t    _HAL_maxHeapBlock(uint32_t caps){ return heap_caps_get_largest_free_block(caps); }

// #ifdef __cplusplus
// }
// #endif