#include "wfa/asset_manager_stub.hpp"

#include <iostream>
#include <string>

namespace wfa {

struct AAssetManagerStub {
  std::string apk_path;
};

AAssetManager* MakeStubAssetManager(const std::string& apk_path) {
  auto* manager = new AAssetManagerStub{apk_path};
  std::cout << "[asset-stub] manager created for: " << apk_path << "\n";
  return manager;
}

}  // namespace wfa
