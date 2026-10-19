// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
namespace app_title {
bool relocate(const std::uint8_t* code, std::size_t available,
              std::uintptr_t source, std::uintptr_t destination,
              std::uint8_t* output, std::size_t capacity,
              std::size_t& consumed, std::size_t& written,
              std::size_t minimum=14);
void jump(std::uint8_t* out,std::uintptr_t destination);
bool relative_jump(std::uint8_t* out,std::uintptr_t source,std::uintptr_t destination);
}
