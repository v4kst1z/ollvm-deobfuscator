/*
 * @Author: V4kst1z (dcydane@gmail.com)
 * @Date: 2021-08-19 18:05:00
 * @LastEditors: V4kst1z
 * @Description:
 * @FilePath: /ollvm-deobfuscator/tests/test2.c
 */

#include <stdlib.h>

unsigned int target_function(unsigned int n) {
  unsigned int mod = n % 4;
  unsigned int result = 0;

  if (mod == 0)
    result = (n | 0xBAAAD0BF) * (2 ^ n);

  else if (mod == 1)
    result = (n & 0xBAAAD0BF) * (3 + n);

  else if (mod == 2)
    result = (n ^ 0xBAAAD0BF) * (4 | n);

  else
    result = (n + 0xBAAAD0BF) * (5 & n);

  return result;
}

int main() {
  int a = 10;
  int b = a / 3;
  int c = a + a * b + 4;
  int d = a + c * b;
  return target_function(d);
}