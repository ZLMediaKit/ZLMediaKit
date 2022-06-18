#pragma once
#include "Util/logger.h"

#define MS_TRACE()
#define MS_ERROR PrintE
#define MS_THROW_ERROR(...) do { PrintE(__VA_ARGS__); throw std::runtime_error("MS_THROW_ERROR"); } while(false)
#define MS_DUMP PrintT
#define MS_DEBUG_2TAGS(tag1, tag2, ...) PrintD(__VA_ARGS__)
#define MS_WARN_2TAGS(tag1, tag2, ...) PrintW(__VA_ARGS__)
#define MS_DEBUG_TAG(tag, ...) PrintD(__VA_ARGS__)
#define MS_ASSERT(con, ...) do { if(!(con)) { PrintE(__VA_ARGS__); std::runtime_error("MS_ASSERT"); } } while(false)
#define MS_ABORT(...) do { PrintE(__VA_ARGS__); abort(); } while(false)
#define MS_WARN_TAG(tag, ...) PrintW(__VA_ARGS__)
#define MS_DEBUG_DEV PrintD