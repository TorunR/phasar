int foo(int i,int k,int j){
  int a = i *k;
  int b = i *j;
  int c = k *j;
  return a * b *c;
}
int main() {
  int A = foo(0, 1, 2);
  int B = foo(5, 3, 4);
  printf("%d\n", A);
  printf("%d\n", 8);
}