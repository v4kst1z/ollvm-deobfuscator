/*
 * @Author: V4kst1z (dcydane@gmail.com)
 * @Date: 2021-08-19 18:05:07
 * @LastEditors: V4kst1z
 * @Description:
 * @FilePath: /ollvm-deobfuscator/tests/test4.c
 */

#include <stdio.h>

int main() {
  static int k[10];
  int i, j, n, s;
  for (j = 2; j < 1000; j++) {
    n = -1;
    s = j;
    for (i = 1; i < j; i++) {
      if ((j % i) == 0) {
        n++;
        s = s - i;
        k[n] = i;
      }
    }
    if (s == 0) {
      printf("%d is a wanshu: ", j);
      for (i = 0; i < n; i++)
        printf("%d,", k[i]);
      printf("%d\n", k[n]);
    }
  }
}