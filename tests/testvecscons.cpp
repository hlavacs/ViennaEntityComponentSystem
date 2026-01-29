#include <iostream>
#include <utility>
#include <ranges>
#include <random>
#include <string>

#include "VECS.h"

#ifdef WIN32
#include <conio.h>
#else
#include <termios.h>
static struct termios termios_org;

// Replacement for _kbhit(), _getche() and _getch() in unixoid environments
// Idea gleaned from https://askubuntu.com/questions/1185926/how-can-i-use-kbhit-function-while-using-gcc-compiler-as-it-does-not-contains
// and https://stackoverflow.com/questions/7469139/what-is-the-equivalent-to-getch-getche-in-linux
/* Initialize new terminal i/o settings */
static void initTermios(int echo)
{
	tcgetattr(0, &termios_org);
	struct termios termios_current = termios_org; /* make new settings same as old settings */
	termios_current.c_lflag &= ~ICANON; /* disable buffered i/o */
	if (echo)
		termios_current.c_lflag |= ECHO; /* set echo mode */
	else
		termios_current.c_lflag &= ~ECHO; /* set no echo mode */
	tcsetattr(0, TCSANOW, &termios_current); /* use these new terminal i/o settings now */
}
/* reset terminal I/O settings to previous setup*/
static void restoreTermios(void)
{
	tcsetattr(0, TCSANOW, &termios_or);
}
int _kbhit() {
	// Unixoid approximation of the _kbhit() function available in MSVC -
	// performs roughly the same by checking for input on file 0 (i.e., standard input) with select().
	fd_set set;
	struct timeval tv { 0 };
	initTermios(0);
	FD_ZERO(&set);
	FD_SET(0, &set);
	select(1, &set, 0, 0, &tv);
	int rc = FD_ISSET(0, &set);
	restoreTermios();
	return rc;
}
int _getch() {
	initTermios(0);
	int c = getchar();
	restoreTermios();
	return c;
}
int _getche() {
	initTermios(1);
	int c = getchar();
	restoreTermios();
	return c;
}
#endif

void TestConn(std::string consoleHost = "127.0.0.1", int consolePort = 2000) {
	std::cout << "\x1b[37m testing Connection!...\n";

	// 
	// create a populated registry
	vecs::Registry system;

	// this test module needs a direct connection to the console communication
	auto comm = vecs::GetConsoleComm(&system, consoleHost, consolePort);

	vecs::Handle h1 = system.Insert(5, 3.0f, 4.0);
	vecs::Handle h2 = system.Insert(1, 23.0f, 3.0);

	system.AddTags(h1, 47ul);
	system.AddTags(h2, 666ul);

	vecs::Handle h3 = system.Insert(6, 7.0f, 8.0);
	vecs::Handle h4 = system.Insert(2, 24.0f, 4.0);

	struct height_t { int i; std::string to_string() const { return std::to_string(i); } };
	using weight_t = vsty::strong_type_t<int, vsty::counter<>>;
	vecs::Handle hx1 = system.Insert(height_t{ 5 }, weight_t{ 6 });

	//Test to see changes in the liveview
	static const char* letsjam[] = {
		 "string 1",
		 "string 2",
		 "string 3",
		 "string 4",
		 "string 5",
	#ifndef _DEBUG
		 "string 6",
		 "string 7",
		 "string 8",
		 "string 9",
		 "string 10",
		 "string 11",
		 "string 12",
		 "string 13",
		 "string 14",
		 "string 15",
	#endif
	};

	std::vector<vecs::Handle> handles;
	// create 20 handles
	for (int i = 10; i < 30; i++) {
		handles.push_back(system.Insert(i, static_cast<float>(i * 2), std::string(letsjam[i % _countof(letsjam)])));
	}
	// erase one of them, leaving 19
	system.Erase(handles[4]);
	handles.erase(handles.begin() + 4);

	std::cout << "\x1b[37m isConnected: " << comm->IsConnected() << "\n";
	bool abortWait{ false }, toldya{ false };
	while (!comm->IsConnected() && !abortWait) {
		if (!toldya) {
			std::cout << "not yet connected!! Press Escape to terminate" << "\n";
			toldya = true;
		}
		while (_kbhit()) {
			if (_getch() == 0x1b)
				abortWait = true;
		}
#ifdef WIN32
		Sleep(250);
#else
		usleep(250 * 1000);
#endif
	}
	std::cout << "\x1b[37m isConnected: " << comm->IsConnected() << "\n";

	if (comm->IsConnected()) {

		// do nothing for 600 seconds, let background task work
		for (int secs = 0; secs < 600 && !abortWait; secs++) {

			while (_kbhit()) {
				char c = _getch();
				auto t1 = std::chrono::high_resolution_clock::now();
				switch (c) {
				case 'a':
					for (int i = 0; i < 100000; i++) {
						handles.push_back(system.Insert(i + 100000, static_cast<float>(i * 7)));
					}
					break;
				case 'd':
					if (handles.size() > 100000) {
						for (int i = 0; i < 100000; i++) {
							system.Erase(handles[handles.size() - 1]); handles.erase(handles.end() - 1);
						}
					}
					break;
				case 'x':
					abortWait = true;
					break;
				default:
					break;
				}
				auto t2 = std::chrono::high_resolution_clock::now();

				auto mics = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
				if (mics > 10)
					std::cout << c << " duration: " << mics << " msecs\n";
			}

			// test for dynamic scaling of entity graph in Console LiveView
			if (secs < 80) {
				handles.push_back(system.Insert(secs + 1000, static_cast<float>(secs * 7)));
				handles.push_back(system.Insert(secs + 1000, static_cast<float>(secs * 7), static_cast<double>(secs * 5)));

			}
			else if (secs == 80) {
				for (auto hit = std::prev(handles.end()); hit > handles.begin() + 18; hit--)
					system.Erase(*hit);
				handles.erase(handles.begin() + 19, handles.end());
			}
			// change contents of 0 every second
			std::cout << "Setting " << std::to_string(handles[0].GetValue()) << " int to " << secs + 1000 << "\n";
			system.Put(handles[0], secs + 10000);
			system.Put(handles[0], std::string(letsjam[secs % _countof(letsjam)]));
			// alternating "add 2 at end, remove 2 at front + 1"
			if (secs & 1) {
				system.Erase(handles[1]); handles.erase(handles.begin() + 1);
				system.Erase(handles[2]); handles.erase(handles.begin() + 2);
			}
			else {
				handles.push_back(system.Insert(secs + 20, static_cast<float>(secs * 2)));
				handles.push_back(system.Insert(secs + 15, static_cast<float>(secs * 3)));
			}

#ifdef WIN32
			Sleep(1000);
#else
			usleep(1000 * 1000);
#endif
			// ... but get out if console has decided to leave the building
			if (!comm->IsConnected())
				break;
		}

		comm->DisconnectFromServer();
	}
	std::cout << "\x1b[37m I hope it works? ...\n";
}

void TestConnComplex(std::string consoleHost = "127.0.0.1", int consolePort = 2000) {
	std::cout << "\x1b[37m testing Connection!...\n";

	// create a populated registry
	vecs::Registry system;

	struct struct1 { int i; double d; };
	struct struct2 { int i; int j; std::string s; };
	struct struct3 { float f; double d; char c; int i; };
	struct struct4 { std::string s; std::string t; };



	// this test module needs a direct connection to the console communication
	auto comm = vecs::GetConsoleComm(&system, consoleHost, consolePort);

	std::cout << "\x1b[37m isConnected: " << comm->IsConnected() << "\n";
	bool abortWait{ false }, toldya{ false };
	while (!comm->IsConnected() && !abortWait) {
		if (!toldya) {
			std::cout << "not yet connected!! Press Escape to terminate" << "\n";
			toldya = true;
		}
		while (_kbhit()) {
			if (_getch() == 0x1b)
				abortWait = true;
		}
#ifdef WIN32
		Sleep(250);
#else
		usleep(250 * 1000);
#endif
	}
	std::cout << "\x1b[37m isConnected: " << comm->IsConnected() << "\n";

	if (comm->IsConnected()) {
		std::vector<vecs::Handle> handles;
		std::cout << "Complex test - use the following keys:\n"
			"  a - add 100000 comple entities\n"
			"  d - delete 100000 comple entities\n"
			"  x - terminate the test\n";
		while (comm->IsConnected() && !abortWait) {
			while (_kbhit()) {
				char c = _getch();
				auto t1 = std::chrono::high_resolution_clock::now();
				switch (c) {
				case 'a':
					for (int i = 0; i < 100000; i++) {
						//handles.push_back(system.Insert(i + 100000, static_cast<float>(i * 7)));
						switch (i % 4) {
						case 0:
							handles.push_back(system.Insert(i, struct1{ i,0.0 }, struct2{ i + 2, i + 3, "still struct 1" }, std::string("this is arch1")));
							break;
						case 1:
							handles.push_back(system.Insert(i, struct2{ i, i + 1, "struct2 string" }, struct3{ 0.2f, 66.6, 'a', i + 9 }, 13.f, 33.2));
							break;
						case 2:
							handles.push_back(system.Insert(i, struct3{ 0.1f, 0.0, 'c', i }, std::string("arch3 rules"), 'r', 1.3));
							break;
						case 3:
							handles.push_back(system.Insert(i, struct4{ "struct4 1", "struct4 2" }, struct1{ i + 6, 9.9 }, struct3{ 17.f, 8.1, 't', i + 78 }, std::string("arch4"), 63.f, 5.5, '6'));
							break;
						default:
							break;
						}
					}
					break;
				case 'd':
					if (handles.size() >= 100000) {
						for (int i = 0; i < 100000; i++) {
							system.Erase(handles[handles.size() - 1]); handles.erase(handles.end() - 1);
						}
					}
					break;
				case 'x':
					abortWait = true;
					break;
				default:
					break;
				}
				auto t2 = std::chrono::high_resolution_clock::now();

				auto mics = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();
				if (mics > 10)
					std::cout << c << " duration: " << mics << " msecs\n";
			}

			Sleep(250);
		}


		comm->DisconnectFromServer();
	}

}

int main(int argc, char* argv[]) {
	std::string host = "127.0.0.1";
	std::string port = "2000";
	bool complexMode = false;
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-')
			for (int j = 1; argv[i][j]; j++)
				switch (argv[i][j]) {
				case 'H':
				case 'h':
					host = argv[i] + j + 1;
					{
						auto colPos = host.find(':');
						if (colPos != std::string::npos) {
							port = host.substr(colPos + 1);
							host = host.substr(0, colPos);
						}
						j = (int)strlen(argv[i]) - 1;
					}
					break;
				case 'P':
				case 'p':
					port = argv[i] + j + 1;
					j = (int)strlen(argv[i]) - 1;
					break;
				case 'C':
				case 'c':
					complexMode = !complexMode;
					break;
				default:
					// simply ignore unknown command line parameters
					break;
				}
	}
	std::cout << "testing VECS Console communication...\n";
	if (complexMode) TestConnComplex(host, std::stoi(port));
	else TestConn(host, std::stoi(port));
	return 0;
}

