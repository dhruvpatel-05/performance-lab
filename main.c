#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/mman.h>

#include "kernels.h"
#include "main.h"
#include "assert.h"

void *in, *out, *out_check;
char hostname[HOST_NAME_MAX];
char cache_profile=1;
char do_all=1;

enum kernel_name {
  KERNEL_ABS,
  KERNEL_MATVEC,
  KERNEL_TRANSPOSE,
  KERNEL_STENCIL
};

Complex compArr[MATVEC_NI][MATVEC_NJ];

#define PERFLINESZ (200)

// The following functions are helper functions.  You should really consider "inlining" them.
// I.e. you should copy these into your implementations.

// Transfer one value from the In to Out array
void move_one_value(int i1, int j1, int i2, int j2, int Ni, int Nj, char In[Ni][Nj], char Out[Ni][Nj]) {
  Out[i2][j2]=In[i1][j1];
}

// Sets one value of an array to zero
void set_to_zero(int i, int j, int k, int Ni, int Nj, int Nk,float array[Ni][Nj][Nk]) {
  array[i][j][k]=0;
}

// Multiply two numbers, and store the result in a third.  
void macc_element(const float* In, float* Out, const float* Stencil) {
  *Out += (*In) * (*Stencil);
}

// Function to add two complex numbers
Complex complex_add(Complex a, Complex b) {
    Complex result;
    result.real = a.real + b.real;
    result.imag = a.imag + b.imag;
    return result;
}

// Function to multiply two complex numbers
Complex complex_multiply(Complex a, Complex b) {
    Complex result;
    result.real = a.real * b.real - a.imag * b.imag;
    result.imag = a.real * b.imag + a.imag * b.real;
    return result;
}

static struct option long_options[] = {
//    {"direct-path",        no_argument,       nullptr, 'd',},
    {"input",         required_argument, 0, 'i'},
    {"trials",        required_argument, 0, 't'},
    {0, 0, 0, 0,},
};

// Calculate energy based on the number of L1, L2, L3 and Memory accesses
// This is not meant to be accurate, it's just a rough estimate...
float calc_mem_energy(long l1, long l2, long l3, long mem) {
  return  (l1*0.05+l2*0.1+l3*0.5+mem*10.0)/1000000000.0;
}

typedef struct {
  enum kernel_name kernel;
  int index;
  int Ni,Nj,Nk;
  int S;
  float orig_msec, orig_mem;
  float best_msec, best_mem;
} TestParams;

// Set all values in an array to zero
void clear3d(int Ni, int Nj, int Nk, float a[Ni][Nj][Nk]) {
  for(int i = 0; i < Ni; ++i) {
    for(int j = 0; j < Nj; ++j) {
      for(int k = 0; k < Nk; ++k) {
        a[i][j][k] = 0;
      }
    }
  }
}

// Generate a random 3d array
void gen_3d(int Ni, int Nj, int Nk, float a[Ni][Nj][Nk],float normalize) {
  for(int i = 0; i < Ni; ++i) {
    for(int j = 0; j < Nj; ++j) {
      for(int k = 0; k < Nk; ++k) {
        int val = rand();
        a[i][j][k] = (((float)rand())/RAND_MAX - 1.0/2.0) * normalize;
      }
    }
  }
}

static int max_errors_to_print = 5;

//Check the character array "a[][]" vs "a_check[][]" to see if they are equal
char check_2d(int Ni, int Nj, 
              char a[Ni][Nj], char a_check[Ni][Nj], char in[Ni][Nj]) {

  int errors_printed=0;
  char has_errors = 0;
  for(int i = 0; i < Ni; ++i) {
    for(int j = 0; j < Nj; ++j) {
      if( (a[i][j] != a_check[i][j]) ) { 
        has_errors = 1;
        if(errors_printed < max_errors_to_print) {
          if(errors_printed==0) printf("\n");
          printf("Error on index: [%d][%d].",i,j);
          printf("Your output: %01X, Correct output %01X\n", 
              (unsigned char)a[i][j], (unsigned char)a_check[i][j]);
          errors_printed++;
        } else {
          //printed too many errors already, just stop
          if(max_errors_to_print !=0) {
            printf("and many more errors likely exist...\n");
          }
          return 1;
        }
      }
    }
  }
  return has_errors;
}

char different(float a, float b) {
    if( (a < (b - 0.005)) ||
        (a > (b + 0.005)) ) {
      return 1;
    }
    return 0;
}

//Check the float array "a[][][]" vs "a_check[][][]" to see if they are similar enough
char check_3d(int Ni, int Nj, int Nk,
              float a[Ni][Nj][Nk], float a_check[Ni][Nj][Nk]) {
  int errors_printed=0;
  char has_errors = 0;
  for(int i = 0; i < Ni; ++i) {
    for(int j = 0; j < Nj; ++j) {
      for(int k = 0; k < Nk; ++k) {
        if(different(a[i][j][k], a_check[i][j][k])) {
          has_errors = 1;
          if(errors_printed < max_errors_to_print) {
            if(errors_printed==0) printf("\n");
            printf("Error on index: [%d][%d][%d].",i,j,k);
            printf("Your output: %f, Correct output %f\n",a[i][j][k], a_check[i][j][k]);
            errors_printed++;
          } else {
            //printed too many errors already, just stop
            if(max_errors_to_print !=0) {
              printf("and many more errors likely exist...\n");
            }
            return 1;
          }
        }
      }
    }
  }
  return has_errors;
}

//Check the float array "a[][][]" vs "a_check[][][]" to see if they are similar enough
char check_complex(int Ni, Complex a[Ni], Complex a_check[Ni]) {
  int errors_printed=0;
  char has_errors = 0;
  for(int i = 0; i < Ni; ++i) {
    if(different(a[i].real,a_check[i].real) || different(a[i].imag,a_check[i].imag)) {
        has_errors = 1;
        if(errors_printed < max_errors_to_print) {
          if(errors_printed==0) printf("\n");
          printf("Error on index: [%d].",i);
          printf("Your output: %f %f, Correct output %f %f\n",
                  a[i].real, a_check[i].real, a[i].imag, a_check[i].imag);
          errors_printed++;
        } else {
          //printed too many errors already, just stop
          if(max_errors_to_print !=0) {
            printf("and many more errors likely exist...\n");
          }
          return 1;
        }
      }
  }
  return has_errors;
}

// Grab the first number out of a string ... this is for parsing linux "perf" results
long first_num(char* p) {
  char str[128] = {0};
  
  int found=0;
  char* s=str;
  for(int i = 0; i <1000; i++){
    char c = *p++;
    if(c==0) break;
    if(c==',') continue;
    if(c==' ' || c=='\t') {
      if(found) break;
      else continue;
    }
    found=1;
    *s++=c;
  }

  return strtol(str, &str, 10);
}

// Return the minimum of two numbers
long min(long l1, long l2) {
  if(l1 < l2) return l1;
  else return l2;
}

// Run one of our test cases one time.
// check_func: should we check correctness?
// is_broken: return value indicating if the program is incorrect
float run(TestParams* p, char check_func, char* is_broken) {
  uint64_t start_time, total_time=0;

  int Ni = p->Ni, Nj = p->Nj, Nk=p->Nk;
  int S = p->S;

  // Any special kernel-specific setup
  void* stencil;
  if(p->kernel==KERNEL_ABS) {
     // copy the Input array to the output array
     for(int i = 0; i < Nj*Ni; ++i) {
       float* outf = (float*) out;
       float* inf = (float*) in;
       outf[i]=inf[i];
     }
  } else if(p->kernel==KERNEL_STENCIL) {
    stencil = malloc(sizeof(float) * S * S * S);
    // Generate the inputs for stencil
    gen_3d(S,S,S,stencil,1.0/((float)S*S*S));
  }


  // Compute the outputs that can be checked later
  if(check_func) {
    if(p->kernel==KERNEL_ABS) {
      for(int i = 0; i < Ni*Nj; ++i) {
        float* out_checkf = (float*) out_check;
        float* inf = (float*) in;
        out_checkf[i]=inf[i];
      }
      abs_check(Ni*Nj,out_check);
    } else if(p->kernel==KERNEL_MATVEC) {
      for(int trial = 0; trial < Nk; ++trial) {
        matvec_check(Ni,Nj,in+Ni*trial*sizeof(float),
                    out_check+Ni*trial*sizeof(float),compArr);
      }
    } else if(p->kernel==KERNEL_TRANSPOSE) {
      transpose_check(Ni,Nj,in,out_check);
    } else if(p->kernel==KERNEL_STENCIL) {
      stencil_check(Ni,Nj,Nk,S,in,out_check,stencil);
    } else {
      assert(0 && "bad kernel idx");
    }
  }

  if(p->kernel==KERNEL_ABS) {
    float sum=0;
    for(int trial = 0; trial < Ni; ++trial) {
       for(int i = 0; i < Nj; ++i) {
         float* outf = (float*) out;
         sum += outf[i+Nj*trial];
       } 

       start_time = read_usec();
       compute_abs(Nj,out+Nj*trial*sizeof(float));
       total_time += read_usec() - start_time;
    }
    if(sum==0) printf("error\n");
  } else if(p->kernel==KERNEL_MATVEC) {
    start_time = read_usec();
    for(int trial = 0; trial < Nk; ++trial) {
      compute_matvec(Ni,Nj,in+Ni*trial*sizeof(float),
                     out+Ni*trial*sizeof(float) );
    }
    total_time = read_usec() - start_time;
  } else if(p->kernel==KERNEL_TRANSPOSE) {
    start_time = read_usec();
    compute_transpose(Ni,Nj,in,out);
    total_time = read_usec() - start_time;
  } else if(p->kernel==KERNEL_STENCIL) {
    start_time = read_usec();
    compute_stencil(Ni,Nj,Nk,S,in,out,stencil);
    total_time = read_usec() - start_time;
  }

  if(check_func) {
    if(p->kernel==KERNEL_TRANSPOSE) {
      if(check_2d(Ni,Nj,out,out_check,in)) {
        *is_broken=1;
      }
    } else if(p->kernel==KERNEL_MATVEC) {
       for(int trial = 0; trial < Nk; ++trial) {
         if(check_complex(Ni,out+Ni*trial*sizeof(float) 
               ,out_check+Ni*trial*sizeof(float) )) {
           *is_broken=1;
           printf("failed on trial %d\n", trial);
           break;
         } 
       }
    } else {
      if(check_3d(Ni,Nj,Nk,out,out_check)) {
        *is_broken=1;
      } 
    }
  }

  double total_msec = total_time / 1000.0;
  float speedup = p->orig_msec / total_msec ;
  p->best_msec = fmin(p->best_msec,total_msec);

  double computations = ((double)Ni)*Nj*Nk*S*S*S;

  float ghz=2.0;
  double clocks = total_time*1000.0 * ghz;

  if(p->kernel==KERNEL_STENCIL) {
      free(stencil);
  } 

  // Now run the plain binary using system calls and parse the standard out
  long insts=0, vec_insts=0, l1=0,l2=0,l3=0,mem=0;
  FILE *fp;
  char cmd[240];

  // Check if we are on the right machine before running perf, otherwise it
  // will just crash and make us look bad!
  if(cache_profile) {

    // This command runs linux "perf." for cache profiling.
    // You can just type this command on your command line if you
    // want to see more detailed stats... but it's not necessary hopefully.
    sprintf(cmd,"perf stat -e instructions,simd_fp_256.packed_single,L1-dcache-loads,l2_rqsts.all_demand_data_rd,l2_rqsts.all_rfo,l2_rqsts.all_pf,LLC-loads,LLC-stores,LLC-prefetches ./plain %d 2>&1",p->index);
    
    if ((fp = popen(cmd, "r")) == NULL) {
        printf("Could not execute: \"%s\", did it get compiled?\n",cmd);
        exit(-1);
    }

    // This is some really poorly written code to parse some random things from the output of perf
    char buf[PERFLINESZ];
    fgets(buf, PERFLINESZ, fp); //discard three lines lazily
    fgets(buf, PERFLINESZ, fp);
    fgets(buf, PERFLINESZ, fp);

    fgets(buf, PERFLINESZ, fp); insts=first_num(buf);

    fgets(buf, PERFLINESZ, fp); vec_insts=first_num(buf);

    fgets(buf, PERFLINESZ, fp); l1=first_num(buf);

    fgets(buf, PERFLINESZ, fp); l2+=first_num(buf);
    fgets(buf, PERFLINESZ, fp); l2+=first_num(buf);
    fgets(buf, PERFLINESZ, fp); l2+=first_num(buf);

    fgets(buf, PERFLINESZ, fp); l3+=first_num(buf);
    fgets(buf, PERFLINESZ, fp); l3+=first_num(buf);
    fgets(buf, PERFLINESZ, fp); l3+=first_num(buf);

    fgets(buf, PERFLINESZ, fp); mem+=first_num(buf);
    fgets(buf, PERFLINESZ, fp); mem+=first_num(buf);
    fgets(buf, PERFLINESZ, fp); mem+=first_num(buf);
  } 

  float mem_energy=calc_mem_energy(l1,l2,l3,mem);

  if(p->kernel==KERNEL_ABS) {
    mem_energy=0.0004; // just override
  }


  p->best_mem = fmin(p->best_mem,mem_energy);
  float energyup=p->orig_mem/mem_energy;

  printf(" | %10.3f %6.2f %8.3f %10.3f | %7.1f %7.1f %7.1f %7.3f %8.6f | %7.1f %8.1f\n",
          total_msec, clocks/computations, insts/computations, vec_insts/computations, 
          l1/computations*1000, l2/computations*1000, l3/computations*1000, mem/computations*1000, mem_energy,
          speedup, energyup);

  if(cache_profile) {
    if (pclose(fp)) {
      printf("\"%s\" exited with nonzero error status\n",cmd);
      exit(-1);
    }
  }

  return total_time;
}

// This contains the parameters for each testcase, as well as the 
// measured results on our grading server -- not exactly the same
// as lnxsrv07, but close enough.
TestParams Tests[NUM_TESTS] = {
   {KERNEL_ABS,      0,     10000,    ABS_NI,       1,      1,      8.74, 0.000400, 10000000.00,10000000.00},
   {KERNEL_TRANSPOSE,1,  TRANS_NI,  TRANS_NJ,       1,      1,      600, 0.09,    10000000.00,10000000.00},
   {KERNEL_MATVEC,   2, MATVEC_NI, MATVEC_NJ,       8,      1,      431, 0.014,  10000000.00,10000000.00},
   {KERNEL_STENCIL,  3,     ST_N1,     ST_N1,   ST_N1,  ST_S1,     4250, 0.34,    10000000.00,10000000.00}, 
   {KERNEL_STENCIL,  4,     ST_N2,     ST_N2,   ST_N2,  ST_S2,     8000, 1.27,    10000000.00,10000000.00}, 
   {KERNEL_STENCIL,  5,    ST_NI3,    ST_NJ3,  ST_NK3,  ST_S3,      830, 0.14,    10000000.00,10000000.00} };

// Run the test for one test case
char run_test(int i, char check_func) {
  if(i<0 || i >= NUM_TESTS) {
    printf("Bad Test Case: %d, exiting\n",i);
    exit(0);
  }
  char is_broken=0;

  if(Tests[i].kernel==KERNEL_ABS) {
     printf("%2d %7s | %7d %5s %7s %2s",
          i,"Abs",Tests[i].Nj,"-", "-", "-");
  } else if(Tests[i].kernel==KERNEL_MATVEC) {
     printf("%2d %7s | %7d %5d %7s %2s",
          i,"MatVec.",Tests[i].Ni,Tests[i].Nj, "-", "-");
  } else if(Tests[i].kernel==KERNEL_TRANSPOSE) {
     printf("%2d %7s | %7d %5d %7s %2s",
          i,"Transp.",Tests[i].Ni,Tests[i].Nj, "-", "-");
  } else if(Tests[i].kernel==KERNEL_STENCIL) {
     printf("%2d %7s | %7d %5d %7d %2d",
          i,"Stencil",Tests[i].Ni,Tests[i].Nj,Tests[i].Nk, Tests[i].S);
  } else {
    assert(0);
  }

  fflush(stdout);
  run(&(Tests[i]),check_func,&is_broken);
  return is_broken;
}

// One leg of a piecewise linear interpolation.
// Here, s is between l and h
// and we linearly interpolate between lgrade and hgrade
float interp(float s, float l, float lgrade, 
                      float h, float hgrade) {
  return (s - l) * (hgrade - lgrade) / (h -l) + lgrade;
}

// This is the grading curve.  It's not really based on
// anything. I hope I didn't make this too easy or too hard.
// I guess we'll just experiment on you this year and see
// what happens.
float grade(float s) {
  int f_cutoff = 1;
  int d_cutoff = 10;
  int c_cutoff = 20;
  int b_cutoff = 25;
  int a_cutoff = 30;
  int x_cutoff = 110;

  if(s<f_cutoff)   return 0;
  if(s<d_cutoff)  return interp(s,  f_cutoff,   0, d_cutoff,  60);
  if(s<c_cutoff)  return interp(s,  d_cutoff,  60, c_cutoff,  80);
  if(s<b_cutoff)  return interp(s,  c_cutoff,  80, b_cutoff,  90);
  if(s<a_cutoff)  return interp(s,  b_cutoff,  90, a_cutoff, 100);
  if(s<x_cutoff)  return interp(s, a_cutoff, 100, x_cutoff, 115);
  return 115;
}

int main(int argc, char** argv) {
  char check_func=1; //Should we check correctness (yes)
  int test_case=0;   //Which test to run
  int num_trials=1;  //number of trials to run

  //Check the hostname
  int result = gethostname(hostname, HOST_NAME_MAX);
  if (result) {
    perror("gethostname");
    return EXIT_FAILURE;
  }

  //Parse Inputs
  int opt;         
  while ((opt = getopt_long(argc, argv, "i:t:", long_options, 0)) != -1) {
    switch (opt) {
      case 'i':
        if(*optarg == 'a') {
          do_all=1;
        } else if(*optarg == 'g') { 
          do_all=2;
        } else {
          do_all=0;
          test_case=atoi(optarg);
        }
        break;
      case 't': 
        num_trials=atoi(optarg);
    }
  }

  reticulate_splines(do_all);

  if(strcmp(hostname,"lnxsrv07.seas.ucla.edu")!=0 && do_all!=2) {
    printf("Since you're not running on lnxsrv07, we are turning off");
    printf("cache profiling, since it won't work on other machines.\n");
    cache_profile=0;
  }

  printf("\n");


  //First generate the input for matvec
  gen_3d(MATVEC_NI,MATVEC_NJ,8,compArr,2);
  mprotect(compArr,sizeof(float)*MATVEC_NI*MATVEC_NJ*8,PROT_READ);

  init();

  //setup the input array and generate some random values  
  srand (time(NULL));
  int max=0;
  
  if(do_all) {
    for(int i = 0; i < NUM_TESTS; ++i) {
      int S = Tests[i].S;
      int total=0;
      total= (Tests[i].Ni+S)*(Tests[i].Nj+S)*(Tests[i].Nk+S);
      if(total>max) {
        max=total;
      }
    }
  } else {
    int i = test_case;
    int S = Tests[i].S;
    max = (Tests[i].Ni+S)*(Tests[i].Nj+S)*(Tests[i].Nk+S);
  }


  max=(max+4095)>>12<<12;
  in = aligned_alloc(4096,sizeof(float) * max); 
  out = aligned_alloc(4096,sizeof(float) * max); 
  out_check = aligned_alloc(4096,sizeof(float) * max); 

  gen_3d(max,1,1,in,10);
  mprotect(in,sizeof(float)*max,PROT_READ);

  //---------------------------------------------------------

  int benchmarks_failed = 0;

  printf("%2s %7s | %7s %5s %7s %2s | %10s %6s %8s %10s | %7s %7s %7s %7s %8s | %7s %8s\n",
          "T#", "Kernel",
          "Ni","Nj","Nk", "S", 
          "Time(ms)", "CPE", "#Ins PE", "#VecInsPE", 
          "L1 PKE", "L2 PKE", "L3 PKE", "MEM PKE", "MemEn(j)",
          "Speedup", "Energyup");


  if(do_all) {
    for(int t = 0; t < num_trials; ++t) {
      if(t!=0) printf("\n");
      for(int i = 0; i < NUM_TESTS; i++) {
        benchmarks_failed+=run_test(i,check_func);
      }
    }

    if(!cache_profile) {
      printf("Quitting before grade computation, as you're not on lnxsrv07\n");
      exit(0);
    }


    float gmean_speedup=1;
    float gmean_energyup=1;
    for(int i = 0; i < NUM_TESTS; i++) {
      double speedup = ((double)Tests[i].orig_msec / (double)Tests[i].best_msec);
      gmean_speedup *= speedup;
    }
    for(int i = 0; i < NUM_TESTS; i++) {
      double enup = ((double)Tests[i].orig_mem / (double)Tests[i].best_mem);
      gmean_energyup *= enup;
    }

    gmean_speedup = pow(gmean_speedup,1.0/NUM_TESTS);
    gmean_energyup = pow(gmean_energyup,1.0/NUM_TESTS);

    printf("Geomean Speedup: %0.2f\n", gmean_speedup);
    printf("Geomean Energyup: %0.2f\n", gmean_energyup);

    float ed=gmean_speedup*gmean_energyup;
    printf("Energy-Delay Improvement: %0.2f\n", ed);


    if(benchmarks_failed) {
      printf("Number of Benchmarks FAILED: %d\n",benchmarks_failed);
      printf("No grade given, because of incorrect execution.\n");
    } else {
      printf("Grade: %0.1f\n",grade(ed));
      if(do_all==1) {
        printf("The above grade is an *estimate* only, you must submit your grade");
        printf("by running \"make submit\" to get your real grade and cache stats.\n");
        printf("\"make submit\" will also update the scoreboard.\n");
      }
    }
  } else {
    for(int t = 0; t < num_trials; ++t) {
      run_test(test_case,check_func);
    }
    double speedup = ((double)Tests[test_case].orig_msec / (double)Tests[test_case].best_msec);
    printf("Speedup: %0.2f\n\n", speedup);

    double energyup = ((double)Tests[test_case].orig_mem / (double)Tests[test_case].best_mem);
    printf("Energyup: %0.2f\n\n", energyup);

    printf("Energy-Delay Improvement: %0.2f\n", speedup*energyup);

    printf("No grade given, because only one test is run.\n");
    printf(" ... To see your grade, run all tests with \"-i a\" (or just no params)\n");
  }
}

