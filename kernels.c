#include "kernels.h"

// Global Matrix for compute_matvec
extern Complex compArr[MATVEC_NI][MATVEC_NJ];

void init() {
// This function executes once before kernels are executed.  It is not timed.
}

// Ni -- Dimension of the Output Matrix
void compute_abs(int Ni, float Out[Ni]) {
    for(int i = 0; i < Ni; ++i) {
        if(Out[i] <= 0) {
            Out[i] = -Out[i];
        } 
    }
}

// Ni,Nj -- Dimensions of the In/Out matricies
// Note that compArr is a global array
void compute_matvec(int Ni, int Nj, const Complex In[Nj], Complex Out[Ni]) {
    for (int i = 0; i < Ni; i++) {
        Out[i].real = 0.0f;
        Out[i].imag = 0.0f;
        for (int j = 0; j < Nj; j++) {
            Complex product = complex_multiply(compArr[i][j], In[j]);
            Out[i] = complex_add(Out[i], product);
        }
    }
}

// Ni,Nj -- Dimensions of the In/Out matricies
void compute_transpose(int Ni, int Nj,
                    const char In[Ni][Nj], char Out[Nj][Ni]) {
  for(int j = 0; j < Nj; ++j) {
    for(int i = 0; i < Ni; ++i) {
      move_one_value(i,j,j,i,Ni,Nj,In,Out);
    }
  }
}


// Ni,Nj,Nk -- Dimensions of the output matrix
// S -- width/length/height of the stencil
void compute_stencil(int Ni, int Nj, int Nk, int S, 
            const float In[Ni+S][Nj+S][Nk+S], float Out[Ni][Nj][Nk], 
            const float Stencil[S][S][S]) {
  for(int k = 0; k < Nk; ++k) {
    for(int j = 0; j < Nj; ++j) { 
      for(int i = 0; i < Ni; ++i) { 
        set_to_zero(i,j,k,Ni,Nj,Nk,Out);
        for(int z = 0; z < S; ++z) {
          for(int y = 0; y < S; ++y) {
            for(int x = 0; x < S; ++x) {
              macc_element(&In[i+x][j+y][k+z],&Out[i][j][k],&Stencil[x][y][z]);
        } } }
  } } }
}




