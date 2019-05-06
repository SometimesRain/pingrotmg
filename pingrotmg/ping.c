#include <stdio.h>

#include <Winsock2.h> 
#include <ws2tcpip.h> 
#pragma comment(lib, "Ws2_32.lib")

#include <MSWSock.h>
#pragma comment(lib, "Mswsock.lib")

typedef struct SERVER
{
	const char* name;
	const char* host;
} SERVER;

typedef struct SESSION
{
	OVERLAPPED overlapped;
	SERVER server;
	SOCKADDR_IN serverAddr;
	SOCKET socket;
	LONGLONG startTime;
} SESSION;

typedef struct STATE
{
	LONGLONG frequency;
	HANDLE completionPort;
	int numTested;
} STATE;

typedef enum CONSOLECOLOR
{
	CONSOLECOLOR_BLACK,
	CONSOLECOLOR_BLUE,
	CONSOLECOLOR_GREEN,
	CONSOLECOLOR_TEAL,
	CONSOLECOLOR_RED,
	CONSOLECOLOR_MAGENTA,
	CONSOLECOLOR_YELLOW,
	CONSOLECOLOR_WHITE,
	CONSOLECOLOR_GRAY,
	CONSOLECOLOR_BRIGHT_BLUE,
	CONSOLECOLOR_BRIGHT_GREEN,
	CONSOLECOLOR_BRIGHT_TEAL,
	CONSOLECOLOR_BRIGHT_RED,
	CONSOLECOLOR_BRIGHT_MAGENTA,
	CONSOLECOLOR_BRIGHT_YELLOW,
	CONSOLECOLOR_BRIGHT_WHITE
} CONSOLECOLOR;

SERVER servers[] =
{
	{ "USEast",			"52.23.232.42" },
	{ "AsiaSouthEast",	"52.77.221.237" },
	{ "USSouth",		"52.91.68.60" },
	{ "USSouthWest",	"54.183.179.205" },
	{ "USEast2",		"3.88.196.105" },
	{ "USNorthWest",	"54.234.151.78" },
	{ "AsiaEast",		"54.199.197.208" },
	{ "EUSouthWest",	"52.47.178.13" },
	{ "USSouth2",		"54.183.236.213" },
	{ "EUNorth2",		"52.59.198.155" },
	{ "EUSouth",		"35.180.134.209" },
	{ "USSouth3",		"13.57.182.96" },
	{ "EUWest2",		"34.243.37.98" },
	{ "USMidWest",		"18.220.226.127" },
	{ "EUWest",			"52.47.149.74" },
	{ "USEast3",		"54.157.6.58" },
	{ "USWest",			"13.57.254.131" },
	{ "USWest3",		"54.67.119.179" },
	{ "USMidWest2",		"18.218.255.91" },
	{ "EUEast",			"18.195.167.79" },
	{ "Australia",		"54.252.165.65" },
	{ "EUNorth",		"54.93.78.148" },
	{ "USWest2",		"54.215.251.128" }
};

DWORD _stdcall completionPortRoutine(STATE* state)
{
	SESSION* session;
	DWORD numBytes;
	ULONG key;
	HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);

	while (state->numTested < sizeof(servers) / sizeof(SERVER))
	{
		if (GetQueuedCompletionStatus(state->completionPort, &numBytes, &key, (OVERLAPPED**)&session, INFINITE))
		{
			//Connected, stop the time
			LONGLONG endTime;
			QueryPerformanceCounter((LARGE_INTEGER*)&endTime);

			//Select a color
			LONGLONG timePassed = (endTime - session->startTime) / state->frequency;
			if (timePassed < 140)
				SetConsoleTextAttribute(hStdout, CONSOLECOLOR_BRIGHT_GREEN);
			else if (timePassed < 280)
				SetConsoleTextAttribute(hStdout, CONSOLECOLOR_BRIGHT_YELLOW);
			else
				SetConsoleTextAttribute(hStdout, CONSOLECOLOR_BRIGHT_RED);

			//Format nicely
			printf("%-14s %4lld ms\n", session->server.name, timePassed);
		}
		else
		{
			//Return if completion port was closed
			if (session == NULL)
				return 0;

			//Some other issue
			SetConsoleTextAttribute(hStdout, CONSOLECOLOR_BRIGHT_RED);
			printf("%s connection error\n", session->server.name);
		}

		//Close the socket and increment test finished counter
		closesocket(session->socket);
		InterlockedIncrement(&state->numTested);
	}

	//Test complete, close the completion port to terminate worker threads
	SetConsoleTextAttribute(hStdout, CONSOLECOLOR_WHITE);
	printf("\nAll servers tested\n");
	CloseHandle(state->completionPort);
	return 0;
}

LPFN_CONNECTEX loadConnectExtension()
{
	//Create dummy socket needed calling WSAIoctl
	SOCKET socket = WSASocketW(AF_INET, SOCK_STREAM, 0, NULL, 0, 0);

	LPFN_CONNECTEX ConnectEx;
	DWORD dwBytes;
	GUID guid = WSAID_CONNECTEX;
	WSAIoctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(GUID), &ConnectEx, sizeof(LPFN_CONNECTEX), &dwBytes, NULL, NULL);

	closesocket(socket);
	return ConnectEx;
}

int main()
{
	printf("Fast Ping Tester by CrazyJani\n\n");

	//########################################## Initialize Winsock ##########################################

	//Start winsock and load ConnectEx extension
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	LPFN_CONNECTEX ConnectEx = loadConnectExtension();

	//Initialize state shared by the worker threads
	STATE state = { 0 };
	QueryPerformanceFrequency((LARGE_INTEGER*)&state.frequency);
	state.frequency /= 1000;

	SOCKADDR_IN clientAddr = { 0 };
	clientAddr.sin_family = AF_INET;

	//Create an IO completion port and start two worker threads
	state.completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	for (size_t numThreads = 0; numThreads < 2; numThreads++)
	{
		HANDLE hThread = CreateThread(NULL, 0, completionPortRoutine, &state, 0, NULL);
		CloseHandle(hThread); //Detach thread
	}

	//########################################## Initialize sockets ##########################################

	//Create a session for every server to be tested
	SESSION sessions[sizeof(servers) / sizeof(SERVER)] = { 0 };
	for (size_t i = 0; i < sizeof(servers) / sizeof(SERVER); i++)
	{
		sessions[i].server = servers[i];

		//Create a socket with overlapped IO, bind it to any port and associate with the completion port
		sessions[i].socket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		bind(sessions[i].socket, (SOCKADDR*)&clientAddr, sizeof(SOCKADDR_IN));
		CreateIoCompletionPort((HANDLE)sessions[i].socket, state.completionPort, 0, 0);

		//Initialize a SOCKADDR structure for the server this socket is pinging
		sessions[i].serverAddr.sin_family = AF_INET;
		sessions[i].serverAddr.sin_port = htons(2050);
		InetPtonA(AF_INET, servers[i].host, &sessions[i].serverAddr.sin_addr.s_addr);
	}

	//########################################## Run the ping test ###########################################

	for (size_t i = 0; i < sizeof(servers) / sizeof(SERVER); i++)
	{
		//Connect and start measuring time
		ConnectEx(sessions[i].socket, (SOCKADDR*)&sessions[i].serverAddr, sizeof(SOCKADDR_IN), NULL, 0, NULL, &sessions[i].overlapped);
		QueryPerformanceCounter((LARGE_INTEGER*)&sessions[i].startTime);
	}

	//Keep console open
	getchar();
	return 0;
}