# performance-lab
CS33 Project

For this lab, I optimized 4 kernels (computational algorithms):

  1. Absolute Value (abs)
  Computes the absolute values of a float array.
  Optimization goal: Minimize branch mispredictions and improve vectorization.
  
  2. Transpose (transpose)
  Performs a 2D matrix transpose.
  Optimization goal: Improve spatial and temporal locality to reduce cache misses.
  
  3. Complex Matrix-Vector Multiply (cmatvecmul)
  Performs a matrix-vector multiply using complex numbers.
  Optimization goal: Reduce instruction count and improve memory access efficiency.
  
  4. 3D Stencil (stencil)
  Computes a 3D 7-point stencil operation.
  Optimization goal: Improve cache reuse and reduce memory bandwidth pressure.

Some of the optimization techniques used include loop unrolling, loop reordering, and cache tiling.
