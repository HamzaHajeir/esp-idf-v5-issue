
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwipopts.h"
#include "lwip/tcpip.h"
#include "TCPCommon.h"

#define H4AT_HAS_RTOS 1
#define H4AS_RTOS_GET_THREAD_NAME   pcTaskGetName(NULL) //

volatile int LwIPCoreLocker::_locks;
#define PRINTAPPENDS "\t=====LOCKER=====\t"

LwIPCoreLocker::LwIPCoreLocker() {
#if H4AT_HAS_RTOS
	lock();
#endif
}

void LwIPCoreLocker::unlock()
{
#if H4AT_HAS_RTOS
	H4AT_PRINT4(PRINTAPPENDS"LwIPCoreLocker::unlock _locks=%d _locked=%d\n", _locks, _locked);
	if (strcmp(H4AS_RTOS_GET_THREAD_NAME, TCPIP_THREAD_NAME) == 0)
	{
		H4AT_PRINT4(PRINTAPPENDS"Don't UNLOCK from LWIP THREAD\n");
		return;
	}

	if (_locked){
		if (--_locks == 0)
			UNLOCK_TCPIP_CORE();
	}
	_locked=false;
#endif
}

LwIPCoreLocker::~LwIPCoreLocker()
{
#if H4AT_HAS_RTOS
	H4AT_PRINT4(PRINTAPPENDS"~LwIPCoreLocker\n");
	unlock();
#endif
}

void LwIPCoreLocker::lock()
{
#if H4AT_HAS_RTOS
	H4AT_PRINT4(PRINTAPPENDS"LwIPCoreLocker _locks=%d _locked=%d\n", _locks, _locked);
	if (_locked) {
		H4AT_PRINT4(PRINTAPPENDS "LwIPCoreLocker Already locked\n");
		return;
	}
    if (strcmp(H4AS_RTOS_GET_THREAD_NAME, TCPIP_THREAD_NAME) == 0)
	{
		H4AT_PRINT4(PRINTAPPENDS"Don't LOCK from LWIP THREAD\n");
		return;
	}
	if (!_locks++) { // The first lock
		LOCK_TCPIP_CORE();
	}
	_locked=true;
#endif
}
