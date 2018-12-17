# 测试环境
- 系统:centos6.8 64bit
- 内存:8GB
- CPU:Intel(R) Xeon(R) CPU E3-1220 v5 @ 3.00GHz;4核
- 网络:千兆网卡
- 测试端环境跟服务端一致(同一千兆局域网内)

# 测试工具
ZLMeidaKit自带测试程序test_benchmark，其为单进程模型；
请在编译ZLToolKit时打开宏`ENABLE_ASNC_TCP_CLIENT`，否则测试程序是单线程模型；将影响测试端性能。

# 测试服务器
ZLMeidaKit自带测试服务器test_server,支持RTSP/RTMP/HLS服务器；多线程模型。

# 测试媒体流
使用test_server拉取的rtmp流`rtmp://live.hkstv.hk.lxdns.com/live/hks`;然后通过test_server转发代理。
该码流大概300~400Kbit/s左右。

# 测试结果

说明:在cmake构建时，输入`cmake .. -DCMKAE_BUILD_TYPE=Release`以编译优化版本。

| 播放器个数(rtmp) | CPU(最大400%) | 内存(VIRT/RES) | 带宽(平均) | 丢包 |
| --- | --- | --- | --- | --- |
| 1000 | 35% | 582M/14M | 43.11 MByte/s | 无 |
| 2000 | 65% | 584M/20M | 84.88 MByte/s | 无 |

| 播放器个数(rtsp/tcp) | CPU(最大400%) | 内存(VIRT/RES) | 带宽(平均) | 丢包 |
| --- | --- | --- | --- | --- |
| 1000 | 42% | 582M/12M | 43.10 MByte/s| 无 |
| 2000 | 80% | 584M/17M | 85.97 MByte/s | 无 |

# srs性能对比
| 播放器个数(rtmp) | CPU(最大400%) | 内存(VIRT/RES) | 带宽(平均) | 丢包 |
| --- | --- | --- | --- | --- |
| 1000 | 10% | 310M/53M | 41.17 MByte/s | 无 |
| 2000 | 18% | 604M/117M | 83.86 MByte/s | 无 |
