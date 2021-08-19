# ollvm-deobfuscator
这个项目是因为看到 `retdec` 可以把 `bin` 转成 `llvm ir`，所以就写了个 `llvm pass` 来对 `ollvm` 进行解混淆，可是最后发现 `retdec` 似乎有些问题，转成 `llvm ir` 后再编译成 `bin` 会崩溃，所以在测试的时候没法验证做的对不对，不知道是 retdec 的问题还是 `pass` 的问题。本来这个 `pass` 是基于 `retdec` 生产的 `llvm ir` 处理的，但是后来测试的时候发现 `clang` 生成的 `llvm ir` 也可以被解混淆。

这个放到以后完善吧。