/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "RedLineConfig.h"
#include "Util/File.h"
#include "Util/logger.h"
#include "Util/util.h"
#include <fstream>
#include <sstream>

using namespace std;
using namespace toolkit;
using namespace mediakit;

RedLineConfigManager& RedLineConfigManager::getInstance() {
    static RedLineConfigManager instance;
    return instance;
}

RedLineConfigManager::RedLineConfigManager() {
    _config_file = "./redline_config.json";
    loadConfig(_config_file);
}

bool RedLineConfigManager::loadConfig(const string &config_file) {
    lock_guard<mutex> lock(_mutex);
    _config_file = config_file;

    ifstream ifs(config_file);
    if (!ifs.is_open()) {
        WarnL << "红线配置文件不存在,将使用默认配置: " << config_file;
        return false;
    }

    try {
        Json::Value root;
        Json::CharReaderBuilder builder;
        string errs;

        if (!Json::parseFromStream(builder, ifs, &root, &errs)) {
            ErrorL << "解析红线配置文件失败: " << errs;
            return false;
        }

        _configs.clear();

        const Json::Value &cameras = root["cameras"];
        if (cameras.isObject()) {
            for (auto it = cameras.begin(); it != cameras.end(); ++it) {
                string camera_id = it.key().asString();
                CameraRedLineConfig config = CameraRedLineConfig::fromJson(*it);
                config.camera_id = camera_id;
                _configs[camera_id] = config;
            }
        }

        InfoL << "成功加载红线配置,共 " << _configs.size() << " 个摄像头";
        return true;
    } catch (const exception &ex) {
        ErrorL << "加载红线配置文件异常: " << ex.what();
        return false;
    }
}

bool RedLineConfigManager::saveConfig(const string &config_file) {
    lock_guard<mutex> lock(_mutex);

    try {
        Json::Value root;
        Json::Value cameras;

        for (const auto &pair : _configs) {
            cameras[pair.first] = pair.second.toJson();
        }

        root["cameras"] = cameras;

        // 确保目录存在
        File::createfile_path(config_file.data(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

        ofstream ofs(config_file);
        if (!ofs.is_open()) {
            ErrorL << "无法打开配置文件进行写入: " << config_file;
            return false;
        }

        Json::StreamWriterBuilder builder;
        builder["indentation"] = "  ";
        unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
        writer->write(root, &ofs);

        InfoL << "成功保存红线配置到: " << config_file;
        return true;
    } catch (const exception &ex) {
        ErrorL << "保存红线配置文件异常: " << ex.what();
        return false;
    }
}

CameraRedLineConfig RedLineConfigManager::getConfig(const string &camera_id) {
    lock_guard<mutex> lock(_mutex);

    auto it = _configs.find(camera_id);
    if (it != _configs.end()) {
        return it->second;
    }

    // 返回空配置
    CameraRedLineConfig empty_config;
    empty_config.camera_id = camera_id;
    empty_config.enabled = false;
    return empty_config;
}

void RedLineConfigManager::setConfig(const string &camera_id, const CameraRedLineConfig &config) {
    lock_guard<mutex> lock(_mutex);
    _configs[camera_id] = config;
    saveConfig(_config_file);
}

void RedLineConfigManager::deleteConfig(const string &camera_id) {
    lock_guard<mutex> lock(_mutex);
    _configs.erase(camera_id);
    saveConfig(_config_file);
}

bool RedLineConfigManager::deleteRedLine(const string &camera_id, const string &line_id) {
    lock_guard<mutex> lock(_mutex);

    auto it = _configs.find(camera_id);
    if (it == _configs.end()) {
        return false;
    }

    auto &lines = it->second.lines;
    for (auto line_it = lines.begin(); line_it != lines.end(); ++line_it) {
        if (line_it->id == line_id) {
            lines.erase(line_it);
            saveConfig(_config_file);
            return true;
        }
    }

    return false;
}

Json::Value RedLineConfigManager::getAllConfigs() {
    lock_guard<mutex> lock(_mutex);

    Json::Value result;
    for (const auto &pair : _configs) {
        result[pair.first] = pair.second.toJson();
    }

    return result;
}

string RedLineConfigManager::generateFFmpegFilter(const string &camera_id) {
    lock_guard<mutex> lock(_mutex);

    auto it = _configs.find(camera_id);
    if (it == _configs.end() || !it->second.enabled || it->second.lines.empty()) {
        return "";
    }

    const auto &config = it->second;
    stringstream ss;

    bool first = true;
    for (const auto &line : config.lines) {
        if (line.points.empty()) {
            continue;
        }

        // 解析颜色 (#RRGGBB -> 0xRRGGBB)
        string color_hex = line.color;
        if (color_hex[0] == '#') {
            color_hex = "0x" + color_hex.substr(1);
        }

        if (!first) {
            ss << ",";
        }
        first = false;

        if (line.type == "line" && line.points.size() >= 2) {
            // 绘制直线
            const auto &p1 = line.points[0];
            const auto &p2 = line.points[1];

            ss << "drawbox=x=" << min(p1.x, p2.x)
               << ":y=" << min(p1.y, p2.y)
               << ":w=" << abs(p2.x - p1.x)
               << ":h=" << abs(p2.y - p1.y)
               << ":color=" << color_hex
               << ":t=" << line.thickness;

        } else if (line.type == "rect" && line.points.size() >= 2) {
            // 绘制矩形
            const auto &p1 = line.points[0];
            const auto &p2 = line.points[1];

            ss << "drawbox=x=" << min(p1.x, p2.x)
               << ":y=" << min(p1.y, p2.y)
               << ":w=" << abs(p2.x - p1.x)
               << ":h=" << abs(p2.y - p1.y)
               << ":color=" << color_hex
               << ":t=" << line.thickness;

        } else if (line.type == "polygon" && line.points.size() >= 3) {
            // 绘制多边形(通过多条线段实现)
            for (size_t i = 0; i < line.points.size(); i++) {
                size_t next = (i + 1) % line.points.size();
                const auto &p1 = line.points[i];
                const auto &p2 = line.points[next];

                if (i > 0) {
                    ss << ",";
                }

                ss << "drawbox=x=" << min(p1.x, p2.x)
                   << ":y=" << min(p1.y, p2.y)
                   << ":w=" << max(1, abs(p2.x - p1.x))
                   << ":h=" << max(1, abs(p2.y - p1.y))
                   << ":color=" << color_hex
                   << ":t=" << line.thickness;
            }
        }

        // 添加文本标签
        if (!line.label.empty()) {
            int text_x = line.points[0].x + 5;
            int text_y = line.points[0].y - 10;

            ss << ",drawtext=text='" << line.label
               << "':x=" << text_x
               << ":y=" << text_y
               << ":fontsize=16"
               << ":fontcolor=" << color_hex;
        }
    }

    return ss.str();
}
