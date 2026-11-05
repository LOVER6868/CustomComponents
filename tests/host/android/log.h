#pragma once
#define ANDROID_LOG_INFO 4
#define ANDROID_LOG_WARN 5
#define ANDROID_LOG_ERROR 6
#define ANDROID_LOG_DEBUG 3
#define ANDROID_LOG_VERBOSE 2
#ifdef __cplusplus
extern "C" {
#endif
int __android_log_print(int,const char*,const char*,...);
#ifdef __cplusplus
}
#endif
