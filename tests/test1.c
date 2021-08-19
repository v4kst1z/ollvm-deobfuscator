/*
 * @Author: V4kst1z (dcydane@gmail.com)
 * @Date: 2021-08-19 18:04:52
 * @LastEditors: V4kst1z
 * @Description:
 * @FilePath: /ollvm-deobfuscator/tests/test1.c
 */
#include <stdio.h>
int foo(int a) { return a * 2; }

int main(int argc, char *argv[]) {
  int a = 123;
  int ret = 0;

  ret += foo(a);

  return ret;
}
