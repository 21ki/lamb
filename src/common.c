
#include <time.h>
#include <sys/time.h>

void cmpp_sleep(unsigned long long milliseconds) {
    struct timeval timeout;

    timeout.tv_sec = milliseconds / 1000;
    timeout.tv_usec = (milliseconds % 1000) * 1000000;
    select(0, NULL, NULL, NULL, &timeout);

    return;
}
