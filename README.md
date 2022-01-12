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
git clone https://github.com/v4kst1z/ollvm-deobfuscator
cd ollvm-deobfuscator && mkdir build && cd build
cmake ../
make -j16
```

```
opt -load-pass-plugin ./libDFlattening.so -load-pass-plugin ./libDBogusFlow.so  -passes="dfla","dbcf" -adce -instcombine -S test2.bc -o test2.bc

```





