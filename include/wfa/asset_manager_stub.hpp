#ifndef WFA_ASSET_MANAGER_STUB_HPP
#define WFA_ASSET_MANAGER_STUB_HPP

#include "wfa/native_types.hpp"

#include <string>

namespace wfa {

AAssetManager* MakeStubAssetManager(const std::string& apk_path);

}  // namespace wfa

#endif  // WFA_ASSET_MANAGER_STUB_HPP
