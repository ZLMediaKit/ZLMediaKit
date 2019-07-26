//
// Created by 陈磊 on 2019-06-24.
//
#include <string>
#include "DbUtil.h"
#include "Globals.h"
#include <functional>
#include <sqlite3pp/sqlite3pp.h>
#include "jsoncpp/json.h"
#include "Common/config.h"
#include <unordered_map>

static ChannelPropTupleList ctl;
static std::unordered_map<string, ChannelPropTuple> ctm;

void initChannelAss() {
    ctl.push_back(ChannelPropTuple("id", "ID", "int"));
    ctl.push_back(ChannelPropTuple("name", "通道名称", "string"));
    ctl.push_back(ChannelPropTuple("vhost", "虚拟主机Vhost", "string"));
    ctl.push_back(ChannelPropTuple("app", "应用标识App", "string"));
    ctl.push_back(ChannelPropTuple("stream", "通道标识Stream", "string"));
    ctl.push_back(ChannelPropTuple("source_url", "接入地址", "string"));
    ctl.push_back(ChannelPropTuple("ffmpeg_cmd", "FFMpeg拉流参数", "string"));
    ctl.push_back(ChannelPropTuple("enable_hls", "是否开启HLS", "int"));
    ctl.push_back(ChannelPropTuple("record_mp4", "录像保留(天)", "int"));
    ctl.push_back(ChannelPropTuple("rtsp_transport", "RTSP协议(1:TCP,2:UDP)", "int"));
    ctl.push_back(ChannelPropTuple("on_demand", "按需直播", "int"));
    ctl.push_back(ChannelPropTuple("active", "是否启用", "int"));


    for (const auto &item : ctl) {
        ChannelPropTuple c = item;
        string desc = std::get<1>(c);
        ctm[desc] = c;
    }

}

std::unordered_map<string, ChannelPropTuple> getChannelPropsMap() {
    return ctm;
}

ChannelPropTupleList getChannelPropTupleList() {
    return ctl;
}

std::string channelsJsonToCsvStr(Json::Value channels) {
    auto channelsData = channels.isNull() ? Json::Value(Json::ValueType::arrayValue) : channels;
    _StrPrinter printer;
    printer << "\xef\xbb\xbf"; // BOM UTF-8

    ChannelPropTupleList props = ctl;
    for (auto &pp : props) {
        printer << std::get<1>(pp) << ",";
    }
    printer << "\r\n";


    for (Json::Value::ArrayIndex i = 0; i != channelsData.size(); i++) {
        Json::Value cConfig = channelsData[i];

        for (auto &pp : props) {
            printer << cConfig[std::get<0>(pp)] << ",";
        }
        printer << "\r\n" << endl;
    }
    return printer;
}

Json::Value channelsCsvStrToJson(std::string channelsCsv) {
    Json::Value csvJsonRet;
    vector<string> rows = split(channelsCsv, "\r\n");
    if (rows.size() == 0) {
        throw std::invalid_argument("无效的csv文件,请检查!");
    } else {
        if (rows.size() > 1) {
            string titleRowStr = rows[0];
            vector<string> titleCols = split(titleRowStr, ",");
            vector<ChannelPropTuple> titlePropCols;
            for (vector<string>::iterator iter = titleCols.begin(); iter != titleCols.end(); iter++) {
                string propName = trim(*iter, "\n\r \xef\xbb\xbf");

                ChannelPropTuple prop = ctm[propName];
                titlePropCols.push_back(prop);

            }
            for (int i = 1; i < rows.size(); i++) {
                auto dataRow = rows[i];
                vector<string> dataCols = split2(dataRow, ",");
                Json::Value rowJson;
                for (int m = 0; m < dataCols.size(); m++) {
                    ChannelPropTuple prop = titlePropCols[m];
                    string propName = std::get<0>(prop);
                    string propType = std::get<2>(prop);
                    string val = trim(dataCols[m], "\"");
                    if (!propName.empty()) {
                        if (propType.compare("int") == 0) {
                            rowJson[propName] = atoi(val.c_str());
                        } else {
                            rowJson[propName] = val;
                        }
                    }
                }
                csvJsonRet.append(rowJson);
            }
        }

    }
    InfoL << "csvJsonRet" << ": " << csvJsonRet.toStyledString();
    return csvJsonRet;
}


static string dbpath = "zlmedia.db";

void initDatabase(string dbpathParent) {
    initChannelAss();
    dbpath = dbpathParent+dbpath;
    InfoL << "dbpath: " << dbpath;
    sqlite3pp::database db(dbpath.data());

    try {
        string channelTableCreateSql = "CREATE TABLE IF NOT EXISTS CHANNEL("  \
         "ID  INTEGER   PRIMARY KEY         AUTOINCREMENT," \
         "PROXY_KEY             TEXT        NOT NULL UNIQUE," \
         "NAME                  TEXT        NOT NULL," \
         "VHOST                 TEXT        NOT NULL," \
         "APP                   TEXT        NOT NULL," \
         "STREAM                TEXT        NOT NULL," \
         "SOURCE_URL            TEXT        NOT NULL," \
         "FFMPEG_CMD            TEXT        NOT NULL," \
         "ENABLE_HLS            TINYINT     NOT NULL," \
         "RECORD_MP4            INT         NOT NULL," \
         "RTSP_TRANSPORT        TINYINT     NOT NULL," \
         "ON_DEMAND             TINYINT     NOT NULL,"  \
         "ACTIVE                TINYINT     NOT NULL,"  \
         "CREATE_TIME           DATETIME    NOT NULL,"  \
         "MODIFY_TIME           DATETIME    DEFAULT (datetime('now', 'localtime'))"  \
         ");";

        db.execute(channelTableCreateSql.data());

    } catch (exception &ex) {
        ErrorL << ex.what();
    }
}


int deleteChannel(int channelId, std::function<void()> cb) {
    try {
        sqlite3pp::database db(dbpath.data());

        sqlite3pp::transaction xct(db, true);
        sqlite3pp::command cmd(db, " DELETE FROM CHANNEL " \
                                       "WHERE " \
                                       "ID = :id");

        cmd.bind(":id", channelId);
        int rc = cmd.execute();
        xct.commit();

        if (rc == SQLITE_OK) {
            //回调
            cb();
        }

        return rc;

    } catch (exception &ex) {
        ErrorL << "删除 channel通道 失败" << ex.what();
    }

    return -1;
}


int updateChannel(int channelId, Json::Value jsonArgs, std::function<void(Json::Value channel)> cb) {
    try {
        sqlite3pp::database db(dbpath.data());

        sqlite3pp::transaction xct(db, true);
        sqlite3pp::command cmd(db, "UPDATE CHANNEL " \
                                       "SET " \
                                       "PROXY_KEY=:proxyKey, " \
                                       "NAME=:name, " \
                                       "VHOST=:vhost, " \
                                       "APP=:app, "\
                                       "STREAM=:stream, "\
                                       "SOURCE_URL=:source_url, " \
                                       "FFMPEG_CMD=:ffmpeg_cmd, " \
                                       "ENABLE_HLS=:enable_hls, " \
                                       "RECORD_MP4=:record_mp4, " \
                                       "RTSP_TRANSPORT=:rtsp_transport, " \
                                       "ON_DEMAND=:on_demand, " \
                                       "ACTIVE=:active," \
                                       "MODIFY_TIME=datetime('now', 'localtime') " \
                                       "WHERE " \
                                       "ID = :id");

        string vhost = jsonArgs.get("vhost", DEFAULT_VHOST).asString();
        string app = jsonArgs["app"].asString();
        string stream = jsonArgs["stream"].asString();

        string proxyKey = getProxyKey(vhost, app, stream);
        cmd.bind(":proxyKey", proxyKey, sqlite3pp::copy);
        cmd.bind(":name", jsonArgs["name"].asString(), sqlite3pp::copy);
        cmd.bind(":vhost", vhost, sqlite3pp::copy);
        cmd.bind(":app", app, sqlite3pp::copy);
        cmd.bind(":stream", stream, sqlite3pp::copy);
        cmd.bind(":source_url", jsonArgs["source_url"].asString(), sqlite3pp::copy);
        cmd.bind(":ffmpeg_cmd", jsonArgs.get("ffmpeg_cmd", "").asString(), sqlite3pp::copy);
        cmd.bind(":enable_hls", jsonArgs.get("enable_hls", 1).asInt());
        cmd.bind(":record_mp4", jsonArgs.get("record_mp4", 0).asInt());
        cmd.bind(":rtsp_transport", jsonArgs.get("rtsp_transport", 1).asInt());
        cmd.bind(":on_demand", jsonArgs.get("on_demand", 1).asInt());
        cmd.bind(":active", jsonArgs.get("active", 0).asInt());
        cmd.bind(":id", channelId);
        int rc = cmd.execute();
        xct.commit();

        if (rc == SQLITE_OK) {
            Json::Value ret = jsonArgs;
            ret["proxyKey"] = proxyKey;
            //回调
            cb(ret);
        }

        return rc;

    } catch (exception &ex) {
        ErrorL << "更新 channel通道 失败" << ex.what();
    }

    return -1;
}

extern int saveChannel(int channelId, Json::Value jsonArgs,
                       std::function<void(bool isCreate, Json::Value originalChannel, Json::Value channel)> cb) {
    int rc = 0;
    auto createFunc = [jsonArgs, cb]() {
        return createChannel(jsonArgs, [cb](Json::Value channel) {
            cb(true, channel, channel);
        });
    };
    if (channelId == 0) {
        rc = createFunc();
    } else {
        Json::Value originalChannel = searchChannel(channelId);
        if (originalChannel.isNull()) {
            rc = createFunc();
        } else {
            rc = updateChannel(channelId, jsonArgs, [cb, originalChannel](Json::Value channel) {
                cb(false, originalChannel, channel);
            });
        }
    }
    return rc;
}

int createChannel(Json::Value jsonArgs, std::function<void(Json::Value channel)> cb) {
    try {

        sqlite3pp::database db(dbpath.data());

        sqlite3pp::transaction xct(db, true);
        sqlite3pp::command cmd(db, "INSERT INTO CHANNEL " \
                                       " (ID, PROXY_KEY, NAME, VHOST, APP, STREAM, SOURCE_URL, FFMPEG_CMD, ENABLE_HLS, RECORD_MP4, RTSP_TRANSPORT, ON_DEMAND, ACTIVE, CREATE_TIME)" \
                                       " VALUES" \
                                       " (:id, :proxyKey, :name, :vhost, :app, :stream, :source_url, :ffmpeg_cmd, :enable_hls, :record_mp4, :rtsp_transport, :on_demand, :active, datetime('now', 'localtime'))");


        string vhost = jsonArgs.get("vhost", DEFAULT_VHOST).asString();
        string app = jsonArgs["app"].asString();
        string stream = jsonArgs["stream"].asString();

        string proxyKey = getProxyKey(vhost, app, stream);


        cmd.bind(":proxyKey", proxyKey, sqlite3pp::copy);
        if (jsonArgs["id"].isNull()) {
            cmd.bind(":id", sqlite3pp::null_type());
        } else {
            cmd.bind(":id", jsonArgs["id"].asInt());
        }

        cmd.bind(":name", jsonArgs["name"].asString(), sqlite3pp::copy);
        cmd.bind(":vhost", vhost, sqlite3pp::copy);
        cmd.bind(":app", app, sqlite3pp::copy);
        cmd.bind(":stream", stream, sqlite3pp::copy);
        cmd.bind(":source_url", jsonArgs["source_url"].asString(), sqlite3pp::copy);
        cmd.bind(":ffmpeg_cmd", jsonArgs.get("ffmpeg_cmd", "").asString(), sqlite3pp::copy);
        cmd.bind(":enable_hls", jsonArgs.get("enable_hls", 1).asInt());
        cmd.bind(":record_mp4", jsonArgs.get("record_mp4", 0).asInt());
        cmd.bind(":rtsp_transport", jsonArgs.get("rtsp_transport", 1).asInt());
        cmd.bind(":on_demand", jsonArgs.get("on_demand", 1).asInt());
        cmd.bind(":active", jsonArgs.get("active", 0).asInt());

        int rc = cmd.execute();
        xct.commit();

        if (rc == SQLITE_OK) {
            sqlite3pp::query qry(db, "select last_insert_rowid()");
            sqlite3pp::query::iterator lastRowIdIter = qry.begin();
            int lastInsertId;
            std::tie(lastInsertId) = (*lastRowIdIter).get_columns<int>(0);

            Json::Value ret = jsonArgs;
            ret["proxyKey"] = proxyKey;
            ret["id"] = lastInsertId;
            cb(ret);
        }

        return rc;
    } catch (exception &ex) {
        ErrorL << "创建 channel通道 失败" << ex.what();
    }
    return -1;
}

Json::Value searchChannels() {
    return searchChannels("", "", "", 1, 99999);
}

int countChannels(string searchText, string enableMp4, string active) {
    sqlite3pp::database db(dbpath.data());


    string baseQuery = "SELECT count(*) FROM CHANNEL ";
    string where = " where 1=1 ";
    string conditions = "";

    string query = baseQuery;

    if (!enableMp4.empty() && atoi(enableMp4.c_str())) {
        conditions += " and record_mp4 > 0 ";
    }
    if (!active.empty()) {
        conditions += " and active=:active ";
    }

    if (!searchText.empty()) {
        conditions += " and name like :searchText ";
    }
    if (!conditions.empty()) {
        query += where + conditions;
    }

    sqlite3pp::query qry(db, query.c_str());
    if (!searchText.empty()) {
        qry.bind(":searchText", "%" + searchText + "%", sqlite3pp::copy);
    }
    if (!active.empty()) {
        qry.bind(":active", atoi(active.c_str()));
    }

    sqlite3pp::query::iterator lastRowIdIter = qry.begin();
    int total;
    std::tie(total) = (*lastRowIdIter).get_columns<int>(0);

    return total;
}

Json::Value searchChannels(string searchText, string enableMp4, string active, int page, int pageSize) {
    sqlite3pp::database db(dbpath.data());

    string baseQuery = "SELECT ID, PROXY_KEY, NAME, VHOST, APP, STREAM, SOURCE_URL, FFMPEG_CMD, ENABLE_HLS, RECORD_MP4, RTSP_TRANSPORT, ON_DEMAND, ACTIVE FROM CHANNEL ";
    string where = " where 1=1 ";
    string conditions = "";

    string limit = " limit :pageSize offset :pageSize*(:page-1)";
    string order = " order by ID ";

    string query = baseQuery;

    if (!enableMp4.empty() && atoi(enableMp4.c_str())) {
        conditions += " and record_mp4 > 0 ";
    }
    if (!active.empty()) {
        conditions += " and active=:active ";
    }

    if (!searchText.empty()) {
        conditions += " and name like :searchText ";
    }
    if (!conditions.empty()) {
        query += where + conditions;
    }
    query += order;

    if (!(page == 1 && pageSize == 99999)) {
        query = query + limit;
    }

    sqlite3pp::query qry(db, query.c_str());


    if (!searchText.empty()) {
        qry.bind(":searchText", "%" + searchText + "%", sqlite3pp::copy);
    }
    if (!active.empty()) {
        qry.bind(":active", atoi(active.c_str()));

    }


    if (!(page == 1 && pageSize == 99999)) {
        qry.bind(":pageSize", pageSize);
        qry.bind(":page", page);
    }


    Json::Value ret;

    for (sqlite3pp::query::iterator i = qry.begin(); i != qry.end(); ++i) {
        int id, enable_hls, record_mp4, rtsp_transport, on_demand, active;
        std::string proxyKey, name, vhost, app, stream, source_url, ffmpeg_cmd;

        std::tie(id, proxyKey, name, vhost, app, stream, source_url, ffmpeg_cmd, enable_hls, record_mp4, rtsp_transport,
                 on_demand, active) =
                (*i).get_columns <
                int, char const*, char const*, char const*, char const*, char const*, char const*, char const*, int, int, int, int,
                int > (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
        //(*i).getter() >> sqlite3pp::ignore >> id >> name >>  vhost >> app >> stream >> source_url >> ffmpeg_cmd >>enable_hls >> record_mp4 >> rtsp_transport >> on_demand >> active;

        Json::Value rowRet;
        rowRet["id"] = id;
        rowRet["proxyKey"] = proxyKey;
        rowRet["vhost"] = vhost;
        rowRet["name"] = name;
        rowRet["app"] = app;
        rowRet["stream"] = stream;
        rowRet["source_url"] = source_url;
        rowRet["ffmpeg_cmd"] = ffmpeg_cmd;
        rowRet["enable_hls"] = enable_hls;
        rowRet["record_mp4"] = record_mp4;
        rowRet["rtsp_transport"] = rtsp_transport;
        rowRet["on_demand"] = on_demand;
        rowRet["active"] = active;

        ret.append(rowRet);
    }

    return ret;
}


Json::Value searchChannel(std::string vhost, std::string app, std::string stream) {
    return searchChannel(getProxyKey(vhost, app, stream));
}

Json::Value searchChannel(std::string proxyKey) {
    sqlite3pp::database db(dbpath.data());

    std::string query =
            "SELECT ID, PROXY_KEY, NAME, VHOST, APP, STREAM, SOURCE_URL, FFMPEG_CMD, ENABLE_HLS, RECORD_MP4, RTSP_TRANSPORT, ON_DEMAND, ACTIVE FROM CHANNEL WHERE PROXY_KEY=:proxyKey";
    sqlite3pp::query qry(db, query.c_str());
    qry.bind(":proxyKey", proxyKey, sqlite3pp::copy);


    Json::Value rowRet;

    for (sqlite3pp::query::iterator i = qry.begin(); i != qry.end(); ++i) {
        int id, enable_hls, record_mp4, rtsp_transport, on_demand, active;
        std::string proxyKey, name, vhost, app, stream, source_url, ffmpeg_cmd;

        std::tie(id, proxyKey, name, vhost, app, stream, source_url, ffmpeg_cmd, enable_hls, record_mp4, rtsp_transport,
                 on_demand, active) =
                (*i).get_columns <
                int, char const*, char const*, char const*, char const*, char const*, char const*, char const*, int, int, int, int,
                int > (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
        //(*i).getter() >> sqlite3pp::ignore >> id >> name >>  vhost >> app >> stream >> source_url >> ffmpeg_cmd >>enable_hls >> record_mp4 >> rtsp_transport >> on_demand >> active;

        rowRet["id"] = id;
        rowRet["proxyKey"] = proxyKey;
        rowRet["vhost"] = vhost;
        rowRet["name"] = name;
        rowRet["app"] = app;
        rowRet["stream"] = stream;
        rowRet["source_url"] = source_url;
        rowRet["ffmpeg_cmd"] = ffmpeg_cmd;
        rowRet["enable_hls"] = enable_hls;
        rowRet["record_mp4"] = record_mp4;
        rowRet["rtsp_transport"] = rtsp_transport;
        rowRet["on_demand"] = on_demand;
        rowRet["active"] = active;
        break;
    }

    return rowRet;
}


Json::Value searchChannel(int channelId) {
    sqlite3pp::database db(dbpath.data());

    std::string query =
            "SELECT ID, PROXY_KEY, NAME, VHOST, APP, STREAM, SOURCE_URL, FFMPEG_CMD, ENABLE_HLS, RECORD_MP4, RTSP_TRANSPORT, ON_DEMAND, ACTIVE FROM CHANNEL WHERE ID=:channelId";
    sqlite3pp::query qry(db, query.c_str());
    qry.bind(":channelId", channelId);


    Json::Value rowRet;

    for (sqlite3pp::query::iterator i = qry.begin(); i != qry.end(); ++i) {
        int id, enable_hls, record_mp4, rtsp_transport, on_demand, active;
        std::string proxyKey, name, vhost, app, stream, source_url, ffmpeg_cmd;

        std::tie(id, proxyKey, name, vhost, app, stream, source_url, ffmpeg_cmd, enable_hls, record_mp4, rtsp_transport,
                 on_demand, active) =
                (*i).get_columns <
                int, char const*, char const*, char const*, char const*, char const*, char const*, char const*, int, int, int, int,
                int > (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
        //(*i).getter() >> sqlite3pp::ignore >> id >> name >>  vhost >> app >> stream >> source_url >> ffmpeg_cmd >>enable_hls >> record_mp4 >> rtsp_transport >> on_demand >> active;

        rowRet["id"] = id;
        rowRet["proxyKey"] = proxyKey;
        rowRet["vhost"] = vhost;
        rowRet["name"] = name;
        rowRet["app"] = app;
        rowRet["stream"] = stream;
        rowRet["source_url"] = source_url;
        rowRet["ffmpeg_cmd"] = ffmpeg_cmd;
        rowRet["enable_hls"] = enable_hls;
        rowRet["record_mp4"] = record_mp4;
        rowRet["rtsp_transport"] = rtsp_transport;
        rowRet["on_demand"] = on_demand;
        rowRet["active"] = active;
        break;
    }

    return rowRet;
}
