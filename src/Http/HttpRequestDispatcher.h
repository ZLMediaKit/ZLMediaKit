#ifndef SRC_HTTP_HTTPREQUESTDISPATCHER_H_
#define SRC_HTTP_HTTPREQUESTDISPATCHER_H_

#include "HttpFileManager.h"
#include "Common/config.h"

namespace mediakit {

class HttpRequestDispatcher {
public:
    static bool emitHttpEvent(const Parser &parser, toolkit::SockInfo &sender, const HttpResponseInvokerImp &invoker, bool doInvoke);
    static void onAccessPath(toolkit::Session &sender, Parser &parser, const HttpFileManager::invoker &cb);
    static void onAccessPath(toolkit::SockInfo &sender, Parser &parser, const HttpFileManager::invoker &cb, toolkit::Session *session);

private:
    HttpRequestDispatcher() = delete;
    ~HttpRequestDispatcher() = delete;
};

} // namespace mediakit

#endif /* SRC_HTTP_HTTPREQUESTDISPATCHER_H_ */
