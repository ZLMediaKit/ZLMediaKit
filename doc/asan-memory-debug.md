# 使用 ASAN 调试 MediaServer 内存泄漏

## 背景

AddressSanitizer（ASAN）在 Linux 上默认集成 LeakSanitizer（LSan），可在程序退出时打印所有未释放内存的完整调用栈。项目已内置 `ENABLE_ASAN` 编译选项（默认关闭），无需修改代码即可启用。

---

## 一、重新编译（开启 ASAN）

在 `build` 目录下重新运行 cmake，以 Debug 模式开启 ASAN：

```bash
cd /w/ZLMediaKit/build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
make -j$(nproc) MediaServer
```

`ENABLE_ASAN=ON` 会为编译器附加以下标志：

| 标志 | 作用 |
|---|---|
| `-fsanitize=address` | 开启 AddressSanitizer |
| `-fno-omit-frame-pointer` | 保留帧指针，确保调用栈完整 |

链接阶段同样加入 `-fsanitize=address`。Debug 模式保留行号信息，调用栈可直接对应源码。

---

## 二、配置运行环境变量

运行前通过环境变量控制 ASAN / LSan 行为：

```bash
export ASAN_OPTIONS=detect_leaks=1:abort_on_error=0:log_path=/tmp/asan.log
export LSAN_OPTIONS=report_objects=1:print_suppressions=0
```

| 变量 | 说明 |
|---|---|
| `detect_leaks=1` | 开启 LeakSanitizer（Linux 默认已开启，显式指定更保险） |
| `abort_on_error=0` | 发现错误后不直接 abort，方便收集完整报告 |
| `log_path=/tmp/asan.log` | 将报告写入文件，避免被业务日志淹没 |
| `report_objects=1` | 显示泄漏对象的数量和总大小 |
| `print_suppressions=0` | 打印所有抑制规则匹配情况，便于排查误判 |

---

## 三、运行并复现问题

```bash
cd /w/ZLMediaKit/build
./server/MediaServer -c ../conf/config.ini
```

复现导致内存上涨的业务场景（推流、拉流、反复连接断开等），然后**正常退出**（`Ctrl+C` 或 `kill -SIGTERM <pid>`）。

> LSan 在进程退出时才汇总并打印泄漏报告，**不可使用 `kill -9`**，否则无输出。

---

## 四、分析报告

报告保存在 `/tmp/asan.log.<pid>`，典型格式如下：

```
=================================================================
==12345==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 1024 byte(s) in 1 object(s) allocated from:
    #0 0x... in operator new(unsigned long) (/lib/.../libasan.so)
    #1 0x... in SomeClass::create() /w/ZLMediaKit/src/SomeClass.cpp:42
    #2 0x... in ...

SUMMARY: AddressSanitizer: 1024 byte(s) leaked in 1 allocation(s).
```

分析要点：

- **Direct leak**：对象直接未释放，优先处理。
- **Indirect leak**：由 Direct leak 对象持有的子对象，根因在 Direct leak 处。
- 通过调用栈中的源文件路径和行号定位 `new` / `malloc` 的触发位置。

若调用栈显示 `??`（符号化失败），安装 `llvm-symbolizer` 并追加：

```bash
export ASAN_OPTIONS=...:symbolize=1
export ASAN_SYMBOLIZER_PATH=$(which llvm-symbolizer)
```

---

## 五、（可选）使用 jemalloc 追踪长时内存增长

若内存持续上涨但 LSan 未报泄漏（可能是逻辑上的内存积累），可改用 jemalloc 的统计 dump：

```bash
cmake .. -DENABLE_JEMALLOC_STATIC=ON -DENABLE_JEMALLOC_DUMP=ON
make -j$(nproc) MediaServer
```

运行时向进程发送 `SIGUSR1` 可触发内存统计输出到日志，适合分析缓存、对象池等长期驻留场景。

---

## 六、常见问题

| 现象 | 原因 | 解决方法 |
|---|---|---|
| 退出后无 `/tmp/asan.log.*` | 进程被 `kill -9` | 改用 `SIGTERM` 正常退出 |
| 调用栈只有地址无行号 | 未以 Debug 模式编译 | 加 `-DCMAKE_BUILD_TYPE=Debug` 重新编译 |
| 报告行数极多难以阅读 | 泄漏点太多 | 先用 `LSAN_OPTIONS=max_leaks=20` 限制输出条数 |
| 与 jemalloc 同时使用崩溃 | ASAN 与 jemalloc 不兼容 | 二者只能选其一，调试期优先用 ASAN |
