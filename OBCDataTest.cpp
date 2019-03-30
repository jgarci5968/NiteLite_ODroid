//  OBCDataTest.cpp
//  Unit tests for OBCData class

// System includes
#include <string>
#include <iostream>
#include <system_error>
#include <thread>
#include <mutex>
#include <cstdio>
#include <unistd.h>


// Local includes
#include "OBCData.h"

// System namespace
using namespace std;


int main(int argc, char* argv[])
{
	char* filename;
	FILE* fp;

	if ( argc > 1 )
	{
		filename = *++argv;
	}
	else
	{
		cerr << "usage: OBCDataTest filename" << endl;
		exit(-1);
	}

	fp = fopen(filename, "r");
	if ( fp == NULL )
	{
		throw system_error{errno, system_category(), filename};
	}

	shared_data.obc_data.obc_mode = true;
	cerr << "shared_data.available: " << shared_data.available << endl;

	thread t1 {read_usb, fp};

	while (true)
	{
		OBCData data;
		shared_data.m.lock();
		if ( shared_data.available )
		{
			data = shared_data.obc_data;
			shared_data.available = false;
		}
		shared_data.m.unlock();
		if ( data.input != "" )
		{
			cout << "read: " << " " << data.input << endl;
			cout << "parsed: " << data.display() << endl;
			cout << "parsed: " << data.getTimeString() << endl;
		}
		//sleep(2);
	}
}
