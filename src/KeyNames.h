#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

namespace WarmHands
{
	using KeyName = std::pair<std::string_view, std::uint32_t>;

	inline constexpr std::array<KeyName, 53> kKeyboardKeys{ {
		{ "Q", 0x10 },
		{ "W", 0x11 },
		{ "E", 0x12 },
		{ "R", 0x13 },
		{ "T", 0x14 },
		{ "Y", 0x15 },
		{ "U", 0x16 },
		{ "I", 0x17 },
		{ "O", 0x18 },
		{ "P", 0x19 },
		{ "A", 0x1E },
		{ "S", 0x1F },
		{ "D", 0x20 },
		{ "F", 0x21 },
		{ "G", 0x22 },
		{ "H", 0x23 },
		{ "J", 0x24 },
		{ "K", 0x25 },
		{ "L", 0x26 },
		{ "Z", 0x2C },
		{ "X", 0x2D },
		{ "C", 0x2E },
		{ "V", 0x2F },
		{ "B", 0x30 },
		{ "N", 0x31 },
		{ "M", 0x32 },
		{ "1", 0x02 },
		{ "2", 0x03 },
		{ "3", 0x04 },
		{ "4", 0x05 },
		{ "5", 0x06 },
		{ "6", 0x07 },
		{ "7", 0x08 },
		{ "8", 0x09 },
		{ "9", 0x0A },
		{ "0", 0x0B },
		{ "F1", 0x3B },
		{ "F2", 0x3C },
		{ "F3", 0x3D },
		{ "F4", 0x3E },
		{ "F5", 0x3F },
		{ "F6", 0x40 },
		{ "F7", 0x41 },
		{ "F8", 0x42 },
		{ "F9", 0x43 },
		{ "F10", 0x44 },
		{ "F11", 0x57 },
		{ "F12", 0x58 },
		{ "Space", 0x39 },
		{ "Tab", 0x0F },
		{ "LeftShift", 0x2A },
		{ "LeftCtrl", 0x1D },
		{ "LeftAlt", 0x38 },
	} };

	inline constexpr std::array<KeyName, 16> kGamepadButtons{ {
		{ "A", 0x1000 },
		{ "B", 0x2000 },
		{ "X", 0x4000 },
		{ "Y", 0x8000 },
		{ "LB", 0x0100 },
		{ "RB", 0x0200 },
		{ "LT", 0x0009 },
		{ "RT", 0x000A },
		{ "LS", 0x0040 },
		{ "RS", 0x0080 },
		{ "Start", 0x0010 },
		{ "Back", 0x0020 },
		{ "DpadUp", 0x0001 },
		{ "DpadDown", 0x0002 },
		{ "DpadLeft", 0x0004 },
		{ "DpadRight", 0x0008 },
	} };

	[[nodiscard]] constexpr bool NamesMatch(std::string_view a, std::string_view b) noexcept
	{
		const auto lower = [](char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; };
		return std::ranges::equal(a, b, [&lower](char x, char y) { return lower(x) == lower(y); });
	}

	template <std::size_t N>
	[[nodiscard]] constexpr std::optional<std::uint32_t> FindKey(const std::array<KeyName, N>& table, std::string_view name) noexcept
	{
		const auto it = std::ranges::find_if(table, [name](const KeyName& key) { return NamesMatch(key.first, name); });
		if (it == table.end()) {
			return std::nullopt;
		}
		return it->second;
	}

	[[nodiscard]] constexpr std::optional<std::uint32_t> KeyboardKeyCode(std::string_view name) noexcept
	{
		return FindKey(kKeyboardKeys, name);
	}

	[[nodiscard]] constexpr std::optional<std::uint32_t> GamepadButtonCode(std::string_view name) noexcept
	{
		return FindKey(kGamepadButtons, name);
	}
}
