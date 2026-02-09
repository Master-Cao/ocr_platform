/** 模板匹配库符号导出（.so / .dll） */
#ifndef TEMPLATEMATCH_EXPORT_H
#define TEMPLATEMATCH_EXPORT_H

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#  ifdef TEMPLATEMATCH_EXPORTS
#    define TEMPLATEMATCH_API __declspec(dllexport)
#  else
#    define TEMPLATEMATCH_API __declspec(dllimport)
#  endif
#  define TEMPLATEMATCH_CALL __stdcall
#else
#  if __GNUC__ >= 4
#    define TEMPLATEMATCH_API __attribute__((visibility("default")))
#  else
#    define TEMPLATEMATCH_API
#  endif
#  define TEMPLATEMATCH_CALL
#endif

#endif
