#define _GNU_SOURCE // for recvmmsg

#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>


#include <fcntl.h>
#include <signal.h>

#include <bcm2835.h>
#include <time.h>

#define BUFFERLEN 128

// Pulses on RPi Plug P1 pin 11 (which is GPIO pin 17)
#define PULSE_PIN RPI_GPIO_P1_11

// Alive blinking on RPi Plug P1 pin 36 (which is GPIO pin 16)
#define LED_PIN RPI_BPLUS_GPIO_J8_36


int pulse()
{
   struct timespec t1;

   // Turn it on
   bcm2835_gpio_write(PULSE_PIN, HIGH);
   t1.tv_sec = 0;
   t1.tv_nsec = 500000;
   nanosleep(&t1, NULL);

   // turn it off
   bcm2835_gpio_write(PULSE_PIN, LOW);

}

struct net_addr
{
   int ipver;
   struct sockaddr_in sin4;
   struct sockaddr_in6 sin6;
   struct sockaddr *sockaddr;
   int sockaddr_len;
};


int net_gethostbyname(struct net_addr *shost, const char *host, int port)
{
   memset(shost, 0, sizeof(struct net_addr));

   struct in_addr in_addr;
   struct in6_addr in6_addr;

   /* Try ipv4 address first */
   if (inet_pton(AF_INET, host, &in_addr) == 1) {
      goto got_ipv4;
   }

   fprintf(stderr, "Failure in inet_pton\n");
   return -1;

got_ipv4:
   shost->ipver = 4;
   shost->sockaddr = (struct sockaddr*)&shost->sin4;
   shost->sockaddr_len = sizeof(shost->sin4);
   shost->sin4.sin_family = AF_INET;
   shost->sin4.sin_port = htons(port);
   shost->sin4.sin_addr = in_addr;
   return 0;
}

int main_fd = -1;
struct sockaddr_in cliaddr;
int len = sizeof(cliaddr); //len is value/result

int process_messages(char *buffer) {
   const char simple_ack[] = {'A'};

   switch (buffer[0]) {
      case 'C':
	 fprintf(stderr, "Status check\n");
	 sendto(main_fd, simple_ack, 1, MSG_CONFIRM, (const struct sockaddr *) &cliaddr, len);
	 break;
      case 'T':
	 pulse();
	 fprintf(stderr, "Trigger\n");
	 sendto(main_fd, simple_ack, 1, MSG_CONFIRM, (const struct sockaddr *) &cliaddr, len);
	 break;
      default:
	 fprintf(stderr, "Non-handled command, 0x%02x\n", buffer[0]);
	 break;
   }
   return 0;
}

struct timespec ts_last, ts_now;

int check_and_flash() {
   static int state = 0;
   int64_t diff;
   int res = clock_gettime(CLOCK_REALTIME_COARSE, &ts_now);
   if (res < 0) {
      fprintf(stderr, "Error in clock_gettime.\n");
      return(-1);
   }
   else {
      diff = ((int64_t) ts_now.tv_sec - (int64_t) ts_last.tv_sec) * (int64_t)1000000000 
	 + ((int64_t) ts_now.tv_nsec - (int64_t) ts_last.tv_nsec);
      if (diff > 250000000) {
	 if (state) {
	    bcm2835_gpio_write(LED_PIN, HIGH);
	    state = 0;
	 }
	 else {
	    bcm2835_gpio_write(LED_PIN, LOW);
	    state = 1;
	 }
	 ts_last = ts_now;
      }
   }
}


int main()
{
   if (!bcm2835_init())
      return 1;
   // Set the pin to be an output
   bcm2835_gpio_fsel(PULSE_PIN, BCM2835_GPIO_FSEL_OUTP);
   bcm2835_gpio_fsel(LED_PIN, BCM2835_GPIO_FSEL_OUTP);


   const char *listen_host = "0.0.0.0";
   int port = 20782;

   char buffer[BUFFERLEN];

   struct net_addr listen_addr;

   char *hello = "Hello from server";



   int ret = net_gethostbyname(&listen_addr, listen_host, port);
   if (ret < 0)
      return(-1);

   fprintf(stderr, "[*] Starting udpreceiver on %s:20782\n", listen_host);

   // Bind UDP
   main_fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
   if (main_fd < 0) {
      fprintf(stderr, "Error in socket.\n");
      return(-1);
   }
   if (bind(main_fd, listen_addr.sockaddr, listen_addr.sockaddr_len) < 0) {
      fprintf(stderr, "Error in bind.\n");
      return(-1);
   }


   int res = clock_gettime(CLOCK_REALTIME_COARSE, &ts_last);
   if (res < 0) {
      fprintf(stderr, "Error in clock_gettime.\n");
      return(-1);
   }

   while (1) {
      /* Blocking recv. */
      int r = recvfrom(main_fd, (char *)buffer, BUFFERLEN, MSG_DONTWAIT, ( struct sockaddr *) &cliaddr, &len);
      if (r <= 0) {
	 if (errno == EAGAIN || errno == EWOULDBLOCK) {
	    if (check_and_flash() < 0) {
	       return -1;
	    }
	    else
	       continue;
	 }
	 else if (errno == EINTR) {
	    continue;
	 }
	 fprintf(stderr, "Error in recvfrom.\n");
	 return(-1);
      }
      else {
	 printf("Received %d, First byte from client: 0x%02x\n", r, buffer[0]);
	 process_messages(buffer);
      }

   }

   bcm2835_close();
   return(0);
}







