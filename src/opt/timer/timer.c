#include "timer.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

/* Primitives.
 */

double timer_now() {
  struct timespec tv={0};
  clock_gettime(CLOCK_REALTIME,&tv);
  return (double)tv.tv_sec+(double)tv.tv_nsec/1000000000.0;
}

double timer_now_cpu() {
  struct timespec tv={0};
  clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&tv);
  return (double)tv.tv_sec+(double)tv.tv_nsec/1000000000.0;
}

/* Init.
 */
 
void timer_init(struct timer *timer,int rate_hz,double tolerance) {
  if ((rate_hz<1)||(rate_hz>300)) rate_hz=60;
  memset(timer,0,sizeof(struct timer));
  timer->interval=1.0/(double)rate_hz;
  if (tolerance<=0.0) {
    timer->min_update=timer->interval;
    timer->max_update=timer->interval;
  } else if (tolerance>=1.0) {
    timer->min_update=0.0;
    timer->max_update=timer->interval*2.0;
  } else {
    tolerance*=timer->interval;
    timer->min_update=timer->interval-tolerance;
    timer->max_update=timer->interval+tolerance;
  }
  timer->starttime_real=timer_now();
  timer->starttime_cpu=timer_now_cpu();
  timer->prevtime=timer->starttime_real-timer->interval;
}

/* Update.
 */
 
double timer_tick(struct timer *timer) {
  timer->vframec++;
  double now=timer_now();
  double elapsed=now-timer->prevtime;
  if (elapsed<=0.0) {
    // Clock is broken. Reset hard without sleeping.
    timer->faultc++;
    timer->prevtime=now;
    return timer->min_update;
  }
  while (elapsed<timer->min_update) {
    usleep((timer->interval-elapsed)*1000000.0);
    now=timer_now();
    elapsed=now-timer->prevtime;
  }
  timer->prevtime=now;
  if (elapsed>timer->max_update) {
    // Too long between updates. This is normal from time to time.
    timer->longc++;
    return timer->max_update;
  }
  return elapsed;
}

/* Report.
 */

void timer_report(struct timer *timer) {
  if (timer->vframec>0) {
    double endtime_real=timer_now();
    double endtime_cpu=timer_now_cpu();
    double elapsed_real=endtime_real-timer->starttime_real;
    double elapsed_cpu=endtime_cpu-timer->starttime_cpu;
    double framerate=(double)timer->vframec/elapsed_real;
    double cpuload=elapsed_cpu/elapsed_real;
    fprintf(stderr,
      "%d frames in %.03f s, average rate %.03f Hz, CPU load %.03f, faultc=%d, longc=%d\n",
      timer->vframec,elapsed_real,framerate,cpuload,timer->faultc,timer->longc
    );
  }
}
