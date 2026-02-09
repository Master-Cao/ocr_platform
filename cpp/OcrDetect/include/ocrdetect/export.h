/** OCR 检测库符号导出 */
#ifndef OCRDETECT_OCR_EXPORT_H
#define OCRDETECT_OCR_EXPORT_H

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
#  ifdef OCRDETECT_OCR_EXPORTS
#    define OCRDETECT_OCR_API __declspec(dllexport)
#  else
#    define OCRDETECT_OCR_API __declspec(dllimport)
#  endif
#  define OCRDETECT_OCR_CALL __stdcall
#else
#  if __GNUC__ >= 4
#    define OCRDETECT_OCR_API __attribute__((visibility("default")))
#  else
#    define OCRDETECT_OCR_API
#  endif
#  define OCRDETECT_OCR_CALL
#endif

#endif
