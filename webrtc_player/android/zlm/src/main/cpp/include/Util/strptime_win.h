#ifndef ZLMEDIAKIT_STRPTIME_WIN_H
#define ZLMEDIAKIT_STRPTIME_WIN_H

#include <ctime>
#ifdef _WIN32
//window上自己实现strptime函数，linux已经提供strptime
//strptime函数windows平台上实现
char * strptime(const char *buf, const char *fmt, struct tm *tm);
#endif
#endif //ZLMEDIAKIT_STRPTIME_WIN_H
