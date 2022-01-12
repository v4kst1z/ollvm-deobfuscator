<!--
 * @Author: V4kst1z (dcydane@gmail.com)
 * @Date: 2021-08-19 18:22:30
 * @LastEditors: V4kst1z
 * @Description: 
 * @FilePath: /ollvm-deobfuscator/README.md
-->
# ollvm-deobfuscator
## 环境搭建
### ollvm 编译
```
git clone -b llvm-4.0 https://github.com/obfuscator-llvm/obfuscator.git
cd obfuscator && mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DLLVM_INCLUDE_TESTS=OFF ..

//patch
https://github.com/obfuscator-llvm/obfuscator/issues/166

make -j16 && cd bin
./clang -mllvm -fla -mllvm -bcf test.c  -o test
```
### 安装llvm8
```
yay -S llvm8
```
### 项目编译
```
export LLVM_DIR=<installation/dir/of/llvm/13>
git clone https://github.com/v4kst1z/ollvm-deobfuscator
cd ollvm-deobfuscator && mkdir build && cd build
cmake ../
make -j16
```

```
opt -load-pass-plugin ./libDFlattening.so -load-pass-plugin ./libDBogusFlow.so  -passes="dfla","dbcf" -adce -instcombine -S test2.bc -o test2.bc

```










这个项目是因为看到 `retdec` 可以把 `bin` 转成 `llvm ir`，所以就写了个 `llvm pass` 来对 `ollvm` 进行解混淆，可是最后发现 `retdec` 似乎有些问题，转成 `llvm ir` 后再编译成 `bin` 会崩溃，所以在测试的时候没法验证做的对不对，不知道是 retdec 的问题还是 `pass` 的问题。本来这个 `pass` 是基于 `retdec` 生产的 `llvm ir` 处理的，但是后来测试的时候发现 `clang` 生成的 `llvm ir` 也可以被解混淆。

