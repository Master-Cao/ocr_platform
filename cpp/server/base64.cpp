#include "base64.h"
#include <stdexcept>
#include <algorithm>

namespace {

static const char kBase64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

inline int decode_char(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

}  // namespace

namespace server {

std::vector<uint8_t> base64_decode(const std::string& in) {
  std::string s = in;
  auto pos = s.find(',');
  if (pos != std::string::npos)
    s = s.substr(pos + 1);
  while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' '))
    s.pop_back();
  size_t len = s.size();
  if (len % 4 != 0)
    return {};
  size_t out_len = (len / 4) * 3;
  if (len >= 2) {
    if (s[len - 1] == '=') { out_len--; if (s[len - 2] == '=') out_len--; }
  }
  std::vector<uint8_t> out;
  out.reserve(out_len);
  for (size_t i = 0; i + 4 <= len; i += 4) {
    bool pad2 = (s[i + 2] == '=');
    bool pad1 = (s[i + 3] == '=');
    int a = decode_char(s[i]);
    int b = decode_char(s[i + 1]);
    int c = pad2 ? 0 : decode_char(s[i + 2]);
    int d = pad1 ? 0 : decode_char(s[i + 3]);
    if (a < 0 || b < 0 || (!pad2 && c < 0) || (!pad1 && d < 0)) return {};
    uint32_t v = (a << 18) | (b << 12) | (c << 6) | d;
    out.push_back(static_cast<uint8_t>(v >> 16));
    if (out.size() < out_len) out.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
    if (out.size() < out_len) out.push_back(static_cast<uint8_t>(v & 0xff));
  }
  return out;
}

}  // namespace server
