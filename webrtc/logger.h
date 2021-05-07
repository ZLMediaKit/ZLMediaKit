#pragma once
#include <stdio.h>
#include <assert.h>

#if 0
#define MS_TRACE()
#define MS_ERROR(fmt, ...) printf("error:" fmt "\n", ##__VA_ARGS__)
#define MS_THROW_ERROR(fmt, ...) do{ printf("throw:" fmt "\n", ##__VA_ARGS__); throw std::runtime_error("error"); } while(false);
#define MS_DUMP(fmt, ...) printf("dump:" fmt "\n", ##__VA_ARGS__)
#define MS_DEBUG_2TAGS(tag1, tag2,fmt, ...) printf("debug:" fmt "\n", ##__VA_ARGS__)
#define MS_WARN_2TAGS(tag1, tag2,fmt, ...) printf("warn:" fmt "\n", ##__VA_ARGS__)
#define MS_DEBUG_TAG(tag,fmt, ...) printf("debug:" fmt "\n", ##__VA_ARGS__)
#define MS_ASSERT(con, fmt, ...) do{if(!(con)) { printf("assert failed:%s" fmt "\n", #con, ##__VA_ARGS__);} assert(con); } while(false);
#define MS_ABORT(fmt, ...) do{ printf("abort:" fmt "\n", ##__VA_ARGS__); abort(); } while(false);
#define MS_WARN_TAG(tag,fmt, ...) printf("warn:" fmt "\n", ##__VA_ARGS__)
#define MS_DEBUG_DEV(fmt, ...) printf("debug:" fmt "\n", ##__VA_ARGS__)
#else
#define MS_TRACE()
#define MS_ERROR(fmt, ...)
#define MS_THROW_ERROR(fmt, ...)
#define MS_DUMP(fmt, ...)
#define MS_DEBUG_2TAGS(tag1, tag2,fmt, ...)
#define MS_WARN_2TAGS(tag1, tag2,fmt, ...)
#define MS_DEBUG_TAG(tag,fmt, ...)
#define MS_ASSERT(con, fmt, ...)
#define MS_ABORT(fmt, ...)
#define MS_WARN_TAG(tag,fmt, ...)
#define MS_DEBUG_DEV(fmt, ...)
#endif