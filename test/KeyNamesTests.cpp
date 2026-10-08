#include <catch2/catch_test_macros.hpp>

#include "KeyNames.h"

using WarmHands::GamepadButtonCode;
using WarmHands::KeyboardKeyCode;

TEST_CASE("KeyboardKeyCode maps letters, digits and function keys", "[keys]")
{
	CHECK(KeyboardKeyCode("R") == 0x13U);
	CHECK(KeyboardKeyCode("Q") == 0x10U);
	CHECK(KeyboardKeyCode("M") == 0x32U);
	CHECK(KeyboardKeyCode("1") == 0x02U);
	CHECK(KeyboardKeyCode("0") == 0x0BU);
	CHECK(KeyboardKeyCode("F1") == 0x3BU);
	CHECK(KeyboardKeyCode("F12") == 0x58U);
	CHECK(KeyboardKeyCode("Space") == 0x39U);
}

TEST_CASE("KeyboardKeyCode ignores case and rejects unknown names", "[keys]")
{
	CHECK(KeyboardKeyCode("r") == 0x13U);
	CHECK(KeyboardKeyCode("leftshift") == 0x2AU);
	CHECK_FALSE(KeyboardKeyCode("").has_value());
	CHECK_FALSE(KeyboardKeyCode("Banana").has_value());
}

TEST_CASE("GamepadButtonCode maps face, shoulder and trigger buttons", "[keys]")
{
	CHECK(GamepadButtonCode("X") == 0x4000U);
	CHECK(GamepadButtonCode("A") == 0x1000U);
	CHECK(GamepadButtonCode("Y") == 0x8000U);
	CHECK(GamepadButtonCode("LB") == 0x0100U);
	CHECK(GamepadButtonCode("RT") == 0x000AU);
	CHECK(GamepadButtonCode("DpadUp") == 0x0001U);
}

TEST_CASE("GamepadButtonCode ignores case and rejects unknown names", "[keys]")
{
	CHECK(GamepadButtonCode("x") == 0x4000U);
	CHECK_FALSE(GamepadButtonCode("Z").has_value());
}
