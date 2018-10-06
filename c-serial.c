#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <string.h>
#include <getopt.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define READ_BUFF_SIZE 16

static int serial_fd = -1;
static char read_buff[READ_BUFF_SIZE];

void process_serial_chars(int in_fd, int out_fd);
void process_stdin_chars(int in_fd, int out_fd);

void sigint_handler(void)
{
   if (serial_fd >= 0)
   {
     char ctrl_c[1] = { 0x03 };

     printf("Sending ctrl_c to serial port\n");

     write(serial_fd, &ctrl_c[0], sizeof(char)*1 );
   }
}

void sigz_handler(void)
{
   if (serial_fd >= 0)
   {
      char ctrl_z[1] = { 0x1A };

      printf("Sending ctrl_z to serial port\n");
      write(serial_fd, &ctrl_z[0], sizeof(char)*1 );
   }
}

void signal_handler(int signum)
{
   switch (signum)
   {
      case SIGTSTP:   sigz_handler(); break;
      case  SIGINT: sigint_handler(); break;
   }
}

int main(int argc, char** argv)
{
   struct termios tc;
   int opt, longindex;
   struct option long_opts[] =
   {
      { "help", 0, 0, 'h' },  // name, has_arg, flag, val
      { "version", 0, 0, 'v' },
      { "port", 1, 0, 'p' },
      { "baud", 1, 0, 'b' },
      { 0, 0, 0, 0 }
   };
   int baud_rate = 115200;
   char* serial_port_string = "/dev/ttyS0";
   fd_set fds;

   /* setup signal handlers */
   signal(SIGINT,signal_handler);  // SIGINT: Interrupt from keyboard
   signal(SIGTSTP,signal_handler); // SIGTSTP: Stop typed at terminal

   while ((opt = getopt_long(argc, argv, "b:hp:v", long_opts, &longindex)) != -1) // : means it needs an argument
   {
      switch (opt)
      {
         case 'h':
            printf("Help:\n");
            printf("   -b --baud [arg]     Set baud rate\n");
            printf("   -h --help           Print this help message.\n");
            printf("   -p --port [arg]     Specify serial port. (ex: /dev/ttyS0)\n");
            printf("   -v --version        Print version.\n\n");
            printf("   \\n~\\n - Close program\n");
            printf("   \\n~b  - Send break\n");

            exit(0);
            break;
         case 'v':
            printf("Version: 0.1\n");
            exit(0);
            break;
         case 'p':
            printf("Setting serial port\n");
            serial_port_string = strdup(optarg);
            break;
         case 'b':
            printf("Setting baud rate\n");
            baud_rate = atoi(optarg);
            break;
      }
   }

   serial_fd = open(serial_port_string, O_RDWR);

   if (serial_fd < 0)
   {
      printf("Error: Could not open %s\n",serial_port_string);
      perror(NULL);
      exit(0);
   }

   /* setup file descriptors to watch for select() */
   FD_ZERO(&fds); // Initialize fds
   FD_SET(STDIN_FILENO, &fds);  // Add file descriptor for STDIN
   FD_SET(serial_fd, &fds);  // Add file descriptor for serial
   
   /* setup TTY attributes for STDIN*/
   tcgetattr(STDIN_FILENO,&tc);
   tc.c_lflag &= ~(ECHO | ICANON); // Disable echo | Disable Canonical mode
                                   // Characters sent to the serial is echoed back, so we don't need to echo what you type in STDIN
                                   // Otherwise, you'll see the character you type twice. 
                                   // In cananical mode, characters are available only after a new line.  You want characters to be available immediately.
   tcsetattr(STDIN_FILENO, TCSANOW, &tc); // TCSANOW: Change occurs immediately

   /* setup TTY attributes for serial*/
   tcgetattr(serial_fd,&tc);
   tc.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP
                  |INLCR|ICRNL|IXON|IXOFF|IGNCR);
                   // IGNBRK: Ignore break -> Don't ignore break
                     // ~(IGNBRK|BRKINT|PARMRK) -> Break = null byte ('\0')
                   // ISTRIP: Strip off eighth bit -> Don't strip off eighth bit
                   // INLCR: Translate NL to CR on input -> Don't translate NL to CR on input
                   // ICRNL: Translate carriage return to newline on input (unless IGNCR is set). -> Don't translate carriage return to newline on input
                   // IXON, IXOFF: Enable XON/XOFF flow control on output. -> Disable XON/XOFF flow control on output
                   // IGNCR: Ignore carriage return -> Don't ignore carriage return
   tc.c_oflag &= ~OPOST;
                   // Enable implementation-defined output processing -> Disable implementation-defined output processing
   tc.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
                   // ECHO: Echo input characters. -> Disable
                   // ECHONL: If ICANON is also set, echo the NL character even if ECHO is not set -> Disable
                   // ICANON: Cananical mode -> Disable (input is availabe immediately, no input processing is performed, line editing is disabled)
                   // ISIG: When any of the characters INTR, QUIT, SUSP, or DSUSP are received, generate the corresponding signal. -> Disable
                   // IEXTEN: Enable implementation-defined input processing -> Disable
   tc.c_cflag &= ~(CSIZE|PARENB);
                  // CSIZE: Character size mask. Values are CS5, CS6, CS7, or CS8. -> Clearing it
                  // PARENB: Enable parity generation on output and parity checking for input. -> Disable
   tc.c_cflag |= CS8;  // Set character size to be 8 bit
   tc.c_cc[VMIN] = 1;  // Minimum number of characters for noncanonical read (MIN).

   switch(baud_rate)
   {
      case 9600:
         tc.c_cflag |= B9600;
         cfsetispeed(&tc, B9600);
         cfsetospeed(&tc, B9600);
         break;
      case 38400:
         tc.c_cflag |= B38400;
         cfsetispeed(&tc, B38400);
         cfsetospeed(&tc, B38400);
         break;
      case 115200:
         tc.c_cflag |= B115200;
         cfsetispeed(&tc, B115200);
         cfsetospeed(&tc, B115200);
         break;
   }

   tcsetattr(serial_fd, TCSANOW, &tc);  // TCSANOW: Change occurs immediately

   printf("Port: %s\nBaud Rate: %d\n",serial_port_string, baud_rate);

   while(1)
   {
      int rc = 0;

      rc = select(serial_fd+1, &fds, NULL, NULL, NULL);

      if (rc > 0)
      {
         if (FD_ISSET(serial_fd, &fds))
         {
            process_serial_chars(serial_fd, STDOUT_FILENO);
         }

         if (FD_ISSET(STDIN_FILENO, &fds))
         {
            process_stdin_chars(STDIN_FILENO, serial_fd);
         }

         FD_ZERO(&fds);
         FD_SET(STDIN_FILENO, &fds);
         FD_SET(serial_fd, &fds);
      }
   }

   return 0;
}

void process_serial_chars(int in_fd, int out_fd)
{
   int count;

   count = read(in_fd,read_buff,READ_BUFF_SIZE);

   if (count > 0)
   {
      write(out_fd,read_buff,count);
   }

   return;
}

void process_stdin_chars(int in_fd, int out_fd)
{
   static unsigned char escape_counter = 0;
   int count;

   count = read(in_fd,read_buff,READ_BUFF_SIZE);

   if (count > 0)
   {
      int i;

      for (i = 0; i < count; i++)
      {
         if (escape_counter == 2)
         {
            if (read_buff[i] == '\n')
            {
               struct termios tc;

               printf("Exit\n");

               /* Reset TTY attributes for STDIN */
               tcgetattr(STDIN_FILENO,&tc);

               tc.c_lflag |= (ECHO | ICANON); 

               tcsetattr(STDIN_FILENO, TCSANOW, &tc);

               // Not sure why, but when I run this program, the "bracketed paste mode" is enabled, so disabling it here
               printf("\e[?2004l");

               exit(0);
            }
            else if (read_buff[i] == 'b')
            {
               printf("send break!\n");
               tcsendbreak(serial_fd, 0);
               escape_counter = 0;
            }
            else
            {
               escape_counter = 0;
            }
         }
         else if (escape_counter == 1)
         {
            if (read_buff[i] == '~')
            {
               escape_counter++;
               continue;
            }
            else
            {
               escape_counter = 0;
            }
         }

         if (escape_counter == 0)
         {
            if (read_buff[i] == '\n')
            {
               escape_counter++;
            }
            else
            {
               escape_counter = 0;
            }
         }
      }

      write(out_fd,read_buff,count);
   }

   return;
}

