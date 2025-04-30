
#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
typedef struct protoent PROTOENT;
typedef struct servent SERVENT;
typedef struct hostent HOSTENT;
// milliseconds are good enough, so resort to these where necessary
#define Sleep(a) usleep((a) * 1000)
#endif

#include <algorithm>

#include <errno.h>

#include "ConsoleListener.h"

// if USING_SELECT is defined, all incoming socket events are handled through select()
// if undefined, timeout loops are used (can be quite inefficient at times)
// TODO: remove the possibility once the select() method works reliably on all target systems
#define USING_SELECT


// ConsoleSocketThread class members

// ClientActivity : the handler for all incoming socket activity

void ConsoleSocketThread::ClientActivity() {

}
