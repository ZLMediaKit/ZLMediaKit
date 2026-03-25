#ifndef ZLMEDIAKIT_HTTPSERVERTYPES_H
#define ZLMEDIAKIT_HTTPSERVERTYPES_H

#include <functional>
#include "HttpBody.h"
#include "Common/Parser.h"
#include "Util/function_traits.h"

namespace mediakit {

class HttpResponseInvokerImp {
public:
    typedef std::function<void(int code, const StrCaseMap &headerOut, const HttpBody::Ptr &body)> HttpResponseInvokerLambda0;
    typedef std::function<void(int code, const StrCaseMap &headerOut, const std::string &body)> HttpResponseInvokerLambda1;

    HttpResponseInvokerImp() = default;

    template<typename C>
    HttpResponseInvokerImp(const C &c) : HttpResponseInvokerImp(typename toolkit::function_traits<C>::stl_function_type(c)) {}

    HttpResponseInvokerImp(const HttpResponseInvokerLambda0 &lambda);
    HttpResponseInvokerImp(const HttpResponseInvokerLambda1 &lambda);

    void operator()(int code, const StrCaseMap &headerOut, const toolkit::Buffer::Ptr &body) const;
    void operator()(int code, const StrCaseMap &headerOut, const HttpBody::Ptr &body) const;
    void operator()(int code, const StrCaseMap &headerOut, const std::string &body) const;

    void responseFile(const StrCaseMap &requestHeader, const StrCaseMap &responseHeader, const std::string &file, bool use_mmap = true, bool is_path = true) const;
    operator bool();

private:
    HttpResponseInvokerLambda0 _lambad;
};

using HttpAccessPathInvoker = std::function<void(const std::string &errMsg, const std::string &accessPath, int cookieLifeSecond)>;

} // namespace mediakit

#endif // ZLMEDIAKIT_HTTPSERVERTYPES_H
