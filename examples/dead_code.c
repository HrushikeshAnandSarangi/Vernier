int process(int a, int b) {
  int x = a * 4;
  int y = b * 4;
  int z = x + y;

  if (a > 100 && a < 50) { // Impossible condition
    z += 9999;
    z *= 2;
  }

  // Redundant computations
  int unused = a * b * 42;
  (void)unused;

  return z;
}
