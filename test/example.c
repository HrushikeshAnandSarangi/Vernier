int sum(const int *a, int n){
  int t=0;
  for(int i=0;i<n;++i){
    t+=a[i];
  }
  return t;
}
