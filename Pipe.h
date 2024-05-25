#pragma once

#include <thread>
#include "FixFeedClient.h"

class Pipe
{
public:
	std::string PipeName = "SA_600";
	std::vector<std::thread> clientThreads;
	HANDLE hPipe;
	TTFixFeedClient::FixFeedClient& application;
public:
	Pipe(TTFixFeedClient::FixFeedClient& application);
	int main();
	void PipeReaderThread(HANDLE handle);
	void PipeSenderThread(HANDLE handle, const char* message);
};