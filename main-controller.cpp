#include <cstdlib>
#include <iostream>
#include "RF24.h"
#include "RF24Network.h"
#include <ctime>
#include <stdio.h>
#include "PracticalSocket.h"
/**
 * g++ -L/usr/lib main.cc -I/usr/include -o main -lrrd
 **/
using namespace std;

RF24 radio("/dev/spidev0.0",8000000,25);  // Setup for GPIO 25 CSN
RF24Network network(radio);

// Address of our node
const uint16_t this_node = 0;

struct payload_t
{
  char type;
  uint8_t sensor;
  uint8_t value_high;
  uint8_t value_low;
  uint8_t options;
};

void send_payload(char *payload)
{
  try {
    TCPSocket sock("10.38.20.1", 2003);
    sock.send(payload, strlen(payload));

  } catch(SocketException &e) {
    cerr << e.what() << endl;
  }
}

int main(int argc, char** argv) 
{
	float value;
	radio.begin();
	radio.setDataRate(RF24_250KBPS);
	radio.setRetries(7,7);
	delay(5);
	network.begin(/*channel*/ 80, /*node address*/ this_node);
	while(1)
	{
		  network.update();
		  while ( network.available() )
		  {
		    // If so, grab it and print it out
		    RF24NetworkHeader header;
		    network.peek(header);
		    payload_t payload;
		    network.read(header,&payload,sizeof(payload));
		    char dataupload[] ="";
	 	    int16_t _value = (payload.value_high << 4) | payload.value_low;
		    if (payload.type == 'P' or payload.type == 'W' or payload.type == 'G') {
	 	    	value = (float)_value;
		    } else {
     			value = (float)_value/100;
		    }
		    if (payload.options == 1){
		    	sprintf(dataupload,"sensor.net.%i.%c.%i -%.2f -1\n\r",
				header.from_node,payload.type,payload.sensor,value);
		    } else {
			sprintf(dataupload,"sensor.net.%i.%c.%i %.2f -1\n\r",
                                header.from_node,payload.type,payload.sensor,value);
		    }
		    if (header.from_node != 0) {
		    	printf(dataupload);
		    	send_payload(dataupload);
		    }
/*
		    if (header.from_node == 1) {
		    	    payload_weather_t payload;
		    	    network.read(header,&payload,sizeof(payload));
			    printf("N:%.2f:%.2f:%lu\n", payload.temperature, payload.humidity, payload.lux);    
		    }

		    if (header.from_node == 2) {
		    	    payload_power_t payload;
		    	    network.read(header,&payload,sizeof(payload));
			    printf("N:%.2f:%.2f\n", payload.power, payload.current);    
		    }
		    //time_t timer;
    		    //char timeBuffer[25];
    		    //struct tm* tm_info;
    		    //time(&timer);
    		    //tm_info = localtime(&timer);
		    //strftime(timeBuffer, 25, "%Y/%m/%d %H:%M:%S", tm_info);
		    //fprintf(pFile, "%s;%lu;%.2f;%.2f\n", timeBuffer,payload.nodeId, payload.data1, payload.data2);    
*/
		  }
		 delay(100);
	}

	return 0;
}
