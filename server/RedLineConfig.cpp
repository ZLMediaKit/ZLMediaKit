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
#include "System.h"
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

string RedLineConfigManager::generateOverlayPNG(const string &camera_id, int width, int height) {
    lock_guard<mutex> lock(_mutex);

    auto it = _configs.find(camera_id);
    if (it == _configs.end() || !it->second.enabled || it->second.lines.empty()) {
        return "";
    }

    const auto &config = it->second;

    // 生成PNG文件路径
    string png_dir = "./redline_overlays/";
    File::createfile_path(png_dir.data(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    // 使用camera_id生成安全的文件名
    string safe_camera_id = camera_id;
    std::replace(safe_camera_id.begin(), safe_camera_id.end(), '/', '_');
    string png_file = png_dir + safe_camera_id + ".png";

    // 使用ImageMagick或Python生成PNG
    // 这里我们使用Python + PIL，因为它最通用
    string python_script = png_dir + "generate_overlay.py";

    // 生成Python脚本
    ofstream script(python_script);
    if (!script.is_open()) {
        ErrorL << "无法创建Python脚本: " << python_script;
        return "";
    }

    script << "#!/usr/bin/env python3\n";
    script << "from PIL import Image, ImageDraw, ImageFont\n";
    script << "import sys\n\n";
    script << "# 创建透明图片\n";
    script << "img = Image.new('RGBA', (" << width << ", " << height << "), (0, 0, 0, 0))\n";
    script << "draw = ImageDraw.Draw(img)\n\n";

    // 生成绘制代码
    for (const auto &line : config.lines) {
        if (line.points.empty()) {
            continue;
        }

        // 解析颜色 (#RRGGBB)
        string color = line.color;
        if (color[0] == '#' && color.length() == 7) {
            int r = stoi(color.substr(1, 2), nullptr, 16);
            int g = stoi(color.substr(3, 2), nullptr, 16);
            int b = stoi(color.substr(5, 2), nullptr, 16);

            script << "# 绘制: " << line.label << "\n";
            script << "color = (" << r << ", " << g << ", " << b << ", 255)\n";

            if (line.type == "line" && line.points.size() >= 2) {
                script << "draw.line([";
                script << "(" << line.points[0].x << ", " << line.points[0].y << "), ";
                script << "(" << line.points[1].x << ", " << line.points[1].y << ")";
                script << "], fill=color, width=" << line.thickness << ")\n";

            } else if (line.type == "rect" && line.points.size() >= 2) {
                int x1 = min(line.points[0].x, line.points[1].x);
                int y1 = min(line.points[0].y, line.points[1].y);
                int x2 = max(line.points[0].x, line.points[1].x);
                int y2 = max(line.points[0].y, line.points[1].y);

                script << "draw.rectangle([";
                script << "(" << x1 << ", " << y1 << "), ";
                script << "(" << x2 << ", " << y2 << ")";
                script << "], outline=color, width=" << line.thickness << ")\n";

            } else if (line.type == "polygon" && line.points.size() >= 3) {
                script << "draw.polygon([";
                for (size_t i = 0; i < line.points.size(); i++) {
                    if (i > 0) script << ", ";
                    script << "(" << line.points[i].x << ", " << line.points[i].y << ")";
                }
                script << "], outline=color, width=" << line.thickness << ")\n";
            }

            // 绘制标签
            if (!line.label.empty()) {
                script << "try:\n";
                script << "    font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 16)\n";
                script << "except:\n";
                script << "    font = ImageFont.load_default()\n";
                script << "draw.text((" << (line.points[0].x + 5) << ", " << (line.points[0].y - 20)
                       << "), '" << line.label << "', fill=color, font=font)\n";
            }
            script << "\n";
        }
    }

    script << "# 保存PNG\n";
    script << "img.save('" << png_file << "')\n";
    script << "print('PNG generated: " << png_file << "')\n";
    script.close();

    // 执行Python脚本生成PNG
#ifdef _WIN32
    string cmd = "python " + python_script;
#else
    string cmd = "python3 " + python_script;
#endif

    InfoL << "执行命令生成PNG: " << cmd;
    string result = trim(System::execute(cmd));
    InfoL << "生成结果: " << result;

    // 检查PNG文件是否生成成功
    if (File::fileSize(png_file.data()) > 0) {
        InfoL << "成功生成红线PNG: " << png_file;
        return png_file;
    } else {
        WarnL << "PNG生成失败";
        return "";
    }
}
