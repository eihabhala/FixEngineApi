#include "stdafx.h"

#include "FileStore.h"
#include "NullStore.h"
#include "FileLog.h"
#include "SocketInitiator.h"
#include "SessionSettings.h"

#include "FixFeedClient.h"
#include "FixFileLog.h"
#include "FixMessage.h"
#include "Pipe.h"
#include <iostream>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "Main.h"
#define HAVE_SSL 1

using namespace TTFixFeedClient;

std::string CreateGUID()
{
	GUID guid;
	HRESULT res = CoCreateGuid(&guid);

	char guidStr[37];
	sprintf_s(
		guidStr,
		"%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
		guid.Data1, guid.Data2, guid.Data3,
		guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

	return std::string(guidStr);
}

void PipeThread(FixFeedClient& application) {
	Pipe Pipe(application);
	Pipe.main();
}

std::vector<std::string> splitString(const std::string& str, char delimiter)
{
	std::vector<std::string> result;
	std::stringstream ss(str);
	std::string token;

	while (std::getline(ss, token, delimiter))
	{
		result.push_back(token);
	}

	return result;
}

bool ReadCredentials(Config& config)
{
	std::string filename = "ctrader.config";
	std::ifstream file(filename);
	if (!file.is_open()) {
		throw std::runtime_error("Failed to open file: " + filename);
		return false;
	}

	std::stringstream readbuffer; readbuffer << file.rdbuf();
	std::string configstring = readbuffer.str();
	rapidjson::Document configobj; configobj.Parse(configstring.c_str());
	 
	if (configobj.HasMember("PipeName") && configobj["PipeName"].IsString()) config.PipeName = configobj["PipeName"].GetString();
	if (configobj.HasMember("Host") && configobj["Host"].IsString()) config.Host = configobj["Host"].GetString();
	if (configobj.HasMember("Port") && configobj["Port"].IsString()) config.Port = configobj["Port"].GetString();
	if (configobj.HasMember("User") && configobj["User"].IsString()) config.User = configobj["User"].GetString();
	if (configobj.HasMember("Password") && configobj["Password"].IsString()) config.Password = configobj["Password"].GetString();
	if (configobj.HasMember("SenderCompID") && configobj["SenderCompID"].IsString()) config.SenderCompID = configobj["SenderCompID"].GetString();
	if (configobj.HasMember("TargetCompID") && configobj["TargetCompID"].IsString()) config.TargetCompID = configobj["TargetCompID"].GetString();
	if (configobj.HasMember("SenderSubID") && configobj["SenderSubID"].IsString()) config.SenderSubID = configobj["SenderSubID"].GetString();
	if (configobj.HasMember("TargetSubID") && configobj["TargetSubID"].IsString()) config.TargetSubID = configobj["TargetSubID"].GetString();
	if (configobj.HasMember("MarketDataMode") && configobj["MarketDataMode"].IsString()) config.MarketDataMode = configobj["MarketDataMode"].GetString();

	//rapidjson::StringBuffer buffer;
	//rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	//config.Accept(writer);
	//std::cout << buffer.GetString() << std::endl;
	
	return true;
}

int _tmain(int argc, _TCHAR* argv[])
{
	try
	{
		Config config;
		if (!ReadCredentials(std::ref(config))) {
			std::cout << "Failed to read cTrader configuration." << std::endl;
			return 0;
		}

		auto now = std::chrono::system_clock::now();
		auto end = std::chrono::system_clock::time_point{ std::chrono::seconds(1688651602) };

		int res = _mkdir("Log");
		FixMessage::LoadDictionary("FIX44-1.7.xml");

		FIX::Dictionary defaults;
		defaults.setString("ConnectionType", "initiator");
		defaults.setString("ReconnectInterval", "5");
		defaults.setString("ResetOnLogout", "Y");
		defaults.setString("ResetOnDisconnect", "N");
		defaults.setString("ResetOnLogon", "Y");
		defaults.setString("CheckLatency", "N");
		defaults.setString("RefreshOnLogon", "Y");
		defaults.setString("StartTime", "00:00:00");
		defaults.setString("EndTime", "00:00:00");
		defaults.setString("HeartBtInt", "30");
		defaults.setString("UseDataDictionary", "Y");
		defaults.setString("DataDictionary", "FIX44-1.7.xml");
		defaults.setString("FileLogPath", "log");
		defaults.setString("FileStorePath", "store");
		defaults.setString("ValidateFieldsHaveValues", "N");
		/*defaults.setString("SSLEnable", "Y");
		defaults.setString("SSLValidateCertificates", "N");
		defaults.setString("SSLCertificate", "cert.pfx");
		defaults.setString("SSLCertificatePassword", "1234567890");
		defaults.setString("SSLProtocols", "Tls");*/

		std::string id = CreateGUID();
		std::string username = config.User;
		std::string password = config.Password;
		std::string deviceId = "123456789";
		std::string appSessionId = "987654321";

		FIX::SessionID sessionId("FIX.4.4", config.SenderCompID, config.TargetCompID);

		FIX::Dictionary sessionSettings;
		sessionSettings.setString("SocketConnectHost", config.Host);
		sessionSettings.setString("SocketConnectPort", config.Port);
		/*sessionSettings.setString("SSLEnable", "Y");
		sessionSettings.setString("SSLValidateCertificates", "N");
		sessionSettings.setString("SSLCertificate", "cert.pfx");
		sessionSettings.setString("SSLCertificatePassword", "1234567890");
		sessionSettings.setString("SSLProtocols", "Tls");*/

		FIX::SessionSettings settings;
		settings.set(defaults);
		settings.set(sessionId, sessionSettings);

		FixFileLogFactory fileLogFactory(L"Log", StringUtils::StringToWString(id + "_Events.log"), StringUtils::StringToWString(id + "_Messages.log"), "", true);
		FIX::NullStoreFactory storeFactory;
		FixFeedClient application(config, deviceId, appSessionId);
		FIX::SocketInitiator initiator(application, storeFactory, settings, fileLogFactory);

		std::thread PipeThread(PipeThread, std::ref(application));
		initiator.start();
		application.run();
		initiator.stop();
		PipeThread.join();

		return 0;
	}
	catch (std::exception& e)
	{
		std::cout << e.what();
		return 1;
	}
}