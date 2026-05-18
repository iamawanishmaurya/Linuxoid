#ifndef WFA_ANDROID_BINARY_XML_HPP
#define WFA_ANDROID_BINARY_XML_HPP

#include <string>
#include <string_view>
#include <vector>

namespace wfa {

struct AndroidBinaryXmlDecodeResult {
  bool success = false;
  std::string xml_text;
  std::vector<std::string> errors;
};

bool LooksLikeAndroidBinaryXml(std::string_view bytes);
AndroidBinaryXmlDecodeResult DecodeAndroidBinaryXmlToText(
    std::string_view bytes);

}  // namespace wfa

#endif  // WFA_ANDROID_BINARY_XML_HPP
