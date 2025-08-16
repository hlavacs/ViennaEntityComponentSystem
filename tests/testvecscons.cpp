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
	struct timeval tv{0};
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

void check( bool b, std::string_view msg = "" ) {
	if( b ) {
		//std::cout << "\x1b[32m passed\n";
	} else {
		std::cout << "\x1b[31m failed: " << msg << "\n";
		exit(1);
	}
}

void test_conn() {
	std::cout << "\x1b[37m testing Connection!...\n";

	// 
	// create a populated registry
	vecs::Registry system;
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
		 "Mein kleiner gruener Kaktus",
		 "faehrt morgen ins Buero -",
		 "Hollari",
		 "hollara",
		 "hollaro!",
	#ifndef DEBUG
	     "Wir fahren mit der U-Bahn",
		 "von hier nach anderswo",
		 "Hollari",
		 "hollara",
		 "hollaro!",
		 "Und wenn ein Boesewicht",
		 "was Ungezog'nes spricht",
         "dann sag' ich's meinen Kaktus",
		 "und der sticht, sticht, sticht",
         "Mein kleiner gruener Kaktus",
		 "faehrt gerne ins Buero",
		 "Hollari",
		 "hollara",
		 "hollaro!",
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

#ifdef _DEBUG
	auto comm = vecs::getConsoleComm();
#else
	auto comm = vecs::getConsoleComm(&system);
#endif

	std::cout << "\x1b[37m isConnected: " << comm->isConnected() << "\n";
	bool abortWait { false }, toldya { false };
	while (!comm->isConnected() && !abortWait) {
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
	std::cout << "\x1b[37m isConnected: " << comm->isConnected() << "\n";

	if (comm->isConnected()) {

		// do nothing for 600 seconds, let background task work
		for (int secs = 0; secs < 600; secs++) {
			// test for dynamic scaling of entity graph in Console LiveView
			if (secs < 80) {
				handles.push_back(system.Insert(secs + 1000, static_cast<float>(secs * 7)));
				handles.push_back(system.Insert(secs + 1000, static_cast<float>(secs * 7), static_cast<double>(secs * 5)));

			} else if (secs == 80) {
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
			if (!comm->isConnected())
				break;
		}

		comm->disconnectFromServer();
	}
	std::cout << "\x1b[37m I hope it works? ...\n";
}

int main() {
	std::cout << "testing VECS Console communication...\n";
	test_conn();
	return 0;
}

