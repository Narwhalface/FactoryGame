#pragma once

#include <cstdint>
#include <string_view>

namespace factory::game {

using MachineId = std::uint64_t;

enum class MachineType : std::uint8_t {
    Furnace,
    Crusher,
    ChemicalReactor,
    Generator,
};

struct Machine {
    MachineId id{};
    MachineType type{MachineType::Furnace};
    int x{};
    int y{};
    bool operational{true};
};

[[nodiscard]] constexpr std::string_view machineTypeName(MachineType type) noexcept {
    switch (type) {
    case MachineType::Furnace: return "Furnace";
    case MachineType::Crusher: return "Crusher";
    case MachineType::ChemicalReactor: return "Chemical Reactor";
    case MachineType::Generator: return "Generator";
    }
    return "Unknown";
}

} // namespace factory::game
