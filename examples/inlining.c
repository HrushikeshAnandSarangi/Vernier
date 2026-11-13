static inline int helper(int x) {
  return x * 3 + 1;
}

static inline int intermediate(int a, int b) {
  return helper(a) + helper(b);
}

int calculate(int x, int y) {
  return intermediate(x, y) + intermediate(y, x);
}
