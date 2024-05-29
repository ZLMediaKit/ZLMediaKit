#include <jni.h>
#include <string>
#include <Http/HttpRequester.h>
#include "Http/HttpClient.h"


using namespace toolkit;
using namespace mediakit;
using namespace std;

extern "C" JNIEXPORT jstring JNICALL
Java_com_zlm_rtc_NativeLib_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}
extern "C"
JNIEXPORT jstring JNICALL
Java_com_zlm_rtc_NativeLib_exchangeSessionDescription(JNIEnv *env, jobject thiz,
                                                      jstring description) {
    static semaphore sem;

    //加载证书，证书包含公钥和私钥
    SSL_Initor::Instance().loadCertificate((exeDir() + "ssl.p12").data());
    //信任某个自签名证书
    SSL_Initor::Instance().trustCertificate((exeDir() + "ssl.p12").data());
    //不忽略无效证书证书(例如自签名或过期证书)
    SSL_Initor::Instance().ignoreInvalidCertificate(false);

    //创建一个Http请求器
    HttpRequester::Ptr requesterUploader(new HttpRequester());
    //使用POST方式请求
    requesterUploader->setMethod("POST");
    //设置http请求头
    HttpArgs argsUploader;
    argsUploader["query"] = "test";

    static string boundary = "0xKhTmLbOuNdArY";
    HttpMultiFormBody::Ptr body(new HttpMultiFormBody(argsUploader, exePath(), boundary));
    requesterUploader->setBody(body);
    requesterUploader->addHeader("Content-Type", HttpMultiFormBody::multiFormContentType(boundary));
    //开启请求
    requesterUploader->startRequester("https://zlmediakit.com/index/api/webrtc?app=live&stream=test&type=play",//url地址
                                      [](const SockException &ex,                          //网络相关的失败信息，如果为空就代表成功
                                         const Parser &parser) {                       //http回复body
                                          DebugL << "=====================HttpRequester Uploader==========================";
                                          if (ex) {
                                              //网络相关的错误
                                              WarnL << "network err:" << ex.getErrCode() << " " << ex.what();
                                          } else {
                                              //打印http回复信息
                                              _StrPrinter printer;
                                              for (auto &pr: parser.getHeader()) {
                                                  printer << pr.first << ":" << pr.second << "\r\n";
                                              }
                                              InfoL << "status:" << parser.status() << "\r\n"
                                                    << "header:\r\n" << (printer << endl)
                                                    << "\r\nbody:" << parser.content();
                                          }
                                      });

    sem.wait();
}
extern "C"
JNIEXPORT jstring JNICALL
Java_com_zlm_rtc_NativeLib_makePlayUrl(JNIEnv *env, jobject thiz, jstring app, jstring stream_id) {
    const char *appString = env->GetStringUTFChars(app, 0);
    const char *streamIdString = env->GetStringUTFChars(stream_id, 0);
    char url[100];
    sprintf(url,"https://zlmediakit.com/index/api/webrtc?app=%s&stream=%s&type=play",appString,streamIdString);
    return env->NewStringUTF(url);
}


extern "C"
JNIEXPORT jstring JNICALL
Java_com_zlm_rtc_NativeLib_makePushUrl(JNIEnv *env, jobject thiz, jstring app, jstring stream_id) {
    const char *appString = env->GetStringUTFChars(app, 0);
    const char *streamIdString = env->GetStringUTFChars(stream_id, 0);
    char url[100];
    sprintf(url,"https://zlmediakit.com/index/api/webrtc?app=%s&stream=%s&type=push",appString,streamIdString);
    return env->NewStringUTF(url);
}