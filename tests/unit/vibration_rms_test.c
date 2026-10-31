#include "vibration_rms.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL: line %d\n",__LINE__);return EXIT_FAILURE; } } while (0)
int main(void)
{
 float out=1;
 const float still[4][3]={{0,0,1},{0,0,1},{0,0,1},{0,0,1}};
 CHECK(vibration_rms_compute(still,4,&out) && out==0);
 const float tilted[4][3]={{0.6f,0,0.8f},{0.6f,0,0.8f},{0.6f,0,0.8f},{0.6f,0,0.8f}};
 CHECK(vibration_rms_compute(tilted,4,&out) && out==0);
 const float moving[4][3]={{0.1f,0,1},{-0.1f,0,1},{0.1f,0,1},{-0.1f,0,1}};
 CHECK(vibration_rms_compute(moving,4,&out) && fabsf(out-0.1f)<1e-6f);
 const float multi[2][3]={{0.3f,0.4f,1},{-0.3f,-0.4f,1}};
 CHECK(vibration_rms_compute(multi,2,&out) && fabsf(out-0.5f)<1e-6f);
 float wave[100][3];
 for (unsigned i=0;i<100;++i) {wave[i][0]=0.2f*sinf(6.28318530718f*(float)i/100);wave[i][1]=0;wave[i][2]=1;}
 CHECK(vibration_rms_compute(wave,100,&out) && fabsf(out-0.141421356f)<1e-5f);
 CHECK(!vibration_rms_compute(NULL,4,&out) && isnan(out));
 CHECK(!vibration_rms_compute(still,1,&out) && isnan(out));
 CHECK(!vibration_rms_compute(still,4,NULL));
 const float bad[2][3]={{NAN,0,1},{0,0,1}};
 CHECK(!vibration_rms_compute(bad,2,&out) && isnan(out));
 const float inf[2][3]={{0,INFINITY,1},{0,0,1}};
 CHECK(!vibration_rms_compute(inf,2,&out) && isnan(out));
 puts("PASS: all 10 vibration RMS checks passed");
 return EXIT_SUCCESS;
}
