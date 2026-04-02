#ifndef ZLMEDIAKIT_HTTPSERVERTYPES_H
#define ZLMEDIAKIT_HTTPSERVERTYPES_H

#include <functional>
#include "HttpBody.h"
#include "Common/Parser.h"
#include "Util/function_traits.h"

namespace mediakit {

class HttpResponseInvokerImp {
public:
    using HttpResponseBodyInvoker = std::function<void(int code, const StrCaseMap &headerOut, const HttpBody::Ptr &body)>;
    using HttpResponseStringInvoker = std::function<void(int code, const StrCaseMap &headerOut, const std::string &body)>;

    HttpResponseInvokerImp() = default;

    template<typename C>
    HttpResponseInvokerImp(const C &c) : HttpResponseInvokerImp(typename toolkit::function_traits<C>::stl_function_type(c)) {}

    HttpResponseInvokerImp(const HttpResponseBodyInvoker &invoker);
    HttpResponseInvokerImp(const HttpResponseStringInvoker &invoker);

    void operator()(int code, const StrCaseMap &headerOut, const toolkit::Buffer::Ptr &body) const;
    void operator()(int code, const StrCaseMap &headerOut, const HttpBody::Ptr &body) const;
    void operator()(int code, const StrCaseMap &headerOut, const std::string &body) const;

    void responseFile(const StrCaseMap &requestHeader, const StrCaseMap &responseHeader, const std::string &file, bool use_mmap = true, bool is_path = true) const;
    explicit operator bool() const;

private:
    HttpResponseBodyInvoker _response_body_invoker;
};

using HttpAccessPathInvoker = std::function<void(const std::string &errMsg, const std::string &accessPath, int cookieLifeSecond)>;

} // namespace mediakit

#endif // ZLMEDIAKIT_HTTPSERVERTYPES_H
