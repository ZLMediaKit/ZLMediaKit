//
// Created by 陈磊 on 2019-06-24.
//
#ifndef KF_DBUTIL_H
#define KF_DBUTIL_H

#include <string>
#include "jsoncpp/json.h"
#include <functional>
#include <unordered_map>

typedef std::tuple<std::string,std::string,std::string> ChannelPropTuple;
typedef std::vector<ChannelPropTuple> ChannelPropTupleList;

ChannelPropTupleList getChannelPropTupleList();
std::unordered_map<std::string, ChannelPropTuple> getChannelPropsMap();
std::string channelsJsonToCsvStr(Json::Value channels);
Json::Value channelsCsvStrToJson(std::string channelsCsv);

extern void initDatabase(std::string dbpathParent);

extern int saveChannel(int channelId, Json::Value jsonArgs, std::function<void(bool isCreate,Json::Value originalChannel, Json::Value channel)> cb);

extern int createChannel(Json::Value jsonArgs, std::function<void(Json::Value channel)> cb);

extern int updateChannel(int channelId, Json::Value jsonArgs, std::function<void(Json::Value channel)> cb);

extern int deleteChannel(int channelId, std::function<void()> cb);

extern int countChannels(std::string searchText, std::string enableMp4,std::string active);

extern Json::Value searchChannels(std::string searchText,std::string enableMp4,std::string active, int page, int pageSize);

extern Json::Value searchChannels();

extern Json::Value searchChannel(std::string vhost,std::string app, std::string stream);

extern Json::Value searchChannel(std::string proxyKef);

extern Json::Value searchChannel(int channelId);

#endif //KF_DBUTIL_H