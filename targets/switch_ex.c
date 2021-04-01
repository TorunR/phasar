
void foo() {}
void foo2() {}
void foo3() {}

int main(int argc, char **argv) {
  switch (argc) {
  case 0:
  case 1:
    foo();
    break;
  case 2:
    foo2();
  case 3:
  default:
    foo3();
  }
}