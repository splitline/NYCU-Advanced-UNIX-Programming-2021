typedef int fd_int;
typedef int open_flag;
typedef const char *path_str;

#define HOOK0(RetType, FunctionName)                               \
    RetType FunctionName() {                                       \
        RetType ret = call_libc_func(FunctionName, #FunctionName); \
        logger::start_log(#FunctionName);                          \
        logger::log_return(#RetType, ret);                         \
        return ret;                                                \
    }

#define HOOK1(RetType, FunctionName, type1, arg1)                        \
    RetType FunctionName(type1 arg1) {                                   \
        logger::start_log(#FunctionName);                                \
        logger::log_argument(#type1, arg1, true);                        \
        /* (f)close, remove need to resolve path arg before call */      \
        RetType ret = call_libc_func(FunctionName, #FunctionName, arg1); \
        logger::log_return(#RetType, ret);                               \
        return ret;                                                      \
    }

#define HOOK2(RetType, FunctionName, type1, arg1, type2, arg2)             \
    RetType FunctionName(type1 arg1, type2 arg2) {                         \
        RetType ret;                                                       \
        if (#FunctionName != "rename")                                     \
            ret = call_libc_func(FunctionName, #FunctionName, arg1, arg2); \
        logger::start_log(#FunctionName);                                  \
        logger::log_argument(#type1, arg1);                                \
        if (#FunctionName == "rename")                                     \
            ret = call_libc_func(FunctionName, #FunctionName, arg1, arg2); \
        logger::log_argument(#type2, arg2, true);                          \
        logger::log_return(#RetType, ret);                                 \
        return ret;                                                        \
    }

#define HOOK3(RetType, FunctionName, type1, arg1, type2, arg2, type3, arg3)          \
    RetType FunctionName(type1 arg1, type2 arg2, type3 arg3) {                       \
        RetType ret = call_libc_func(FunctionName, #FunctionName, arg1, arg2, arg3); \
        logger::start_log(#FunctionName);                                            \
        logger::log_argument(#type1, arg1);                                          \
        logger::log_argument(#type2, arg2);                                          \
        logger::log_argument(#type3, arg3, true);                                    \
        logger::log_return(#RetType, ret);                                           \
        return ret;                                                                  \
    }

#define HOOK4(RetType, FunctionName,                                       \
              type1, arg1, type2, arg2, type3, arg3, type4, arg4)          \
    RetType FunctionName(type1 arg1, type2 arg2, type3 arg3, type4 arg4) { \
        RetType ret = call_libc_func(FunctionName, #FunctionName,          \
                                     arg1, arg2, arg3, arg4);              \
        /* keep `fwrite` from infinite recursive */                        \
        if (logger::logging) {                                             \
            logger::start_log(#FunctionName);                              \
            logger::log_argument(#type1, arg1);                            \
            logger::log_argument(#type2, arg2);                            \
            logger::log_argument(#type3, arg3);                            \
            logger::log_argument(#type4, arg4, true);                      \
            logger::log_return(#RetType, ret);                             \
        }                                                                  \
        return ret;                                                        \
    }

template <class FuncType, class... ArgsType>
auto call_libc_func(FuncType func, const char *symbol, ArgsType... args);