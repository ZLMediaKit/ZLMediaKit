## 配置文件影响性能的参数

### 1、protocol.enable_xxx 
控制转协议开关，关闭某些协议节省cpu和内存。

### 2、protocol.xxx_demand
控制按需转协议，开启转协议且按需转协议时，无人观看时节省cpu和内存，但是第一个播放器无法秒开，影响体验

### 3、protocol.paced_sender_ms
平滑发送定时器频率，用于解决数据源发送不平滑导致转发不平滑播放器卡顿问题，开启后定时器根据数据时间戳驱动数据发送，提高用户体验。
但是增加cpu和内存使用。定时器间隔越小，cpu占用越高，但是平滑度越好，建议设置30~100ms。此功能结合protocol.modify_stamp为2(抑制时间戳跳跃)最佳。

### 4、general.mergeWriteMS 
开启合并写，减少发送数据时系统调用次数以及线程间数据共享频率，大大提高转发性能，但是牺牲播放延时和发送平滑度。

### 5、rtp_proxy.gop_cache
开启startSendRtp级联接口的gop缓存功能，用于国标级联秒开。该选项不影响zlmediakit对外提供直播服务的秒开。
开启该选项后增加内存使用，对cpu影响较小，如果不调用startSendRtp接口，建议关闭。

### 6、hls.fileBufSize
调整该配置可以提高hls协议写磁盘io性能。

### 7、record.fileBufSize
调整该配置可以提高mp4录制写磁盘io性能。
