#ifndef OCR_SERVER_BASE64_H
#define OCR_SERVER_BASE64_H

#include <vector>
#include <cstdint>
#include <string>

namespace server {

/** Base64 解码，返回二进制数据；失败返回空 vector。支持去掉 data URL 前缀。 */
std::vector<uint8_t> base64_decode(const std::string& in);

}  // namespace server

#endif
