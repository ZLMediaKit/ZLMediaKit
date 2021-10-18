#pragma once
#include "Util/logger.h"
using namespace toolkit;

#define MS_TRACE()
#define MS_ERROR PrintE
#define MS_THROW_ERROR(fmt, ...) do { PrintE(fmt, ##__VA_ARGS__); throw std::runtime_error("error"); } while(false)
#define MS_DUMP PrintT
#define MS_DEBUG_2TAGS(tag1, tag2, fmt, ...) PrintD(fmt, ##__VA_ARGS__)
#define MS_WARN_2TAGS(tag1, tag2, fmt, ...) PrintW(fmt, ##__VA_ARGS__)
#define MS_DEBUG_TAG(tag, fmt, ...) PrintD(fmt, ##__VA_ARGS__)
#define MS_ASSERT(con, fmt, ...) do { if(!(con)) { PrintE(fmt, ##__VA_ARGS__); abort(); } } while(false)
#define MS_ABORT(fmt, ...) do { PrintE(fmt, ##__VA_ARGS__); abort(); } while(false)
#define MS_WARN_TAG(tag, fmt, ...) PrintW(fmt, ##__VA_ARGS__)
#define MS_DEBUG_DEV PrintD