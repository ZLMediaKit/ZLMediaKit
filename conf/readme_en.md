## Key parameters that affect performance in the configuration file

### 1. Protocol enable flags (e.g., protocol.enable_hls, protocol.enable_rtsp)

Controls the protocol conversion flags. Disabling unnecessary protocols will save CPU and memory resources.

### 2. On-demand protocol flags (e.g., protocol.hls_demand, protocol.rtsp_demand)

Controls on-demand protocol generation. When both this and the specific protocol are enabled, it saves CPU and memory when there are no active viewers. However, the first viewer will lose the instant playback capability, impacting the initial experience.

### 3. protocol.paced_sender_ms

The interval for the smooth sending timer. This helps address playback stuttering caused by irregular data transmission from the source. When enabled, the timer uses data timestamps to pace the transmission, improving the viewing experience.
However, this increases CPU and memory consumption. A shorter timer interval results in higher CPU usage but better smoothness. The recommended interval is between 30 and 100 milliseconds. For optimal results, use this feature in conjunction with setting `protocol.modify_stamp` to 2 (which suppresses timestamp jumps).

### 4. general.mergeWriteMS

Enables write coalescing, which reduces the number of system calls and the frequency of data sharing between threads during transmission. This significantly boosts forwarding performance but comes at the cost of increased playback latency and reduced transmission smoothness.

### 5. rtp_proxy.gop_cache

Enables the GOP (Group of Pictures) caching feature for the `startSendRtp` cascaded interface, designed to allow instant playback for cascading setups (e.g., GB28181). Note that this setting does not affect the instant playback capability of ZLMediaKit's external live streaming services.
Enabling this option increases memory usage but has a minimal impact on the CPU. We recommend disabling it if you don't use the `startSendRtp` interface.

### 6. hls.fileBufSize

Tuning this parameter can improve the disk I/O performance when writing HLS streams.

### 7. record.fileBufSize

Tuning this parameter can improve the disk I/O performance when recording MP4 files.
