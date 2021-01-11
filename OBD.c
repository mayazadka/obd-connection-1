#include "OBD.h"


int scanSerial(Connections* connections)
{	
	writeToLog("-scanning for serial ports");
	int error = SUCCESS;
		

#ifdef _WIN32 

	int i;
	int current = 0;
	char** ports;

	
	HANDLE hCom;
	OVERLAPPED o;
	BOOL fSuccess;
	DWORD dwEvtMask;
	char text[11] = "\\\\.\\COM";

	ports = (char*)malloc(MAX_POTENTIALS_PORTS * sizeof(char));
	if (ports == NULL)
	{
		free(connections);
		return ALLOCATION_ERROR;
	}
		
	//going through all the possible ports (com 0-256)
	for (i = 0; i < 256; i++)
	{
		if (i < 10) { text[7] = i + '0'; }

		else if (i < 100) {
			text[7] = (i / 10) + '0';
			text[8] = (i % 10) + '0';
		}
		else
		{
			text[7] = (i / 100) + '0';
			text[8] = (i / 10) % 10 + '0';
			text[9] = (i % 10) + '0';
		}

		hCom = CreateFile(TEXT(text),
			GENERIC_READ | GENERIC_WRITE,
			0,    // exclusive access 
			NULL, // default security attributes 
			OPEN_EXISTING,
			FILE_FLAG_OVERLAPPED,
			NULL
		);

	

		if (hCom == INVALID_HANDLE_VALUE)
		{
			//
		}
		else
		{
			ports[current] = _strdup(text);
			if (ports[current] == NULL)
			{
				error = ALLOCATION_WARNING;
				break;

			}

			current++;
		}
		if (current == MAX_POTENTIALS_PORTS)
		{
			error = RICHED_MAX_POTENTIALS_PORTS_WARNING;
			break;
		}
	}
	
	connections->list = ports;
	connections->size = current;
#elif __linux__
	glob_t globlist;
	glob("/dev/ttyS[0-9]", 0, NULL, &globlist);
	glob("/dev/ttyUSB[0-9]*", GLOB_APPEND, NULL, &globlist);

	connections->list = globlist.gl_pathv;
	connections->size = globlist.gl_pathc;

#endif
	return error;
}

// -------------------------------------------------------------
// releases the potential serial-port list(possible connections)
// -------------------------------------------------------------

void freeConnections(Connections* connection)
{
	int i;
	
	for(i=0;i<connection->size;i++)
	{
		free(connection->list[i]);
	}
	free(connection->list);
}

//---------------------------------------------------
// trying to initialis communication with the elm327
//---------------------------------------------------

int connect(char* portname, int* pd)
{
	char sys[100] = "sudo chmod 777 ";
	char buf[MAX_MESSEGE_SIZE];
	char arr[MAX_MESSEGE_SIZE];
	int i;
	for(i=0;i<strlen(portname);i++)
	{
		sys[i+15] = portname[i];
	}
	system(sys);
	*pd = open (portname, O_RDWR | O_NOCTTY | O_NDELAY);
	if (*pd < 0)
	{
		strcpy(arr, "-");
		strcat(arr, portname);
		strcat(arr, " is not obd");
		writeToLog(arr);
		return NOT_OBD;
	}

	set_interface_attribs (*pd, B38400, 0);
	set_blocking (*pd, 1);
	
	command(*pd, getCommands().reset, buf);
	if(buf == NULL)
	{
		disconnect(*pd);
		strcpy(arr, "-");
		strcat(arr, portname);
		strcat(arr, " is not obd");
		writeToLog(arr);
		return NOT_OBD;
	}
	
	if(strcmp(buf, "ATZ\n\n\nELM327 v1.5\n\n>") == 0 || strcmp(buf, "\n\nELM327 v1.5\n\n>") == 0) 
	{
		strcpy(arr, "-obd found: ");
		strcat(arr, portname);
		writeToLog(arr);
		return SUCCESS;
	}
	
	disconnect(*pd);
	strcpy(arr, "-unexpected answer from "); 
	strcat(arr, portname);
	writeToLog(arr);
	return NOT_OBD;
}
// --------------------------
// disconnect 
// --------------------------
int disconnect(int pd)
{
	if(close(pd)<0)
	{
		writeToLog("-closing port error");
		return CLOSE_ERROR;
	}
	writeToLog("-port closed");
	return SUCCESS;
}

// --------------------
// send's messege to obd
// --------------------

int send(int pd, char* msg)
{
	int len = strlen(msg), i;
	char text[len+3];
	char arr[MAX_MESSEGE_SIZE];
	for(i=0;i<len;i++)
	{
		text[i] = msg[i];
	}
	text[i] = '\r';	
	if(write (pd, text, strlen(text)) < 0)
	{
		writeToLog("-write to port error");
		return WRITE_ERROR;
	}
	strcpy(arr, "-write: ");
	strcat(arr, msg);
	writeToLog(arr);
	return SUCCESS;
}
// --------------------------------------------------------------------
// reads a massege from the connection that we created with the elm327
// --------------------------------------------------------------------
int recv(int pd, char* msg, int len)
{
	int n;
	char arr[MAX_MESSEGE_SIZE];
	if((n = read (pd, msg, len))<0)	{return READ_ERROR;}
	msg[n] = '\0';
	strcpy(arr, "-read: ");
	strcat(arr, msg);
	writeToLog(arr);
	return SUCCESS;
}

int set_interface_attribs (int fd, int speed, int parity)
{
        struct termios tty;
        if (tcgetattr (fd, &tty) != 0)
        {
		 printf("error0");
                return -1;
        }

        cfsetospeed (&tty, speed);
        cfsetispeed (&tty, speed);

        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
        // disable IGNBRK for mismatched speed tests; otherwise receive break
        // as \000 chars
        tty.c_iflag &= ~IGNBRK;         // disable break processing
        tty.c_lflag = 0;                // no signaling chars, no echo,
                                        // no canonical processing
        tty.c_oflag = 0;                // no remapping, no delays
        tty.c_cc[VMIN]  = 0;            // read doesn't block
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

        tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                        // enable reading
        tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
        tty.c_cflag |= parity;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
        {
		 printf("error1");
                return -1;
        }
        return 0;
}

void set_blocking (int fd, int should_block)

{
        struct termios tty;
        memset (&tty, 0, sizeof(tty));
        if (tcgetattr (fd, &tty) != 0)
        {
		printf("error2");
               return;
        }

        tty.c_cc[VMIN]  = should_block ? 1 : 0;
        tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
		printf("error3");
}

int OBD()
{
	Connections connections;
	int error , i ,pd;
	// --------------------------------------------------------------
	// scan serial ports - checks if there are connected serial ports
	// --------------------------------------------------------------
	error = scanSerial(&connections);
	if (error >= ERROR )	{return 0;}
	else if(connections.size==0)
	{
		writeToLog("-obd is not connected");
		freeConnections(&connections);
		return -1;
	}
	// ------------------------------------------------------------------------------------------------------------------------------------
	// connect to OBD - checks all the serial portsthat we found and finds the one that has ELM327 v1.5 connected , if we cound not fine one we exit
	// -------------------------------------------------------------------------------------------------------------------------------------
	
	for(i=0;i<connections.size;i++)
	{
		error = connect(connections.list[i], &pd);
		if(error == SUCCESS)
			break;
	}
	if(error!=SUCCESS)
	{
		writeToLog("-obd is not connected");
		freeConnections(&connections);
		return -1;
	}
	freeConnections(&connections);
	return pd;
}

Commands getCommands()
{
	Commands commands;

	commands.echo_off="ATE0";
	commands.echo_on="ATE1";
	commands.header_off="ATH0";
	commands.header_on="ATH1";
	commands.reset="ATZ";

	commands.rpm = "010C";
	commands.speed = "010D";
	commands.throtle = "0111";
	commands.fuel = "012F";
	commands.engine_load = "0104";


	return commands;
}

int command(int pd, char* command, char* answer)
{
	int error = SUCCESS;
	char buf[MAX_MESSEGE_SIZE * sizeof(char)];

	error = send(pd, command);
	if(error >= ERROR)	{return error;}

	if(strcmp(command, getCommands().reset)==0)
		sleep(1);

	usleep(10000);

	error = recv(pd, buf, MAX_MESSEGE_SIZE);
	if(error >= ERROR)	{return error;}

	strcpy(answer, buf);
	return SUCCESS;
}