#include <cstdlib>
#include <iostream>
#include "RF24.h"
#include "RF24Network.h"
#include <ctime>
#include <stdio.h>
#include "config.h"
#include "PracticalSocket.h"
#include <time.h>

/**
 * g++ -L/usr/lib main.cc -I/usr/include -o main -lrrd
 **/
using namespace std;

RF24 radio("/dev/spidev0.0",8000000,25);  // Setup for GPIO 25 CSN
RF24Network network(radio);

struct payload_config_t 
{
  uint8_t pos;
  uint8_t data;
};

int main(int argc, char** argv) 
{
	radio.begin();
	radio.setDataRate(RF24_250KBPS);
	radio.setRetries(7,7);
	delay(5);
	network.begin(CHANNEL, NODEID);
	network.update();

	char _byte[3];
	printf("Enter byte number:");
	cin >> _byte;
	char _value[3];
	printf("Enter config value:");
	cin >> _value;
	payload_config_t payload = {atoi(_byte),atoi(_value)};
	

	RF24NetworkHeader header(5, 'C');
	if (network.write(header,&payload,sizeof(payload))) {
		printf("\nSending config\n\n");
	}else{
		printf("\nError\n");
	}
	return 0;
}

