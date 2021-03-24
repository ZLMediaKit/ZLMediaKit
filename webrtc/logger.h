#pragma once
#include <stdio.h>
#include <assert.h>

#define ELOG_DEBUG(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#define ELOG_WARN(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)

#define MS_TRACE()
#define MS_ERROR(fmt, ...) printf("error:" fmt "\n", ##__VA_ARGS__)
#define MS_THROW_ERROR(fmt, ...) do{ printf("throw:" fmt "\n", ##__VA_ARGS__); throw std::runtime_error("error"); } while(false);
#define MS_DUMP(fmt, ...) printf("dump:" fmt "\n", ##__VA_ARGS__)
#define MS_DEBUG_2TAGS(tag1, tag2,fmt, ...) printf("debug:" fmt "\n", ##__VA_ARGS__)
#define MS_WARN_2TAGS(tag1, tag2,fmt, ...) printf("warn:" fmt "\n", ##__VA_ARGS__)
#define MS_DEBUG_TAG(tag,fmt, ...) printf("debug:" fmt "\n", ##__VA_ARGS__)
#define MS_ASSERT(con, log) assert(con)
#define MS_ABORT(fmt, ...) do{ printf("abort:" fmt "\n", ##__VA_ARGS__); abort(); } while(false);
#define MS_WARN_TAG(tag,fmt, ...) printf("warn:" fmt "\n", ##__VA_ARGS__)
#define MS_DEBUG_DEV(fmt, ...) printf("debug:" fmt "\n", ##__VA_ARGS__)