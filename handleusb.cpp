//	handleusb.cpp
//	Open the USB device attached to the OBC. 
//	Set the line discipline and then read lines of text.

#include <string>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <ctime>

using namespace std;



string devPath;


main(int argc, char* argv[])
{
	if ( argc > 1 ) {
		devPath = argv[1];
		cout << "Opening device " << devPath << endl;
	}
	else
	{
		cout << "usage: handleusb device_path" << endl;
		exit(0);
	}

	time_t tm1, tm2;
	time(&tm1);
	FILE* fp;
	while ( (fp = fopen(devPath.c_str(), "r")) == NULL )
	{
		ostringstream errmsg;
		if ( errno != EBUSY && errno != ENOENT )
		{
			errmsg << "errno = " << errno;
			perror(errmsg.str().c_str());
		}
		time(&tm2);
		if ( 20 < difftime(tm2, tm1) )
		{
			errmsg << "OBC not found on " << devPath;
			perror(errmsg.str().c_str());
			exit(-1);
		}
		sleep(2);
	}

	// stty settings:
	// speed 115200 baud; rows 0; columns 0; line = 0;
	// intr = ^C; quit = ^\; erase = ^?; kill = ^U; eof = ^D; eol = <undef>; eol2 = <undef>; swtch = <undef>;
	//  start = ^Q; stop = ^S;
	// susp = ^Z; rprnt = ^R; werase = ^W; lnext = ^V; discard = ^O; min = 1; time = 0;
	// -parenb -parodd -cmspar cs8 -hupcl -cstopb cread clocal -crtscts
	// ignbrk -brkint -ignpar -parmrk -inpck -istrip -inlcr -igncr -icrnl -ixon -ixoff -iuclc -ixany -imaxbel -iutf8
	// -opost -olcuc -ocrnl -onlcr -onocr -onlret -ofill -ofdel nl0 cr0 tab0 bs0 vt0 ff0
	// -isig -icanon -iexten -echo -echoe -echok -echonl noflsh -xcase -tostop -echoprt -echoctl -echoke -flusho -extproc

	string magic{" 1:0:18b2:80:3:1c:7f:15:4:0:1:0:11:13:1a:0:12:f:17:16:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0"};
	string syscmd{"/bin/stty -F " + devPath + magic};
	system(syscmd.c_str());
	

	if ( fp == NULL )
		perror(devPath.c_str());
	else
	{
		int c;
		string inputLine;

		do {
			c = fgetc(fp);
			inputLine += (char) c;
			if ( c == '\n' )
			{
				cout << inputLine;
				inputLine = "";
			}
		} while ( c != EOF );
		fclose(fp);
		cout << inputLine << endl;
	}
}
