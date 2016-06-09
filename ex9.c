
//SOFTWARE FOR UART COMMUNICATION ON THE OVERO-FIRE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <fcntl.h>
#include <termios.h>
#include  <signal.h> //catch ctrl-c interrupt

#define BAUD B115200
#define UART_DEVICE "/dev/ttyO0"

void     INThandler(int); //ctrl-c handler

int fd = 0; //the UART
int bytes_in_data=1; //defines the size of the uart buffer (assignment 9 make frame size)
char data[bytes_in_data] = {0x01}; 

//PROTOTYPES
void main()
void open_uart(void)

void main(){
	open_uart(); // open uart
	signal(SIGINT, INThandler);
	send();
	close(fd); // close uart
}

/*
FUNCTION[send]: sends info over the uart requires open uart
SOURCE/INSPIRATION: shan: http://www.codeproject.com/Questions/718340/C-program-to-Linux-Serial-port-read-write
*/
void send(){
	int write(fd, data, bytes_in_data)
}

/*
FUNCTION[open]: Opens UART specified in the defined parameters
SOURCE/INSPIRATION: shan: http://www.codeproject.com/Questions/718340/C-program-to-Linux-Serial-port-read-write
*/
void open_uart(void){
	fd = open(UART_DEVICE, O_RDWR | O_NOCTTY |O_NDELAY ); // open uart
	if(fd < 0){ perror(UART_DEVICE); exit(-1);} // check if uart was opened
	fcntl(fd,F_SETFL,0);
	tcgetattr(fd,&oldtp); /* save current serial port settings */
	//tcgetattr(fd,&newtp); /* save current serial port settings */
	bzero(&newtp, sizeof(newtp));
	//bzero(&oldtp, sizeof(oldtp));  
	newtp.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;  //CS8 -> 8 data bits 1 stop bit
	newtp.c_iflag = IGNPAR | ICRNL;
	newtp.c_oflag = 0;  
	newtp.c_lflag = ICANON;  
	newtp.c_cc[VINTR]    = 0;     /* Ctrl-c */
	newtp.c_cc[VQUIT]    = 0;     /* Ctrl-\ */
	newtp.c_cc[VERASE]   = 0;     /* del */
	newtp.c_cc[VKILL]    = 0;     /* @ */
	//newtp.c_cc[VEOF]   = 4;     /* Ctrl-d */
	newtp.c_cc[VEOF]     = 0;     /* Ctrl-d */
	newtp.c_cc[VTIME]    = 0;     /* inter-character timer unused */
	newtp.c_cc[VMIN]     = 1;     /* blocking read until 1 character arrives */
	newtp.c_cc[VSWTC]    = 0;     /* '\0' */
	newtp.c_cc[VSTART]   = 0;     /* Ctrl-q */
	newtp.c_cc[VSTOP]    = 0;     /* Ctrl-s */
	newtp.c_cc[VSUSP]    = 0;     /* Ctrl-z */
	newtp.c_cc[VEOL]     = 0;     /* '\0' */
	newtp.c_cc[VREPRINT] = 0;     /* Ctrl-r */
	newtp.c_cc[VDISCARD] = 0;     /* Ctrl-u */
	newtp.c_cc[VWERASE]  = 0;     /* Ctrl-w */
	newtp.c_cc[VLNEXT]   = 0;     /* Ctrl-v */
	newtp.c_cc[VEOL2]    = 0;     /* '\0' */ 
	//tcflush(fd, TCIFLUSH);
	//tcsetattr(fd,TCSANOW,&newtp);
}

void  INThandler(int sig)
{
     char  c;

     signal(sig, SIG_IGN);
     printf("OUCH, did you hit Ctrl-C?\n"
            "Do you really want to quit? [y/n] ");
     c = getchar();
     if (c == 'y' || c == 'Y')
        printf("Cashew Yoghurt says bye");  
		close(fd);
		exit(0);
     else
          signal(SIGINT, INThandler);
     getchar(); // Get new line character
}