#include "game/World.hpp"

#include <algorithm>
#include <stdexcept>

namespace factory::game {

World::World(int width, int height) : width_(width), height_(height) {
    if (width <= 0 || height <= 0) throw std::invalid_argument("World dimensions must be positive");
    tiles_.resize(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_));
    machines_.reserve(tiles_.size());
}

bool World::contains(int x, int y) const noexcept {
    return x >= 0 && y >= 0 && x < width_ && y < height_;
}

std::size_t World::index(int x, int y) const noexcept {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x);
}

bool World::occupied(int x, int y) const noexcept {
    return contains(x, y) && tiles_[index(x, y)] != 0;
}

MachineId World::machineAt(int x, int y) const noexcept {
    return contains(x, y) ? tiles_[index(x, y)] : 0;
}

const Machine* World::find(MachineId id) const noexcept {
    const auto it = machines_.find(id);
    return it == machines_.end() ? nullptr : &it->second;
}

std::optional<MachineId> World::place(int x, int y, MachineType type) {
    if (!contains(x, y) || occupied(x, y)) return std::nullopt;
    const MachineId id = nextId_++;
    machines_.emplace(id, Machine{id, type, x, y, true});
    tiles_[index(x, y)] = id;
    ++revision_;
    return id;
}

bool World::remove(int x, int y) {
    if (!contains(x, y)) return false;
    const MachineId id = tiles_[index(x, y)];
    if (id == 0) return false;
    tiles_[index(x, y)] = 0;
    machines_.erase(id);
    ++revision_;
    return true;
}

bool World::remove(MachineId id) {
    const auto it = machines_.find(id);
    if (it == machines_.end()) return false;
    tiles_[index(it->second.x, it->second.y)] = 0;
    machines_.erase(it);
    ++revision_;
    return true;
}

void World::clear() {
    if (machines_.empty()) return;
    std::fill(tiles_.begin(), tiles_.end(), MachineId{});
    machines_.clear();
    ++revision_;
}

std::size_t World::populateBenchmark(std::size_t count) {
    clear();
    count = std::min(count, tiles_.size());
    for (std::size_t i = 0; i < count; ++i) {
        const int x = static_cast<int>(i % static_cast<std::size_t>(width_));
        const int y = static_cast<int>(i / static_cast<std::size_t>(width_));
        const auto type = static_cast<MachineType>((x * 17 + y * 31) % 4);
        (void)place(x, y, type);
    }
    return count;
}

} // namespace factory::game
