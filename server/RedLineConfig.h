/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_REDLINECONFIG_H
#define ZLMEDIAKIT_REDLINECONFIG_H

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include "json/json.h"

namespace mediakit {

/**
 * 红线点坐标
 */
struct RedLinePoint {
    int x;
    int y;

    RedLinePoint() : x(0), y(0) {}
    RedLinePoint(int x, int y) : x(x), y(y) {}

    Json::Value toJson() const {
        Json::Value point;
        point.append(x);
        point.append(y);
        return point;
    }

    static RedLinePoint fromJson(const Json::Value &json) {
        if (json.isArray() && json.size() >= 2) {
            return RedLinePoint(json[0].asInt(), json[1].asInt());
        }
        return RedLinePoint();
    }
};

/**
 * 红线配置
 */
struct RedLine {
    std::string id;                         // 红线唯一ID
    std::string type;                       // 类型: "line", "rect", "polygon"
    std::vector<RedLinePoint> points;       // 点坐标列表
    std::string color;                      // 颜色 (hex格式: #RRGGBB)
    int thickness;                          // 线条粗细
    std::string label;                      // 标签文本

    RedLine() : thickness(2), color("#FF0000") {}

    Json::Value toJson() const {
        Json::Value line;
        line["id"] = id;
        line["type"] = type;
        line["color"] = color;
        line["thickness"] = thickness;
        line["label"] = label;

        Json::Value pointsJson;
        for (const auto &point : points) {
            pointsJson.append(point.toJson());
        }
        line["points"] = pointsJson;

        return line;
    }

    static RedLine fromJson(const Json::Value &json) {
        RedLine line;
        line.id = json["id"].asString();
        line.type = json["type"].asString();
        line.color = json["color"].asString();
        line.thickness = json["thickness"].asInt();
        line.label = json["label"].asString();

        const Json::Value &pointsJson = json["points"];
        if (pointsJson.isArray()) {
            for (const auto &pointJson : pointsJson) {
                line.points.push_back(RedLinePoint::fromJson(pointJson));
            }
        }

        return line;
    }
};

/**
 * 摄像头红线配置
 */
struct CameraRedLineConfig {
    std::string camera_id;                  // 摄像头ID (通常是 app/stream)
    std::vector<RedLine> lines;             // 红线列表
    bool enabled;                           // 是否启用

    CameraRedLineConfig() : enabled(true) {}

    Json::Value toJson() const {
        Json::Value config;
        config["camera_id"] = camera_id;
        config["enabled"] = enabled;

        Json::Value linesJson;
        for (const auto &line : lines) {
            linesJson.append(line.toJson());
        }
        config["lines"] = linesJson;

        return config;
    }

    static CameraRedLineConfig fromJson(const Json::Value &json) {
        CameraRedLineConfig config;
        config.camera_id = json["camera_id"].asString();
        config.enabled = json["enabled"].asBool();

        const Json::Value &linesJson = json["lines"];
        if (linesJson.isArray()) {
            for (const auto &lineJson : linesJson) {
                config.lines.push_back(RedLine::fromJson(lineJson));
            }
        }

        return config;
    }
};

/**
 * 红线配置管理器
 */
class RedLineConfigManager {
public:
    static RedLineConfigManager& getInstance();

    /**
     * 加载配置文件
     */
    bool loadConfig(const std::string &config_file);

    /**
     * 保存配置文件
     */
    bool saveConfig(const std::string &config_file);

    /**
     * 获取指定摄像头的红线配置
     */
    CameraRedLineConfig getConfig(const std::string &camera_id);

    /**
     * 设置指定摄像头的红线配置
     */
    void setConfig(const std::string &camera_id, const CameraRedLineConfig &config);

    /**
     * 删除指定摄像头的配置
     */
    void deleteConfig(const std::string &camera_id);

    /**
     * 删除指定摄像头的某条红线
     */
    bool deleteRedLine(const std::string &camera_id, const std::string &line_id);

    /**
     * 获取所有配置
     */
    Json::Value getAllConfigs();

    /**
     * 生成FFmpeg滤镜命令
     */
    std::string generateFFmpegFilter(const std::string &camera_id);

    /**
     * 为指定摄像头生成红线PNG图片
     * @param camera_id 摄像头ID
     * @param width 视频宽度
     * @param height 视频高度
     * @return PNG文件路径，如果没有配置或生成失败返回空字符串
     */
    std::string generateOverlayPNG(const std::string &camera_id, int width, int height);

private:
    RedLineConfigManager();
    ~RedLineConfigManager() = default;

    RedLineConfigManager(const RedLineConfigManager&) = delete;
    RedLineConfigManager& operator=(const RedLineConfigManager&) = delete;

    std::map<std::string, CameraRedLineConfig> _configs;
    std::mutex _mutex;
    std::string _config_file;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_REDLINECONFIG_H
