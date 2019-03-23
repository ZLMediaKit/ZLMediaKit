# 测试环境
- 系统:Linux core 3.16.0-7-amd64 #1 SMP Debian 3.16.59-1 (2018-10-03) x86_64 GNU/Linux
- 内存:15G
- CPU:Intel(R) Xeon(R) CPU E3-1220 v5 @ 3.00GHz;4核
- 网络:千兆网卡
- 测试端采用回环网络方式访问服务器

# 测试工具
ZLMeidaKit自带测试程序test_benchmark，其为单进程多线程模型

# 测试服务器
ZLMeidaKit自带测试服务器test_server,支持RTSP/RTMP/HLS服务器；多线程模型。

# 测试媒体流
使用test_server拉取的rtmp流`rtmp://live.hkstv.hk.lxdns.com/live/hks1`;然后通过test_server转发代理。
该码流大概300~400Kbit/s左右。

# 测试结果

说明:在cmake构建时，输入`cmake .. -DCMKAE_BUILD_TYPE=Release`以编译优化版本。

| 播放器个数(rtmp) | CPU(最大400%) | 内存(VIRT/RES) | 带宽(平均) | 丢包 |
| --- | --- | --- | --- | --- |
| 1000 | 20% | 702M/13M | 40 MByte/s | 无 |
| 2000 | 39% | 702M/18M | 80 MByte/s | 无 |
| 5000 | 92% | 702M/32M | 200 MByte/s | 无 |
| 10000 | 170% | 702M/59M | 400 MByte/s | 无 |

| 播放器个数(rtsp/tcp) | CPU(最大400%) | 内存(VIRT/RES) | 带宽(平均) | 丢包 |
| --- | --- | --- | --- | --- |
| 1000 | 18% | 702M/13M | 42 MByte/s| 无 |
| 2000 | 35% | 702M/19M | 82 MByte/s | 无 |
| 5000 | 80% | 702M/35M | 198 MByte/s | 无 |
| 10000 | 130% | 702M/62M | 405 MByte/s | 无 |

# srs性能对比
| 播放器个数(rtmp) | CPU(最大400%) | 内存(VIRT/RES) | 带宽(平均) | 丢包 |
| --- | --- | --- | --- | --- |
| 1000 | 10% | 310M/53M | 41.17 MByte/s | 无 |
| 2000 | 18% | 604M/117M | 83.86 MByte/s | 无 |
