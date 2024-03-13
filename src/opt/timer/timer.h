/* timer.h
 * Tracks and regulates real time.
 */
 
#ifndef TIMER_H
#define TIMER_H

double timer_now();
double timer_now_cpu();

struct timer {
  double prevtime;
  double interval;
  double min_update;
  double max_update;
  double starttime_real;
  double starttime_cpu;
  int vframec;
  int faultc;
  int longc;
};

/* (tolerance) is 0..1, how far is the result of timer_tick allowed to deviate from (rate_hz).
 * At zero, tick will return 1/rate_hz every time regardless of real time elapsed.
 * At one, it will never sleep.
 */
void timer_init(struct timer *timer,int rate_hz,double tolerance);

/* Sleep if necessary, to advance the clock by one video frame.
 * Returns the time elapsed, clamped to the tolerance you specified at init.
 */
double timer_tick(struct timer *timer);

/* Dump a one-line report to stderr, whatever detail we have since startup.
 */
void timer_report(struct timer *timer);

#endif
