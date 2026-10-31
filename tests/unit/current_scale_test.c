#include "current_scale.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL: line %d\n",__LINE__);return EXIT_FAILURE; } } while (0)
int main(void)
{
 float out=1;
 CHECK(current_scale_from_adc(0,&out) && out==0);
 CHECK(current_scale_from_adc(4095,&out) && fabsf(out-10)<1e-6f);
 CHECK(current_scale_from_adc(2047.5f,&out) && fabsf(out-5)<1e-6f);
 CHECK(current_scale_from_adc(1023.75f,&out) && fabsf(out-2.5f)<1e-6f);
 CHECK(current_scale_from_adc(3276,&out) && fabsf(out-8)<1e-6f);
 CHECK(!current_scale_from_adc(-1,&out) && isnan(out));
 CHECK(!current_scale_from_adc(4096,&out) && isnan(out));
 CHECK(!current_scale_from_adc(NAN,&out) && isnan(out));
 CHECK(!current_scale_from_adc(INFINITY,&out) && isnan(out));
 CHECK(!current_scale_from_adc(1,NULL));
 puts("PASS: all 10 current proxy scaling checks passed");
 return EXIT_SUCCESS;
}
