extern "C"
#if defined(__GNUC__)
__attribute__((weak))
#endif
int __android_log_print(int,const char*,const char*,...){return 0;}
