#ifndef WFA_PROJECT_STATUS_HPP
#define WFA_PROJECT_STATUS_HPP

#include "wfa/checkpoint.hpp"
#include "wfa/package_layout.hpp"

#include <span>
#include <string>
#include <vector>

namespace wfa {

struct PhaseStatus {
  std::string id;
  std::string name;
  int progress;
  std::string summary;
};

std::vector<PhaseStatus> BuildDefaultPhases();
std::vector<Checkpoint> BuildDefaultCheckpoints();
int CalculateAveragePhaseProgress(std::span<const PhaseStatus> phases);
std::string RenderProjectStatusReport();
std::string RenderPackageLayoutReport(const PackageLayout& layout);
std::string DescribeMvpFoundation();

}  // namespace wfa

#endif  // WFA_PROJECT_STATUS_HPP
