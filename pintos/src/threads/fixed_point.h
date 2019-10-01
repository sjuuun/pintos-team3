#include <stdio.h>

#define f (1<<14)


int int_to_fix(int n);
int fix_to_int_r(int x);
int fix_to_int(int x);
int add_fix_fix(int x, int y);
int sub_fix_fix(int x, int y);
int add_fix_int(int x, int n);
int sub_fix_int(int x, int n);
int mul_fix_fix(int x, int y);
int mul_fix_int(int x, int n);
int div_fix_fix(int x, int y);
int div_fix_int(int x, int n);

int int_to_fix(int n) {
  return n * f;
}

int fix_to_int_r(int x) {
  return x / f;
}

int fix_to_int(int x) {
  if (x >= 0) return (x + f/2)/f;
  else return (x - f/2)/f;
}

int add_fix_fix(int x, int y){
  return x + y;
}

int sub_fix_fix(int x, int y){
  return x - y;
}

int add_fix_int(int x, int n){
  return x + n*f;
}

int sub_fix_int(int x, int n){
  return x - n*f;
}

int mul_fix_fix(int x, int y){
  return ((int64_t) x) * y/f;
}

int mul_fix_int(int x, int n){
  return x * n;
}

int div_fix_fix(int x, int y){
  return ((int64_t) x) *f/y;
}

int div_fix_int(int x, int n){
  return x / n;
}
