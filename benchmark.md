# 测试环境
- 系统:centos6.8 64bit
- 内存:8GB
- CPU:Intel(R) Xeon(R) CPU E3-1220 v5 @ 3.00GHz;4核
- 网络:千兆网卡
- 测试端环境跟服务端一致(同一千兆局域网内)

#测试工具
ZLMeidaKit自带测试程序test_benchmark，其为单进程模型；
请在编译ZLToolKit时打开宏`ENABLE_ASNC_TCP_CLIENT`，否则测试程序是单线程模型；将影响测试端性能。

#测试服务器
ZLMeidaKit自带测试服务器test_server,支持RTSP/RTMP/HLS服务器；多线程模型。

#测试媒体流
使用test_server拉取的rtmp流`rtmp://live.hkstv.hk.lxdns.com/live/hks`;然后通过test_server转发代理。

#测试方法

| 播放器个数(rtmp) | CPU(最大400%) | 内存(VIRT/RES) | 带宽(平均) | 丢包 |
| --- | --- | --- | --- | --- |
| 4000 | 230% | 592M/50M | 59.92 MBit/s | 无 |
| 2000 | 110% | 587M/23M | 31.07 MBit/s | 无 |

#测试说明
由于测试服务器性能不足的原因，只能在单台机器上开启2000个播放器实例，否则将会丢包。所以测试过程中，我在测试4000并发量时采用的是2000x2的配置。其中的2000个实例是在test_server同主机上播放127.0.0.1回环地址完成的。


