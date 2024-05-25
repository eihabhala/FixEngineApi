#include "Pipe.h"

Pipe::Pipe(TTFixFeedClient::FixFeedClient& application) : application(application), hPipe(0) {}

void Pipe::PipeReaderThread(HANDLE hPipe) {
	char buffer[1024];
	DWORD bytesRead;

	if (!ReadFile(hPipe, buffer, sizeof(buffer), &bytesRead, NULL)) {
		std::cout << "Failed to read data from client. Error code: " << GetLastError() << std::endl;
	}
	else {
		if(application.m_fix_connected) std::cout << "Received data from client: " << buffer << std::endl;
		double ask = application.askPrices[buffer];
		double bid = application.bidPrices[buffer];
		std::stringstream send;
		send << bid << "|" << ask;
		PipeSenderThread(hPipe, send.str().c_str());
		if (!application.m_symbols_subscribed_map[buffer]) {
			if (bid == 0 && ask == 0 && application.m_symbols.size() > 0) {
				application.ExecuteShellCommand("sb", application.m_symbolnames_symbolid_map[buffer]);
				application.m_symbols_subscribed_map[buffer] = true;
			}
			else if (application.m_fix_connected) {
				application.ExecuteShellCommand("sb", application.m_symbolnames_symbolid_map[buffer]);
				application.m_symbols_subscribed_map[buffer] = true;
			}
		}
	}
}

void Pipe::PipeSenderThread(HANDLE hPipe, const char* message) {
	DWORD bytesWritten;
	if (!WriteFile(hPipe, message, strlen(message) + 1, &bytesWritten, NULL)) {
		std::cout << "Failed to send data to client. Error code: " << GetLastError() << std::endl;
	}
	else {
		if (application.m_fix_connected) std::cout << "Sent data to client. -> " << message << std::endl;
	}
}

int Pipe::main() {
	std::string pipename = "\\\\.\\pipe\\mt4-" + application.m_config.PipeName;
	while (true)
	{
		hPipe = CreateNamedPipe(
			pipename.c_str(),				 // Pipe name
			PIPE_ACCESS_DUPLEX,              // Pipe open mode
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,  // Pipe mode and blocking
			PIPE_UNLIMITED_INSTANCES,        // Maximum number of instances
			0,                               // Output buffer size
			0,                               // Input buffer size
			0,                               // Default timeout (0 means wait indefinitely)
			NULL                             // Security attributes
		);

		/*if (hPipe == INVALID_HANDLE_VALUE)
		{
			std::cerr << "Failed to create named pipe. Error: " << GetLastError() << std::endl;
			return 1;
		}*/

		if (!application.m_fix_connected) {
			std::cerr << "Connection to server not yet established. Waiting..." << std::endl;
			Sleep(1);
		}

		if (ConnectNamedPipe(hPipe, NULL))
		{
			PipeReaderThread(hPipe);
		}
		else
		{
			DWORD lastError = GetLastError();
			if (lastError == ERROR_PIPE_CONNECTED)
			{
				PipeReaderThread(hPipe);
			}
			else if (lastError == ERROR_NO_DATA)
			{
				std::cerr << "Failed to connect to client. Error: " << lastError << std::endl;
				continue;
			}
			else if (lastError == ERROR_PIPE_LISTENING)
			{
				std::cerr << "Failed to connect to client. Error: " << lastError << std::endl;
				continue;
			}
			else
			{
				std::cerr << "Failed to connect to client. Error: " << lastError << std::endl;
				continue;
			}
		}
	}

	CloseHandle(hPipe);
	return 0;
}