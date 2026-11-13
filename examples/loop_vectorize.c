void vector_add(float *restrict a, const float *restrict b,
                const float *restrict c, int n) {
  #pragma clang loop vectorize(enable)
  for (int i = 0; i < n; ++i) {
    a[i] = b[i] * 2.5f + c[i];
  }
}
