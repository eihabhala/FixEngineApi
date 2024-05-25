#pragma once
#include <string>

struct Config {
	std::string PipeName;
	std::string Host;
	std::string Port;
	std::string User;
	std::string Password;
	std::string SenderCompID;
	std::string TargetCompID;
	std::string SenderSubID;
	std::string TargetSubID;
	std::string MarketDataMode;
};