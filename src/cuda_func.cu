#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define DEFAULT_NUM_THREAD 256

/* Case: double */
__global__ void in_perm_k (
  char *precomp_data,
  size_t *input_perm,
  char *input_data,
  char *buff_in,
  size_t interac_indx,
  size_t M_dim0,
  size_t vec_cnt )
{
  /* 1-dim thread Id. */
  int tid = blockIdx.x*blockDim.x + threadIdx.x;

  size_t s_pdata = ((size_t *) precomp_data)[input_perm[(interac_indx + tid)*4 + 0]/8];
  size_t s_pdata1 = ((size_t *) precomp_data)[input_perm[(interac_indx + tid)*4 + 0]/8 + 1];
  size_t s_pdata2 = ((size_t *) precomp_data)[input_perm[(interac_indx + tid)*4 + 0]/8 + 2];
  double d_pdata = ((double *) precomp_data)[input_perm[(interac_indx + tid)*4 + 1]/8];
  double d_pdata1 = ((double *) precomp_data)[input_perm[(interac_indx + tid)*4 + 1]/8 + 1];
  double d_pdata2 = ((double *) precomp_data)[input_perm[(interac_indx + tid)*4 + 1]/8 + 2];

  if (tid < vec_cnt) {
    /* Convert to ptr. */
    size_t *perm  = (size_t *) (precomp_data + input_perm[(interac_indx + tid)*4 + 0]);
    double *scal  = (double *) (precomp_data + input_perm[(interac_indx + tid)*4 + 1]);
    double *v_in  = (double *) (input_data   + input_perm[(interac_indx + tid)*4 + 3]);
    double *v_out = (double *) (buff_in      + input_perm[(interac_indx + tid)*4 + 2]);

    /* PRAM Model: assuming as many threads as we need. */
    for (size_t j = 0; j < M_dim0; j++) {
/*
      size_t perm_tmp = perm[j];
      double scal_tmp = scal[j];
      double v_in_tmp = v_in[perm_tmp];
      v_out[j] = 0.0;
      v_out[j] = v_in_tmp*scal_tmp;
*/
      v_out[j] = v_in[perm[j]]*scal[j];
    }
  }
}

__global__ void out_perm_k (
  double *scaling,
  char *precomp_data,
  size_t *output_perm,
  char *output_data,
  char *buff_out,
  size_t interac_indx,
  size_t M_dim1,
  size_t vec_cnt )
{
  /* 1-dim thread Id. */
  int tid = blockIdx.x*blockDim.x + threadIdx.x;

  /* Specifing range. */
  int a = (tid*vec_cnt)/vec_cnt;
  int b = ((tid + 1)*vec_cnt)/vec_cnt;

  if (tid > 0 && a < vec_cnt) { // Find 'a' independent of other threads.
    size_t out_ptr = output_perm[(interac_indx + a)*4 + 3];
    if (tid > 0) while (a < vec_cnt && out_ptr == output_perm[(interac_indx + a)*4 + 3]) a++;
  }
  if (tid < vec_cnt - 1 &&  - 1 && b < vec_cnt) { // Find 'b' independent of other threads.
    size_t out_ptr = output_perm[(interac_indx + b)*4 + 3];
    if (tid < vec_cnt - 1) while (b < vec_cnt && out_ptr == output_perm[(interac_indx+b)*4 + 3]) b++;
  }

  if (tid < vec_cnt) {
    /* PRAM Model: assuming as many threads as we need. */
    for(int i = a; i < b; i++) { // Compute permutations.
      double scaling_factor = scaling[interac_indx + i];
      size_t *perm = (size_t*) (precomp_data + output_perm[(interac_indx + i)*4 + 0]);
      double *scal = (double*) (precomp_data + output_perm[(interac_indx + i)*4 + 1]);
      double *v_in = (double*) (buff_out + output_perm[(interac_indx + i)*4 + 3]);
      double *v_out = (double*) (output_data + output_perm[(interac_indx + i)*4 + 2]);
      for (int j = 0; j < M_dim1; j++) v_out[j] += v_in[perm[j]]*scal[j]*scaling_factor;
    }
  }
}

extern "C" { 
void test_d (uintptr_t precomp_data, uintptr_t input_perm, uintptr_t input_data, uintptr_t buff_in, 
  int interac_indx, cudaStream_t *stream) {};

void in_perm_d (
/*
  uintptr_t precomp_data,
  uintptr_t input_perm,
  uintptr_t input_data,
  uintptr_t buff_in,
*/
  char *precomp_data,
  size_t *input_perm,
  char *input_data,
  char * buff_in,
  size_t interac_indx,
  size_t M_dim0,
  size_t vec_cnt, 
  cudaStream_t *stream )
{
  int n_thread, n_block;
  n_thread = DEFAULT_NUM_THREAD;
  n_block = vec_cnt/n_thread + 1;
/*
  char *precomp_data_d = (char *) precomp_data;
  char *input_perm_d = (char *) input_perm;
  char *input_data_d = (char *) input_data;
  char *buff_in_d = (char *) buff_in;
*/
  /*
  in_perm_k<<<n_thread, n_block, 0, *stream>>>(precomp_data, input_perm, input_data, buff_in, 
    interac_indx, M_dim0, vec_cnt);
*/
  printf("vec_cnt: %d, M_dim0: %d\n", (int) vec_cnt, (int) M_dim0);
  in_perm_k<<<n_block, n_thread>>>(precomp_data, input_perm, input_data, buff_in, 
    interac_indx, M_dim0, vec_cnt);
};

void out_perm_d (
  double *scaling,
  char *precomp_data,
  size_t *output_perm,
  char *output_data,
  char *buff_out,
  size_t interac_indx,
  size_t M_dim0,
  size_t vec_cnt,
  cudaStream_t *stream )
{
  int n_thread, n_block;
  n_thread = DEFAULT_NUM_THREAD;
  n_block = vec_cnt/n_thread + 1;
/*
  out_perm_k<<<n_block, n_thread, 0, *stream>>>(scaling, precomp_data, output_perm, output_data, buff_out, 
    interac_indx, M_dim0, vec_cnt);
*/
  out_perm_k<<<n_block, n_thread, 0, *stream>>>(scaling, precomp_data, output_perm, output_data, buff_out, 
    interac_indx, M_dim0, vec_cnt);
};

}
