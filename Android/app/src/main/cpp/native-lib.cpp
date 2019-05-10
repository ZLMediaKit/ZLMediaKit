#include <jni.h>
#include <string>
#include "test_server.cpp"

#define JNI_API(retType,funName,...) extern "C"  JNIEXPORT retType Java_com_zlmediakit_jni_ZLMediaKit_##funName(JNIEnv* env, jclass cls,##__VA_ARGS__)


string stringFromJstring(JNIEnv *env,jstring jstr){
    if(!env || !jstr){
        WarnL << "invalid args";
        return "";
    }
    const char *field_char = env->GetStringUTFChars(jstr, 0);
    string ret(field_char,env->GetStringUTFLength(jstr));
    env->ReleaseStringUTFChars(jstr, field_char);
    return ret;
}
string stringFromJbytes(JNIEnv *env,jbyteArray jbytes){
    if(!env || !jbytes){
        WarnL << "invalid args";
        return "";
    }
    jbyte *bytes = env->GetByteArrayElements(jbytes, 0);
    string ret((char *)bytes,env->GetArrayLength(jbytes));
    env->ReleaseByteArrayElements(jbytes,bytes,0);
    return ret;
}
string stringFieldFromJava(JNIEnv *env,  jobject jdata,jfieldID jid){
    if(!env || !jdata || !jid){
        WarnL << "invalid args";
        return "";
    }
    jstring field_str = (jstring)env->GetObjectField(jdata,jid);
    auto ret = stringFromJstring(env,field_str);
    env->DeleteLocalRef(field_str);
    return ret;
}

string bytesFieldFromJava(JNIEnv *env,  jobject jdata,jfieldID jid){
    if(!env || !jdata || !jid){
        WarnL << "invalid args";
        return "";
    }
    jbyteArray jbufArray = (jbyteArray)env->GetObjectField(jdata, jid);
    string ret = stringFromJbytes(env,jbufArray);
    env->DeleteLocalRef(jbufArray);
    return ret;
}

jstring jstringFromString(JNIEnv* env, const char* pat) {
    return (jstring)env->NewStringUTF(pat);
}

jbyteArray jbyteArrayFromString(JNIEnv* env, const char* pat,int len = 0){
    if(len <= 0){
        len = strlen(pat);
    }
    jbyteArray jarray = env->NewByteArray(len);
    env->SetByteArrayRegion(jarray, 0, len, (jbyte *)(pat));
    return jarray;
}

/*
 * 加载动态库
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    //设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
    InfoL;
    return JNI_VERSION_1_6;
}
/*
 * 卸载动态库
 */
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved){
    InfoL;
    s_sem.post();
}


JNI_API(jboolean,startDemo,jstring path_to_jni_file){
    string sd_path = stringFromJstring(env,path_to_jni_file);
    string jni_file = sd_path +  "/zlmediakit.jni";

    DebugL << "sd_path:" << sd_path;
    DebugL << "jni file:" << sd_path;

    static thread s_th([sd_path,jni_file](){
        try {
            mINI::Instance()[Http::kRootPath] = mINI::Instance()[Hls::kFilePath] = sd_path + "/httpRoot";
            do_main(jni_file);
        }catch (std::exception &ex){
            WarnL << ex.what();
        }
    });

    static onceToken s_token([]{
        s_th.detach();
    });
    return true;
};
