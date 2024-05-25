#include "stdafx.h"

#ifdef _MSC_VER
#pragma warning( disable : 4503 4355 4786 )
#else
#include "config.h"
#endif

#include "FixFeedClient.h"
#include "FixMessage.h"

namespace TTFixFeedClient
{
	void FixFeedClient::onLogon(const FIX::SessionID& sessionId)
	{
		std::cout << std::endl << "Logon - " << sessionId << std::endl;
		m_sessionId = &sessionId;
		ExecuteShellCommand("s", "");
		Sleep(2000);
		m_fix_connected = true;
	}

	void FixFeedClient::onLogout(const FIX::SessionID& sessionId)
	{
		std::cout << std::endl << "Logout - " << sessionId << std::endl;
		m_sessionId = nullptr;
		m_fix_connected = false;
		for (auto& pair : m_symbols_subscribed_map) {
			pair.second = false;
		}
	}

	void FixFeedClient::onMessage(const FIX44::TwoFactorLogon& msg, const FIX::SessionID& sessionId)
	{
		FIX::TwoFactorReason reason;
		msg.get(reason);
		char r = reason.getValue();

		FIX::Text text;
		msg.get(text);

		std::cout << std::endl << "TwoFactorLogon received - " << text.getValue() << std::endl;

		if (r == FIX::TwoFactorReason_SERVER_REQUEST)
		{
			std::cout << std::endl << "Enter One-time password: " << std::endl;

			std::string otp;
			std::getline(std::cin, otp);

			FIX44::TwoFactorLogon response(FIX::TwoFactorReason_CLIENT_RESPONSE);
			response.setField(FIX::OneTimePassword(otp));
			FIX::Session::sendToTarget(response, sessionId);
		}
		else if (r == FIX::TwoFactorReason_SERVER_SUCCESS)
			std::cout << std::endl << "One-time password was successfully validated!" << std::endl;
		else if (r == FIX::TwoFactorReason_SERVER_ERROR)
			std::cout << std::endl << "Failed to validate One-time password!" << std::endl;
		else
			std::cout << std::endl << "Invalid two factor reason!" << std::endl;
	}

	void FixFeedClient::onMessage(const FIX44::TradingSessionStatus& msg, const FIX::SessionID& sessionId)
	{
		std::cout << std::endl << "TradingSessionStatus:" << std::endl << FixMessage(msg.toString()) << std::endl;
	}

	void FixFeedClient::onMessage(const FIX44::CurrencyList& msg, const FIX::SessionID&)
	{
		std::cout << std::endl << "CurrencyList: " << FixMessage(msg.toString()) << std::endl;
	}

	void FixFeedClient::onMessage(const FIX44::SecurityList& msg, const FIX::SessionID&)
	{
		std::cout << std::endl << "SecurityList: " << FixMessage(msg.toString()) << std::endl;

		FIX::NoRelatedSym noSym;
		msg.get(noSym);
		int count = noSym.getValue();
		for (int i = 1; i <= count; i++)
		{
			FIX44::SecurityList::NoRelatedSym symGrp;
			msg.getGroup(i, symGrp);

			FIX::Symbol symbol;
			symGrp.get(symbol);

			std::string name;
			if (symGrp.isSetField(1007)) {
				name = symGrp.getField(1007);
				m_symbol_names.push_back(name);
				m_symbolnames_symbolid_map[name] = symbol.getValue();
				m_symbolid_symbolnames_map[symbol.getValue()] = name;
			}

			m_symbols.push_back(symbol.getValue());
		}
	}

	void FixFeedClient::onMessage(const FIX44::MarketDataRequestAck& msg, const FIX::SessionID&)
	{
		FIX::TotalNumMarketSnaps total;
		msg.get(total);
		std::cout << std::endl << "Total number of snapshots returned " << total.getValue() << std::endl;
	}

	void FixFeedClient::onMessage(const FIX44::MarketDataSnapshotFullRefresh& msg, const FIX::SessionID&)
	{
		FIX::Symbol symbol;
		FIX::OrigTime origTime;
		FIX::TickId tickId;
		FIX::IndicativeTick indicative;
		FIX::NoMDEntries noMDEntries;

		std::string ind_str;
		std::string tickId_str;
		double volume;

		if (msg.isSetField(FIX::FIELD::Symbol)) msg.get(symbol);
		if (msg.isSetField(FIX::FIELD::OrigSendingTime)) msg.get(origTime);
		if (msg.isSetField(FIX::FIELD::TickId)) {
			msg.get(tickId);
			tickId_str = tickId.getValue();
		}
		if (msg.isSetField(FIX::FIELD::IndicativeTick)) {
			msg.get(indicative);
			ind_str = indicative.getValue() == 1 ? "indicative" : "";
		}

		msg.get(noMDEntries);
		int count = noMDEntries.getValue();
		for (int i = 1; i <= count; i++)
		{
			FIX::Symbol symbol;
			FIX::MDEntryType mdEntryType;
			FIX::MDEntryPx bidPrice;
			FIX::MDEntryPx askPrice;
			FIX::MDUpdateAction action;
			FIX::MDEntrySize volume;

			FIX44::MarketDataSnapshotFullRefresh::NoMDEntries entry;
			msg.getGroup(i, entry);
			msg.getField(symbol);
			if (entry.isSetField(FIX::FIELD::MDUpdateAction)) entry.getField(action);
			if (entry.isSetField(FIX::FIELD::MDEntryType)) entry.get(mdEntryType);
			if (entry.isSetField(FIX::FIELD::MDEntrySize)) {
				entry.get(volume);
				volume = volume.getValue();
			}

			switch (mdEntryType)
			{
			case FIX::MDEntryType_BID:
				entry.getField(bidPrice);
				bidPrices[symbol] = bidPrice;
				bidPrices[m_symbolid_symbolnames_map[symbol]] = bidPrice.getValue();
				break;
			case FIX::MDEntryType_OFFER:
				entry.getField(askPrice);
				askPrices[symbol] = askPrice;
				askPrices[m_symbolid_symbolnames_map[symbol]] = askPrice.getValue();
				break;
			case FIX::MDEntryType_TRADE:
				break;
			default:
				break;
			}
		}

		std::cout << "Symbol: " << symbol << " Bid Price: " << bidPrices[symbol] << " Ask Price: " << askPrices[symbol] << std::endl;
	}

	void FixFeedClient::onMessage(const FIX44::MarketDataIncrementalRefresh& msg, const FIX::SessionID&)
	{
		FIX::NoMDEntries noMDEntries;

		msg.get(noMDEntries);
		int count = noMDEntries.getValue();
		for (int i = 1; i <= count; i++)
		{
			FIX44::MarketDataIncrementalRefresh::NoMDEntries entry;
			msg.getGroup(i, entry);

			FIX::Symbol symbol;
			FIX::MDEntryType mdEntryType;
			FIX::MDEntryPx bidPrice;
			FIX::MDEntryPx askPrice;
			FIX::MDUpdateAction action;

			if (!entry.isSetField(FIX::FIELD::Symbol) || !entry.isSetField(FIX::FIELD::MDEntryType) || !entry.isSetField(FIX::FIELD::MDEntryPx) || !entry.isSetField(FIX::FIELD::MDUpdateAction))continue;

			entry.getField(symbol);
			entry.getField(mdEntryType);
			entry.getField(action);

			if (action == FIX::MDUpdateAction_DELETE)continue;

			switch (mdEntryType)
			{
			case FIX::MDEntryType_BID:
				entry.getField(bidPrice);
				bidPrices[symbol] = bidPrice;
				bidPrices[m_symbolid_symbolnames_map[symbol]] = bidPrice.getValue();
				break;
			case FIX::MDEntryType_OFFER:
				entry.getField(askPrice);
				askPrices[symbol] = askPrice;
				askPrices[m_symbolid_symbolnames_map[symbol]] = askPrice.getValue();
				break;
			case FIX::MDEntryType_TRADE:
				break;
			default:
				break;
			}

			std::cout << "Symbol: " << symbol << " Bid Price: " << bidPrices[symbol] << " Ask Price: " << askPrices[symbol] << std::endl;
		}
	}

	void FixFeedClient::onMessage(const FIX44::MarketDataHistory& msg, const FIX::SessionID&)
	{
		std::cout << std::endl << "IN: " << FixMessage(msg.toString()) << std::endl;
	}

	void FixFeedClient::onMessage(const FIX44::MarketDataHistoryMetadataReport& msg, const FIX::SessionID&)
	{
		std::cout << std::endl << "IN: " << FixMessage(msg.toString()) << std::endl;
	}

	void FixFeedClient::onMessage(const FIX44::MarketDataHistoryInfoReport& msg, const FIX::SessionID& sessionId)
	{
		std::cout << std::endl << "IN: " << FixMessage(msg.toString()) << std::endl;
	}

	void FixFeedClient::onMessage(const FIX44::ComponentsInfoReport& msg, const FIX::SessionID& sessionId)
	{
		std::cout << std::endl << "IN: " << FixMessage(msg.toString()) << std::endl;
	}

	void FixFeedClient::onMessage(const FIX44::FileChunk& msg, const FIX::SessionID& sessionId)
	{
		//std::cout << std::endl << "IN: " << FixMessage(msg.toString()) << std::endl;

		FIX::FileId fid;
		FIX::ChunkId cid;
		FIX::ChunksNo cno;
		FIX::RawDataLength size;
		FIX::RawData data;

		msg.get(fid);
		msg.get(cid);
		msg.get(cno);
		msg.get(size);
		msg.get(data);

		std::string fidValue = fid.getValue();
		int cidValue = cid.getValue();
		int cnoValue = cno.getValue();
		int sizeValue = size.getValue();
		std::string dataValue = data.getValue();

		std::cout << std::endl << "File ID: " << fidValue << " Chunk ID: " << cidValue << "/" << cnoValue << " Size: " << sizeValue << std::endl;

		const char* dataStr = data.getString().c_str();

		int res = _mkdir("FileChunks");
		std::ofstream ofs(".\\FileChunks\\File_" + fidValue + "_" + cid.getString() + ".zip", std::ios_base::app);
		ofs.write(dataStr, sizeValue);
		ofs.close();
	}

	void FixFeedClient::fromAdmin(const FIX::Message& msg, const FIX::SessionID&)
		throw(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::RejectLogon)
	{
		std::cout << std::endl << "IN [ADMIN]: " << FixMessage(msg.toString()) << std::endl;
	}

	void FixFeedClient::fromApp(const FIX::Message& msg, const FIX::SessionID& sessionId)
		throw(/*FIX::FieldNotFound,*/ FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::UnsupportedMessageType)
	{
		std::cout << std::endl << "IN [APP]: " << FixMessage(msg.toString()) << std::endl;
		crack(msg, sessionId);
	}

	void FixFeedClient::toAdmin(FIX::Message& msg, const FIX::SessionID& sessionId)
	{
		if (FIX::MsgType_Logon == msg.getHeader().getField(FIX::FIELD::MsgType))
		{
			msg.setField(FIX::Username(m_config.User));
			msg.setField(FIX::Password(m_config.Password));
			//msg.setField(FIX::DeviceID(m_deviceId));
			//msg.setField(FIX::AppSessionID(m_appSessionId));
			//msg.setField(FIX::ProtocolSpec("ext.1.72"));
			msg.setField(FIX::SenderSubID(m_config.SenderSubID));
			msg.setField(FIX::TargetSubID(m_config.TargetSubID));
		}
		std::cout << std::endl << "OUT [ADMIN]: " << FixMessage(msg.toString()) << std::endl;
	}

	void FixFeedClient::toApp(FIX::Message& msg, const FIX::SessionID& sessionId) throw(FIX::DoNotSend)
	{
		std::cout << std::endl << "OUT [APP]: " << FixMessage(msg.toString()) << std::endl;
	}

	void printHelp()
	{
		std::cout << std::endl
			<< "c - get all currencies" << std::endl
			<< "s - get all symbols" << std::endl
			<< "sb - subscribe for all or single symbol with depth. Exaples: sb all 1 | sb EUR/USD 5" << std::endl;
	}

	void FixFeedClient::ExecuteShellCommand(std::string cmd, std::string param1) {
		if (cmd == "e")
		{
			return;
		}
		if (cmd == "?")
		{
			printHelp();
		}
		else if (cmd == "c")
		{
			FIX44::CurrencyListRequest request;
			request.setField(FIX::CurrencyReqID("Currencies_123"));
			request.setField(FIX::CurrencyListRequestType(FIX::CurrencyListRequestType_ALLCURRENCIES));
			FIX::Session::sendToTarget(request, *m_sessionId);
		}
		else if (cmd == "s")
		{
			FIX44::SecurityListRequest request;
			request.setField(FIX::SecurityReqID("Securities_123"));
			request.setField(FIX::SecurityListRequestType(FIX::SecurityListRequestType_SYMBOL));
			//request.setField(FIX::Symbol("1"));
			FIX::Session::sendToTarget(request, *m_sessionId);
		}
		else if (cmd == "sb") {
			static int reqid = 1;
			std::string requestId = std::to_string(reqid) + "-MD-" + param1;
			std::string symbolcode = param1;
			FIX44::MarketDataRequest marketDataRequest;
			marketDataRequest.setField(FIX::MDReqID(requestId));
			marketDataRequest.setField(FIX::SubscriptionRequestType(FIX::SubscriptionRequestType_SNAPSHOT_PLUS_UPDATES));
			marketDataRequest.setField(FIX::MarketDepth(std::stoi(m_config.MarketDataMode)));
			marketDataRequest.setField(FIX::MDUpdateType(FIX::MDUpdateType_INCREMENTAL_REFRESH));
			//marketDataRequest.setField(FIX::AggregatedBook(false)); 
			marketDataRequest.setField(FIX::NoMDEntryTypes(2));
			marketDataRequest.setField(FIX::NoRelatedSym(1));

			FIX44::MarketDataRequest::NoMDEntryTypes group;
			group.setField(FIX::MDEntryType(FIX::MDEntryType_BID));
			marketDataRequest.addGroup(group);

			FIX44::MarketDataRequest::NoMDEntryTypes group2;
			group2.setField(FIX::MDEntryType(FIX::MDEntryType_OFFER));
			marketDataRequest.addGroup(group2);

			FIX44::MarketDataRequest::NoRelatedSym symbolGroup;
			symbolGroup.setField(FIX::Symbol(symbolcode));
			marketDataRequest.addGroup(symbolGroup);

			++reqid;

			FIX::Session::sendToTarget(marketDataRequest, *m_sessionId);
		}
	}

	void FixFeedClient::run()
	{
		//Sleep(2000);
		//printHelp();

		while (true)
		{
			int delay = 0;

			try
			{
				//std::cout << std::endl << ">>";
				std::string cmdLine;
				std::getline(std::cin, cmdLine);

				std::istringstream issp(cmdLine);
				std::vector<std::string> params
				{
					std::istream_iterator<std::string> { issp },
					std::istream_iterator<std::string> { }
				};
				std::string cmd = params[0];

				ExecuteShellCommand(cmd, NULL);
			}
			catch (std::exception& e)
			{
				std::cout << "Message Not Sent: " << e.what();
			}

			Sleep(delay);
		}
	}
}
