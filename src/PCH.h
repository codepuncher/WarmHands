#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#pragma warning(push)
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/spdlog.h>
#pragma warning(pop)

#include <SimpleIni.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <mutex>
#include <numbers>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace std::literals;

namespace logger = SKSE::log;

namespace util
{
	using SKSE::stl::report_and_fail;
}
