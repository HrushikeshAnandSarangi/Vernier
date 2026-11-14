#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

// 1. Matrix Multiplication (vectorization and loop unrolling candidate)
void matmul(const float *restrict A, const float *restrict B, float *restrict C, int N) {
  for (int i = 0; i < N; ++i) {
    for (int k = 0; k < N; ++k) {
      for (int j = 0; j < N; ++j) {
        C[i * N + j] += A[i * N + k] * B[k * N + j];
      }
    }
  }
}

// 2. Vector math with reduction
float dot_product(const float *restrict a, const float *restrict b, int n) {
  float sum = 0.0f;
  #pragma clang loop vectorize(enable)
  for (int i = 0; i < n; ++i) {
    sum += a[i] * b[i];
  }
  return sum;
}

// 3. Quicksort Partition & Recursion (inlining and stack frame candidate)
static inline void swap(int *a, int *b) {
  int t = *a;
  *a = *b;
  *b = t;
}

static int partition(int arr[], int low, int high) {
  int pivot = arr[high];
  int i = (low - 1);
  for (int j = low; j <= high - 1; j++) {
    if (arr[j] < pivot) {
      i++;
      swap(&arr[i], &arr[j]);
    }
  }
  swap(&arr[i + 1], &arr[high]);
  return (i + 1);
}

void quick_sort(int arr[], int low, int high) {
  if (low < high) {
    int pi = partition(arr, low, high);
    quick_sort(arr, low, pi - 1);
    quick_sort(arr, pi + 1, high);
  }
}

// 4. Hash Table & FNV-1a Hashing (memory loads/stores & bit manipulation)
uint32_t fnv1a_hash(const char *str, size_t len) {
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < len; ++i) {
    hash ^= (uint8_t)str[i];
    hash *= 16777619u;
  }
  return hash;
}

struct HashEntry {
  uint32_t key;
  int value;
  struct HashEntry *next;
};

int hashtable_lookup(struct HashEntry **table, size_t mask, uint32_t key) {
  size_t idx = key & mask;
  struct HashEntry *curr = table[idx];
  while (curr) {
    if (curr->key == key)
      return curr->value;
    curr = curr->next;
  }
  return -1;
}

// 5. Dead Code & Branch Folding
int complex_conditions(int a, int b, int c) {
  int res = 0;
  if (a > 10 && a < 5) { // Unreachable
    res += 1000;
  }
  if ((b | 0) == b) {
    res += b * 2;
  }
  if (c * 0 != 0) { // Never true
    res += 5000;
  }
  return res + (a << 3) - (a * 8); // Simplifies to res
}
