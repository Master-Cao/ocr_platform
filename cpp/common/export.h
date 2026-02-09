/**
 * @file export.h
 * @brief 统一符号导出宏，用于 Linux .so 与 Windows .dll
 */
#ifndef OCRDETECT_COMMON_EXPORT_H
#define OCRDETECT_COMMON_EXPORT_H

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#  ifdef OCRDETECT_EXPORTS
#    define OCRDETECT_API __declspec(dllexport)
#  else
#    define OCRDETECT_API __declspec(dllimport)
#  endif
#  define OCRDETECT_CALL __stdcall
#else
#  if __GNUC__ >= 4
#    define OCRDETECT_API __attribute__((visibility("default")))
#  else
#    define OCRDETECT_API
#  endif
#  define OCRDETECT_CALL
#endif

#endif /* OCRDETECT_COMMON_EXPORT_H */
