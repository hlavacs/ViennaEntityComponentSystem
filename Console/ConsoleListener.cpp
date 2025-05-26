
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

    int waitrc;

    auto& s = getSocket();

    // TEST - start by sending a welcome string to the other side
    std::string welcome("{ \"cmd\":\"Hello\", \"version\":\"" __DATE__ " " __TIME__ "\" }");
    s.sendData(welcome);

    // wait for incoming data with a timeout of 500 ms
    while ((waitrc = s.wait(500)) != SOCKET_ERROR) {
        // check for timeout ...
        if (waitrc == 0) {
            // TODO : if there's an external influence governing this thread,
            //        make it happen here
        }
        else if (waitrc > 0) {
            // something HAPPENED!

            // if no data there, the socket connection has presumably been closed from this or the other side
            if (!s.dataThere())
                break;

            // FIRST TEST - simply read in as many data as available and echo them back
            int nLen;
            while ((nLen = s.bytesBuffered()) > 0) {
                char sbuf[4096];
                int maxRd = min(sizeof(sbuf), nLen);
                int rlen = s.receiveData(sbuf, maxRd);
                nLen -= rlen;
                s.sendData(sbuf, rlen);
            }

        }
    }
}
