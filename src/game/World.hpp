#pragma once

#include "game/Machine.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace factory::game {

class World {
public:
    explicit World(int width = 400, int height = 250);

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] bool contains(int x, int y) const noexcept;
    [[nodiscard]] bool occupied(int x, int y) const noexcept;
    [[nodiscard]] MachineId machineAt(int x, int y) const noexcept;
    [[nodiscard]] const Machine* find(MachineId id) const noexcept;
    [[nodiscard]] const std::unordered_map<MachineId, Machine>& machines() const noexcept { return machines_; }
    [[nodiscard]] std::size_t machineCount() const noexcept { return machines_.size(); }
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }

    [[nodiscard]] std::optional<MachineId> place(int x, int y, MachineType type);
    bool remove(int x, int y);
    bool remove(MachineId id);
    void clear();
    std::size_t populateBenchmark(std::size_t count);

private:
    [[nodiscard]] std::size_t index(int x, int y) const noexcept;

    int width_{};
    int height_{};
    MachineId nextId_{1};
    std::uint64_t revision_{};
    std::vector<MachineId> tiles_;
    std::unordered_map<MachineId, Machine> machines_;
};

} // namespace factory::game
