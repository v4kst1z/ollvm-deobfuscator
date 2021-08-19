/*
 * @Author: V4kst1z (dcydane@gmail.com)
 * @Date: 2021-08-19 18:05:03
 * @LastEditors: V4kst1z
 * @Description:
 * @FilePath: /ollvm-deobfuscator/tests/test3.c
 */

#include <stdio.h>
int main() {
  int i, j, a[6][6];
  for (i = 0; i <= 5; i++) {
    a[i][i] = 1;
    a[i][0] = 1;
  }
  for (i = 2; i <= 5; i++)
    for (j = 1; j <= i - 1; j++)
      a[i][j] = a[i - 1][j] + a[i - 1][j - 1];
  for (i = 0; i <= 5; i++) {
    for (j = 0; j <= i; j++)
      printf("%4d", a[i][j]);
    printf("\n");
  }
}