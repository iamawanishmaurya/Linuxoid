#include "wfa/checkpoint.hpp"

#include <algorithm>
#include <cmath>

namespace wfa {

namespace {

int ClampProgress(int progress) {
  return std::clamp(progress, 0, 100);
}

int ToPercent(CompletionState state) {
  return static_cast<int>(state);
}

}  // namespace

int CalculateWeightedCheckpointProgress(std::span<const Checkpoint> checkpoints) {
  int weight_total = 0;
  double weighted_total = 0.0;

  for (const auto& checkpoint : checkpoints) {
    weight_total += checkpoint.weight;
    weighted_total += checkpoint.weight * (ToPercent(checkpoint.state) / 100.0);
  }

  if (weight_total == 0) {
    return 0;
  }

  return static_cast<int>(std::lround((weighted_total / weight_total) * 100.0));
}

int CountCompletedCheckpoints(std::span<const Checkpoint> checkpoints) {
  int count = 0;
  for (const auto& checkpoint : checkpoints) {
    if (checkpoint.state == CompletionState::kComplete) {
      ++count;
    }
  }
  return count;
}

std::string RenderLoadingBar(int progress, std::size_t width) {
  const int clamped_progress = ClampProgress(progress);
  const auto filled = static_cast<std::size_t>(
      std::lround((clamped_progress / 100.0) * static_cast<double>(width)));

  std::string bar;
  bar.reserve(width + 12);
  bar.push_back('[');
  bar.append(filled, '#');
  bar.append(width - filled, '-');
  bar.push_back(']');
  bar.push_back(' ');
  bar.append(std::to_string(clamped_progress));
  bar.append("/100");
  return bar;
}

}  // namespace wfa
