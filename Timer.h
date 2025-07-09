#include <signal.h>
#include <time.h>

struct Timer{
    bool initialized;
    timer_t tid;
    Timer(void (*cb)(union sigval), void *arg);
    ~Timer();
    void set(int seconds);
};
