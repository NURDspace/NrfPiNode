#include <cstdlib>
#include <iostream>
#include <ctime>
extern "C" {
    #include <signal.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <sys/ioctl.h>
    #include <sys/socket.h>
    #include <sys/time.h>
    #include <netinet/in.h>
    #include <errno.h>
    #include <strings.h>
    #include <stdio.h>
    #include <inttypes.h>
    #include <fcntl.h>
}

#include "config.h"
#include "PracticalSocket.h"

#include "libs/RF24/RF24.h"
#include "libs/RF24Network/RF24Network.h"

#include "NrfPiNode.h"

using namespace std;

// CE Pin, CSN Pin, SPI Speed
RF24 radio(RPI_V2_GPIO_P1_22, BCM2835_SPI_CS0, BCM2835_SPI_SPEED_8MHZ);

RF24Network network(radio);

#define SERVER_PORT  12345

#define TRUE             1
#define FALSE            0

void print_hex(const char *s, size_t len)
{
  for (size_t i = 0; i < len; ++i)
    printf("0x%02x ", (unsigned int) *s++);
  printf("\n");
}

void send_payload(char *payload)
{
  try {
    TCPSocket sock(GRAPHITE_HOST, GRAPHITE_PORT);
    sock.send(payload, strlen(payload));

  } catch(SocketException &e) {
    cerr << e.what() << endl;
  }
}

void radio_setup() {
    radio.begin();
    delay(5);
    network.begin(CHANNEL, NODEID);
    radio.printDetails();
}

char* handle_sensor_metric(RF24NetworkHeader header, payload_t payload, int devide)
{
    char* dataupload = new char[255];
    float value;
    int16_t _value = (payload.value_high << 4) | payload.value_low;
    if (devide > 0)
        value = (float)_value/devide;
    else
        value = (float)_value;

    if (payload.options == 1){
        sprintf(dataupload,"sensor.net.%o.%c.%i -%.2f -1\n\r",
            header.from_node,payload.type,payload.sensor,value);
    } else {
        sprintf(dataupload,"sensor.net.%o.%c.%i %.2f -1\n\r",
            header.from_node,payload.type,payload.sensor,value);
    }
    return dataupload;
}

//Handle radio output
int handle_radio_tx(uint16_t nodeid, char header_type, const void* payload, size_t len)
{
    RF24NetworkHeader header(nodeid, header_type);
    if (network.write(header,&payload,len)) {
        printf("Command send to node: %o len: %i\n",nodeid,len);
        return 1;
    } else {
        printf("Error sending to node: %o\n",nodeid);
        return 0;
    }
}

int is_valid_fd(int fd)
{
    return fcntl(fd, F_GETFL) != -1 || errno != EBADF;
}

void send_to_socket(fd_set _working_set, int _max_sd, char client_payload[255]) {
    int y;
    for (y=0; y <= _max_sd; ++y)
    {
        if (FD_ISSET(y, &_working_set)) {
            signal(SIGPIPE, SIG_IGN);
            if (is_valid_fd(y))
                send(y, client_payload, strlen(client_payload), 0);
        }
    }
}


//Handle the radio input
void handle_radio(fd_set _working_set, int _max_sd) {
    network.update();
    while ( network.available() )
    {
        // If so, grab it and print it out
        RF24NetworkHeader header;
        network.peek(header);
        payload_t payload;
        char* client_payload = new char[255];

        uint16_t replystamp;

        switch ( header.type ) {
            case 'Q':
                //Handle ping reply
                network.read(header,&replystamp,2);
                printf("Received ping reply from %o\n",header.from_node);
                sprintf(client_payload,"Q %o %i\n",header.from_node,replystamp);
                send_to_socket(_working_set, _max_sd,client_payload);
                free(client_payload);
                return;
                break;
        };
        network.read(header,&payload,sizeof(payload));
        switch ( payload.type ) {
            case 'T': //Process temperature
                client_payload = handle_sensor_metric(header,payload,100);
                break;
            case 'P': //Process Pulse
                client_payload = handle_sensor_metric(header,payload,0);
                break;
            case 'W': //Process Water
                client_payload = handle_sensor_metric(header,payload,0);
                break;
            case 'G': //Process Gass
                client_payload = handle_sensor_metric(header,payload,0);
                break;
            case 'A': //Process Analog input
                client_payload = handle_sensor_metric(header,payload,0);
                break;
            case 'H': //Process Humidity
                client_payload = handle_sensor_metric(header,payload,0);
                break;
            case 'B': //Process Battary voltage in V
                client_payload = handle_sensor_metric(header,payload,100);
                break;
            case 'D': //Process Presure sensor
                client_payload = handle_sensor_metric(header,payload,0);
                break;
           default:
                printf("Unknown payload type %c\n",payload.type);
                return;
                break;
        };
        //Send packet from nodes
        if (header.from_node != 0) {
            printf(client_payload);
            send_to_socket(_working_set, _max_sd,client_payload);
            send_payload(client_payload);
        }
        free(client_payload);
    }
}

//Handle incomming packet from tcp socket
void handle_tcp_rx(char buffer[80], int buffer_len)
{
    if (buffer_len < 4)
        return;
    input_msg input_data; 
    memcpy(&input_data, buffer, buffer_len);
    printf("Sending message to\n\tNodeID: %o\n",input_data.nodeid);
    printf("\tHeader type %c\n",input_data.header_type);
    printf("\tBuffer len: %i\n",buffer_len);
    printf("\tPayload len: %i\n",buffer_len-3);
    printf("\tPayload: ");
    print_hex(input_data.payload,buffer_len-3);
    char* configbuffer = new char[2];
    char* pinoutputbuffer = new char[2];
    int* ws2801buffer = new int[5]; 
    size_t ws2801buffer_len = 5; 
    char* stamp = new char[2];

    switch ( input_data.header_type )
    {
        case 'P':
            memcpy(&stamp,input_data.payload,sizeof(stamp));
            printf("Sending ping to %o\n",input_data.nodeid);
            handle_radio_tx(input_data.nodeid,input_data.header_type,(char*)stamp,sizeof(stamp));
            break;
        case 'C':
            printf("Sending configuration to node %o\n",input_data.nodeid);
            memcpy(&configbuffer,input_data.payload,sizeof(configbuffer));
            handle_radio_tx(input_data.nodeid,input_data.header_type,configbuffer,sizeof(configbuffer));
            break;
        case 'O':
            memcpy(&pinoutputbuffer,input_data.payload,sizeof(pinoutputbuffer));
            printf("Sending pin output to node: %o\n",input_data.nodeid);
            handle_radio_tx(input_data.nodeid,input_data.header_type,pinoutputbuffer,sizeof(pinoutputbuffer));
            break;
        case 'W':
            memcpy(&ws2801buffer,input_data.payload,ws2801buffer_len);
            printf("Sending ws2801 output to node: %o len: %i\n",input_data.nodeid,ws2801buffer_len);
            handle_radio_tx(input_data.nodeid,input_data.header_type,ws2801buffer,ws2801buffer_len);
            break;
        default:
            printf("Unknown header type\n");
            break;
    }
    memset(&input_data, 0, sizeof(input_data));
}

int main (int argc, char *argv[])
{
   int    i, len, rc, on = 1;
   int    listen_sd, max_sd, new_sd;
   int    desc_ready, end_server = FALSE;
   int    close_conn;
   char   buffer[80];
   sockaddr_in   addr;
   timeval       timeout;
   fd_set        master_set, working_set;

   radio_setup();

   listen_sd = socket(AF_INET, SOCK_STREAM, 0);
   if (listen_sd < 0)
   {
      perror("socket() failed");
      exit(-1);
   }

   rc = setsockopt(listen_sd, SOL_SOCKET,  SO_REUSEADDR,
                   (char *)&on, sizeof(on));
   if (rc < 0)
   {
      perror("setsockopt() failed");
      close(listen_sd);
      exit(-1);
   }

   rc = ioctl(listen_sd, FIONBIO, (char *)&on);
   if (rc < 0)
   {
      perror("ioctl() failed");
      close(listen_sd);
      exit(-1);
   }

   memset(&addr, 0, sizeof(addr));
   addr.sin_family      = AF_INET;
   addr.sin_addr.s_addr = htonl(INADDR_ANY);
   addr.sin_port        = htons(SERVER_PORT);
   rc = bind(listen_sd,
             (struct sockaddr *)&addr, sizeof(addr));
   if (rc < 0)
   {
      perror("bind() failed");
      close(listen_sd);
      exit(-1);
   }

   rc = listen(listen_sd, 32);
   if (rc < 0)
   {
      perror("listen() failed");
      close(listen_sd);
      exit(-1);
   }

   FD_ZERO(&master_set);
   max_sd = listen_sd;
   FD_SET(listen_sd, &master_set);

   do
   {
      memcpy(&working_set, &master_set, sizeof(master_set));

      timeout.tv_sec  = 0;
      timeout.tv_usec = 500;

      rc = select(max_sd + 1, &working_set, NULL, NULL, &timeout);

      if (rc < 0)
      {
         perror("  select() failed");
         break;
      }

      if (rc == 0)
      {
         handle_radio(master_set, max_sd);
      }

      desc_ready = rc;
      for (i=0; i <= max_sd  &&  desc_ready > 0; ++i)
      {
         if (FD_ISSET(i, &working_set))
         {
            desc_ready -= 1;
            if (i == listen_sd)
            {
               do
               {
                  new_sd = accept(listen_sd, NULL, NULL);
                  if (new_sd < 0)
                  {
                     if (errno != EWOULDBLOCK)
                     {
                        perror("  accept() failed");
                        end_server = TRUE;
                     }
                     break;
                  }

                  FD_SET(new_sd, &master_set);
                  if (new_sd > max_sd)
                     max_sd = new_sd;

               } while (new_sd != -1);
            }
            else
            {
               close_conn = FALSE;
               do
               {
                  rc = recv(i, buffer, sizeof(buffer), 0);
                  if (rc < 0)
                  {
                     if (errno != EWOULDBLOCK)
                     {
                        perror("  recv() failed");
                        close_conn = TRUE;
                     }
                     break;
                  }

                  if (rc == 0)
                  {
                     close_conn = TRUE;
                     break;
                  }

                  len = rc;
                  printf("  %d bytes received\n", len);
                    
                  handle_tcp_rx(buffer,len);
                  memset(&buffer[0], 0, sizeof(buffer));
                  break;

               } while (TRUE);

               if (close_conn)
               {
                  close(i);
                  FD_CLR(i, &master_set);
                  if (i == max_sd)
                  {
                     while (FD_ISSET(max_sd, &master_set) == FALSE)
                        max_sd -= 1;
                  }
               }
            }
         }
      }

   } while (end_server == FALSE);

   for (i=0; i <= max_sd; ++i)
   {
      if (FD_ISSET(i, &master_set))
         close(i);
   }
}

