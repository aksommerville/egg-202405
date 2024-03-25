#include "synth_internal.h"

/* All of these algorithms, I copied from the blue book without really understanding.
 * _The Scientist and Engineer's Guide to Digital Signal Processing_ by Steven W Smith.
 * Chebyshev low-pass filter, p 341.
 * Chebyshev high-pass filter, p 341.
 * Bandpass and notch, p 326, Equation 19-7.
 */

/* Low pass.
 */
 
void synth_filter_init_lopass(struct synth_filter *filter,float freq) {
  float rp=-cosf(M_PI/2.0f);
  float ip=sinf(M_PI/2.0f);
  float t=2.0f*tanf(0.5f);
  float w=2.0f*M_PI*freq;
  float m=rp*rp+ip*ip;
  float d=4.0f-4.0f*rp*t+m*t*t;
  float x0=(t*t)/d;
  float x1=(2.0f*t*t)/d;
  float x2=(t*t)/d;
  float y1=(8.0f-2.0f*m*t*t)/d;
  float y2=(-4.0f-4.0f*rp*t-m*t*t)/d;
  float k=sinf(0.5f-w/2.0f)/sinf(0.5f+w/2.0f);
   
  filter->coefv[0]=(x0-x1*k+x2*k*k)/d;
  filter->coefv[1]=(-2.0f*x0*k+x1+x1*k*k-2.0f*x2*k)/d;
  filter->coefv[2]=(x0*k*k-x1*k+x2)/d;
  filter->coefv[3]=(2.0f*k+y1+y1*k*k-2.0f*y2*k)/d;
  filter->coefv[4]=(-k*k-y1*k+y2)/d;
  
  filter->statev[0]=filter->statev[1]=filter->statev[2]=filter->statev[3]=filter->statev[4]=0.0f;
}

/* High pass.
 */
 
void synth_filter_init_hipass(struct synth_filter *filter,float freq) {
  float rp=-cosf(M_PI/2.0f);
  float ip=sinf(M_PI/2.0f);
  float t=2.0f*tanf(0.5f);
  float w=2.0f*M_PI*freq;
  float m=rp*rp+ip*ip;
  float d=4.0f-4.0f*rp*t+m*t*t;
  float x0=(t*t)/d;
  float x1=(2.0f*t*t)/d;
  float x2=(t*t)/d;
  float y1=(8.0f-2.0f*m*t*t)/d;
  float y2=(-4.0f-4.0f*rp*t-m*t*t)/d;
  float k=-cosf(w/2.0f+0.5f)/cosf(w/2.0f-0.5f);
   
  filter->coefv[0]=-(x0-x1*k+x2*k*k)/d;
  filter->coefv[1]=(-2.0f*x0*k+x1+x1*k*k-2.0f*x2*k)/d;
  filter->coefv[2]=(x0*k*k-x1*k+x2)/d;
  filter->coefv[3]=-(2.0f*k+y1+y1*k*k-2.0f*y2*k)/d;
  filter->coefv[4]=(-k*k-y1*k+y2)/d;
  
  filter->statev[0]=filter->statev[1]=filter->statev[2]=filter->statev[3]=filter->statev[4]=0.0f;
}

/* Band pass.
 */
 
void synth_filter_init_bandpass(struct synth_filter *filter,float freq,float width) {
  float r=1.0f-3.0f*width;
  float cosfreq=cosf(M_PI*2.0f*freq);
  float k=(1.0f-2.0f*r*cosfreq+r*r)/(2.0f-2.0f*cosfreq);
  
  filter->coefv[0]=1.0f-k;
  filter->coefv[1]=2.0f*(k-r)*cosfreq;
  filter->coefv[2]=r*r-k;
  filter->coefv[3]=2.0f*r*cosfreq;
  filter->coefv[4]=-r*r;
  
  filter->statev[0]=filter->statev[1]=filter->statev[2]=filter->statev[3]=filter->statev[4]=0.0f;
}

/* Band reject.
 */
 
void synth_filter_init_notch(struct synth_filter *filter,float freq,float width) {
  float r=1.0f-3.0f*width;
  float cosfreq=cosf(M_PI*2.0f*freq);
  float k=(1.0f-2.0f*r*cosfreq+r*r)/(2.0f-2.0f*cosfreq);
  
  filter->coefv[0]=k;
  filter->coefv[1]=-2.0f*k*cosfreq;
  filter->coefv[2]=k;
  filter->coefv[3]=2.0f*r*cosfreq;
  filter->coefv[4]=-r*r;
  
  filter->statev[0]=filter->statev[1]=filter->statev[2]=filter->statev[3]=filter->statev[4]=0.0f;
}
