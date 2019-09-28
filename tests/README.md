此目录下的所有.cpp文件将被编译成可执行程序(不包含此目录下的子目录).
子目录DeviceHK为海康IPC的适配程序,需要先下载海康的SDK才能编译,
由于操作麻烦,所以仅把源码放在这仅供参考.

- test_benchmark.cpp
    
    rtsp/rtmp性能测试客户端
    
- test_httpApi.cpp
  
  http api 测试服务器
 
- test_httpClient.cpp
   
   http 测试客户端

- test_player.cpp
   
   rtsp/rtmp带视频渲染的客户端

- test_pusher.cpp
   
   先拉流再推流的测试客户端
 
- test_pusherMp4.cpp
   
   解复用mp4文件再推流的测试客户端
 
- test_server.cpp
   
   rtsp/rtmp/http等服务器
 
- test_wsClient.cpp
  
  websocket测试客户端
 
- test_wsServer.cpp
   
   websocket回显测试服务器
 

