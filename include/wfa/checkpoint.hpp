#ifndef WFA_CHECKPOINT_HPP
#define WFA_CHECKPOINT_HPP

#include <cstddef>
#include <span>
#include <string>

namespace wfa {

enum class CompletionState {
  kNotStarted = 0,
  kInValidation = 50,
  kComplete = 100,
};

struct Checkpoint {
  std::string id;
  std::string name;
  int weight;
  CompletionState state;
  std::string evidence;
};

int CalculateWeightedCheckpointProgress(std::span<const Checkpoint> checkpoints);
int CountCompletedCheckpoints(std::span<const Checkpoint> checkpoints);
std::string RenderLoadingBar(int progress, std::size_t width = 20);

}  // namespace wfa

#endif  // WFA_CHECKPOINT_HPP
