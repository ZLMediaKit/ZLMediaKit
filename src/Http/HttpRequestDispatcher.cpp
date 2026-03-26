#include "HttpRequestDispatcher.h"

using namespace toolkit;

namespace mediakit {

bool HttpRequestDispatcher::emitHttpEvent(const Parser &parser, toolkit::SockInfo &sender, const HttpResponseInvokerImp &invoker, bool doInvoke) {
    bool consumed = false;
    NOTICE_EMIT(BroadcastHttpRequestArgs, Broadcast::kBroadcastHttpRequest, parser, invoker, consumed, sender);
    if (consumed == false && doInvoke) {
        invoker(404, StrCaseMap(), HttpBody::Ptr());
    }
    return consumed;
}

void HttpRequestDispatcher::onAccessPath(toolkit::Session &sender, Parser &parser, const HttpFileManager::invoker &cb) {
    HttpFileManager::onAccessPath(sender, parser, cb);
}

void HttpRequestDispatcher::onAccessPath(toolkit::SockInfo &sender, Parser &parser,
                                         const HttpFileManager::invoker &cb, toolkit::Session *session) {
    HttpFileManager::onAccessPath(sender, parser, cb, session);
}

} // namespace mediakit
