#include "PCH.h"

#include "Config.h"
#include "GameEvents.h"
#include "HeatSource.h"
#include "HoldInput.h"
#include "MenuUI.h"  // NOLINT(readability-duplicate-include): clang-tidy 21 false positive on Linux clang-cl
#include "Ticker.h"

void SetupLog()
{
	auto logsFolder = logger::log_directory();
	if (!logsFolder) {
		util::report_and_fail("SKSE log_directory not provided, logs can't be written");
	}

	const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
	const auto  logName = plugin ? std::string{ plugin->GetName() } + ".log" : "Plugin.log";
	auto        logPath = *logsFolder / logName;

	auto                          fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
	std::vector<spdlog::sink_ptr> sinks{ fileSink };
	if (IsDebuggerPresent()) {
		sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
	}

	auto spdlogger = std::make_shared<spdlog::logger>("global", sinks.begin(), sinks.end());
	spdlog::set_default_logger(std::move(spdlogger));
	spdlog::set_pattern("[%H:%M:%S.%e] [%l] [%s:%#] %v");
#ifdef NDEBUG
	spdlog::set_level(spdlog::level::info);
#else
	spdlog::set_level(spdlog::level::trace);
#endif
	spdlog::flush_on(spdlog::level::info);
}

void OnInputLoaded()
{
	auto* inputDeviceManager = RE::BSInputDeviceManager::GetSingleton();
	if (!inputDeviceManager) {
		logger::error("BSInputDeviceManager unavailable; hold to warm hands disabled");
		return;
	}
	inputDeviceManager->PrependEventSink(WarmHands::HoldInput::GetSingleton());
}

void OnDataLoaded()
{
	WarmHands::MenuUI::Register();
	if (WarmHands::HeatSource::Initialize()) {
		WarmHands::GameEvents::GetSingleton()->Register();
		WarmHands::Ticker::Start();
	}
}

void OnGameLoaded()
{
	WarmHands::Ticker::Reset();
	WarmHands::HoldInput::GetSingleton()->Configure(WarmHands::Config::Active());
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	SetupLog();

	const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
	if (!plugin) {
		logger::error("Failed to get plugin declaration");
		return false;
	}
	logger::info("{} v{} loaded", plugin->GetName(), plugin->GetVersion());

	const auto settings = WarmHands::Config::LoadSettings();
	WarmHands::Config::SetCurrent(settings);
	WarmHands::Config::ApplyLogLevel(settings);

	const auto* messaging = SKSE::GetMessagingInterface();
	if (!messaging) {
		logger::error("Failed to get SKSE messaging interface");
		return false;
	}

	if (!messaging->RegisterListener([](SKSE::MessagingInterface::Message* a_msg) {
			switch (a_msg->type) {
			case SKSE::MessagingInterface::kInputLoaded:
				OnInputLoaded();
				break;
			case SKSE::MessagingInterface::kDataLoaded:
				OnDataLoaded();
				break;
			case SKSE::MessagingInterface::kPostLoadGame:
			case SKSE::MessagingInterface::kNewGame:
				OnGameLoaded();
				break;
			default:
				break;
			}
		})) {
		logger::error("Failed to register messaging listener");
		return false;
	}

	return true;
}
