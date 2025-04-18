#ifndef STENCIL_H
#define STENCIL_H

#include <stdint.h>
#include <stdio.h>

//Define all the test case dimensions
#define ABS_NI  4096

#define MATVEC_NI  1000
#define MATVEC_NJ  1000

#define TRANS_NI  10000
#define TRANS_NJ  10000

#define ST_N1  128
#define ST_S1    8

#define ST_N2   64
#define ST_S2   20

#define ST_NI3       4
#define ST_NJ3       4
#define ST_NK3 1048576
#define ST_S3        2

#define NUM_TESTS    6

typedef struct {
    float real, imag;
    float mag, angle, radius, phase; 
    double tolerance;
} Complex;

void init();

void compute_abs(int Ni, float Out[Ni]);

void abs_check(int Ni, float Out[Ni]);

void compute_transpose(int Ni, int Nj,
                    const char In[Ni][Nj], char Out[Nj][Ni]);

void transpose_check(int Ni, int Nj,
                    const char In[Ni][Nj], char Out[Nj][Ni]);

void compute_matvec(int Ni, int Nj, const Complex In[Nj], Complex Out[Ni]);

void matvec_check(int Ni, int Nj, const Complex In[Nj], Complex Out[Ni], Complex arr[Ni][Nj]);

void compute_stencil(int Ni, int Nj, int Nk, int S, 
            const float In[Ni+S][Nj+S][Nk+S], float Out[Ni][Nj][Nk], 
            const float Stencil[S][S][S]);

void stencil_check(int Ni, int Nj, int Nk, int S, 
            const float In[Ni+S][Nj+S][Nk+S], float Out[Ni][Nj][Nk], 
            const float Stencil[S][S][S]);

// Helper functions
void move_one_value(int i1, int j1, int i2, int j2, int Ni, int Nj, char In[Ni][Nj], char Out[Ni][Nj]);

void set_to_zero(int i, int j, int k, int Ni, int Nj, int Nk,float array[Ni][Nj][Nk]);

void macc_element(const float* In, float* Out, const float* stencil);

Complex complex_add(Complex a, Complex b);

Complex complex_multiply(Complex a, Complex b);

void reticulate_splines(do_all);


#endif
