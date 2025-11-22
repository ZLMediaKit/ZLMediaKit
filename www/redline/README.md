# ZLMediaKit 红线配置功能

## 功能介绍

这个功能允许为指定的摄像头视频流添加红线标记（直线、矩形、多边形），红线会实时叠加在视频流上。适用于：

- 安防监控区域标记
- 禁入区域警示
- 关键区域框选
- 智能分析辅助线

## 功能特性

- ✅ 支持多种红线类型：直线、矩形、多边形
- ✅ 可视化Web配置界面
- ✅ 支持自定义颜色、粗细、标签
- ✅ 基于FFmpeg滤镜实现，性能优异
- ✅ 配置持久化存储
- ✅ 完整的RESTful API支持

## 使用方法

### 1. 访问Web配置界面

启动MediaServer后，在浏览器中访问：

```
http://your-server-ip:port/redline/
```

### 2. 配置摄像头红线

1. 输入API密钥（默认：035c73f7-bb6b-4889-a715-d9eb2d1925cc）
2. 输入摄像头ID（格式：app/stream，例如：live/camera01）
3. 点击"加载/新建"按钮
4. （可选）输入视频流URL进行预览
5. 选择红线类型（直线/矩形/多边形）
6. 在视频画面上点击绘制红线
7. 设置颜色、粗细和标签
8. 点击"保存配置"

### 3. 应用红线

配置保存后，当使用`addStreamProxy`等API拉流时，红线会自动叠加到视频流上。

例如：

```bash
curl -X POST "http://your-server-ip:port/index/api/addStreamProxy" \
  -d "secret=035c73f7-bb6b-4889-a715-d9eb2d1925cc" \
  -d "vhost=__defaultVhost__" \
  -d "app=live" \
  -d "stream=camera01" \
  -d "url=rtsp://admin:password@192.168.1.100:554/h264/ch1/main/av_stream"
```

如果已经为`live/camera01`配置了红线，拉流后的视频将自动包含红线标记。

## API接口

### 获取摄像头红线配置

```http
GET /index/api/getRedLineConfig?secret=xxx&camera_id=live/camera01
```

### 设置摄像头红线配置

```http
POST /index/api/setRedLineConfig
Content-Type: application/json

{
  "secret": "035c73f7-bb6b-4889-a715-d9eb2d1925cc",
  "camera_id": "live/camera01",
  "enabled": true,
  "lines": [
    {
      "id": "line_1",
      "type": "line",
      "points": [[100, 100], [500, 100]],
      "color": "#FF0000",
      "thickness": 2,
      "label": "警戒线"
    }
  ]
}
```

### 删除摄像头红线配置

```http
GET /index/api/deleteRedLineConfig?secret=xxx&camera_id=live/camera01
```

### 删除指定红线

```http
GET /index/api/deleteRedLine?secret=xxx&camera_id=live/camera01&line_id=line_1
```

### 获取所有配置

```http
GET /index/api/getAllRedLineConfigs?secret=xxx
```

## 配置文件

红线配置保存在：`./redline_config.json`

示例配置：

```json
{
  "cameras": {
    "live/camera01": {
      "camera_id": "live/camera01",
      "enabled": true,
      "lines": [
        {
          "id": "line_1732246800000",
          "type": "rect",
          "points": [[100, 100], [400, 300]],
          "color": "#FF0000",
          "thickness": 3,
          "label": "监控区域"
        }
      ]
    }
  }
}
```

## 红线类型说明

### 直线 (line)
需要2个点，绘制一条直线

### 矩形 (rect)
需要2个点（左上角和右下角），绘制矩形框

### 多边形 (polygon)
需要至少3个点，绘制多边形，双击完成绘制

## 技术实现

- 后端：基于FFmpeg的drawbox滤镜实现红线绘制
- 前端：HTML5 Canvas实现可视化配置
- 存储：JSON文件持久化配置
- API：RESTful风格接口

## 注意事项

1. 启用红线会触发视频重新编码，会增加一定的CPU开销
2. 坐标系统基于视频原始分辨率
3. 建议在服务器性能允许的情况下使用
4. 配置更改后需要重新拉流才能生效

## 故障排除

### 红线没有显示
- 检查配置是否正确保存
- 确认camera_id与实际流的app/stream匹配
- 检查MediaServer日志，查看FFmpeg命令是否包含滤镜参数

### Web界面无法访问
- 检查MediaServer的HTTP服务是否启动
- 确认www目录配置正确
- 检查防火墙设置

## 开发者

该功能由Claude Code开发，集成到ZLMediaKit项目中。

## 许可证

遵循ZLMediaKit项目的MIT许可证。
