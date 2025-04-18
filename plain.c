#include <stdio.h>
#include <stdlib.h>

#include "kernels.h"

// Intentionally Uninitialized Arrays
float in1[ABS_NI];
float in2[TRANS_NI][TRANS_NJ];
float in3[MATVEC_NI][MATVEC_NJ];
float in4[ST_N1+ST_S1][ST_N1+ST_S1][ST_N1+ST_S1];
float in5[ST_N2+ST_S2][ST_N2+ST_S2][ST_N2+ST_S2];
float in6[ST_NI3+ST_S3][ST_NJ3+ST_S3][ST_NK3+ST_S3];

float stencil1[ST_S1][ST_S1][ST_S1];
float stencil2[ST_S2][ST_S2][ST_S2];
float stencil3[ST_S3][ST_S3][ST_S3];

float out1[ABS_NI];
float out2[TRANS_NI][TRANS_NJ];
float out3[MATVEC_NI][MATVEC_NJ];
float out4[ST_N1+ST_S1][ST_N1+ST_S1][ST_N1+ST_S1];
float out5[ST_N2+ST_S2][ST_N2+ST_S2][ST_N2+ST_S2];
float out6[ST_NI3+ST_S3][ST_NJ3+ST_S3][ST_NK3+ST_S3];

Complex compArr[MATVEC_NI][MATVEC_NJ];

void move_one_value(int i1, int j1, int i2, int j2, int Ni, int Nj, char In[Ni][Nj], char Out[Ni][Nj]) {
  Out[i2][j2]=In[i1][j1];
}

void set_to_zero(int i, int j, int k, int Ni, int Nj, int Nk,float array[Ni][Nj][Nk]) {
  array[i][j][k]=0;
}

void macc_element(const float* In, float* Out, const float* Stencil) {
  *Out += (*In) * (*Stencil);
}

Complex complex_add(Complex a, Complex b) {
    Complex result;
    result.real = a.real + b.real;
    result.imag = a.imag + b.imag;
    return result;
}

Complex complex_multiply(Complex a, Complex b) {
    Complex result;
    result.real = a.real * b.real - a.imag * b.imag;
    result.imag = a.real * b.imag + a.imag * b.real;
    return result;
}

int main(int argc, char** argv) {
  char cache_run=0;
  int test_case=1;
  int opt;

  test_case=atoi(argv[1]);

  switch(test_case) {
    case 0: 
      for(int i = 0; i < 10000; ++i) {
        compute_abs(ABS_NI,out1);
      }
      break;
    case 1: compute_transpose(TRANS_NI,TRANS_NJ,in2,out2); break;
    case 2: 
            for(int i = 0; i < 8; ++i) {
              compute_matvec(MATVEC_NI,MATVEC_NJ,in3,out3); 
            }
            break;
    case 3: compute_stencil(ST_N1,ST_N1,ST_N1,ST_S1,in4,out4,stencil1); break;
    case 4: compute_stencil(ST_N2,ST_N2,ST_N2,ST_S2,in5,out5,stencil2); break;
    case 5: compute_stencil(ST_NI3,ST_NJ3,ST_NK3,ST_S3,in6,out6,stencil3); break;
    default: break;
  }

  return 0;
}

