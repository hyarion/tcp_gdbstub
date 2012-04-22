#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "target.h"

static const char hexchars[]="0123456789abcdef";

void runstub(int port);

// TODO move this to another file
int main(int argc, char ** argv)
{
	int port;
	if (argc > 1) {
		port = atoi(argv[1]);
	} else {
		port = 8112;
	}
	runstub(port);
	return 0;
}

//public
struct stubdata {
	unsigned char * inbuffer;
	size_t inbuffersize;
	unsigned char * outbuffer;
	size_t outbuffersize;
};
//private
struct socket_stubdata {
	struct stubdata s;
	int fd;
};

static void putDebugChar(struct stubdata * const s, const unsigned char ch) {
	struct socket_stubdata* ss = (struct socket_stubdata*)s;
	assert(write(ss->fd, &ch, 1)==1);
}
static unsigned char getDebugChar(struct stubdata * const s) {
	unsigned char ch;
	struct socket_stubdata* ss = (struct socket_stubdata*)s;
	int didread = recv(ss->fd, &ch, 1, 0);
	if (1!=didread) {
		if (didread >= 1) {
			fprintf(stderr, "read %d bytes\n",didread);
			fflush(stderr);
		} else if (didread == 0){
			assert(!"debugger kicked us out :(");
		} else {
			assert(1==didread);
		}
	}
	fflush(stdout);
	return ch;
}

static int hex(const unsigned char ch)
{
	if (ch >= 'a' && ch <= 'f')
		return ch-'a'+10;
	if (ch >= '0' && ch <= '9')
		return ch-'0';
	if (ch >= 'A' && ch <= 'F')
		return ch-'A'+10;
	return -1;
}
/*
 * While we find nice hex chars, build an int.
 * Return number of chars processed.
 */
static int hexTo_target_intptr_t(char const**const ptr, target_intptr_t *const intptrValue)
{
	int numChars = 0;
	int hexValue;

	*intptrValue = 0;

	while (**ptr) {
		hexValue = hex(**ptr);
		if (0 > hexValue) {
			break;
		}
		*intptrValue = (*intptrValue << 4) | hexValue;
		numChars++;
		(*ptr)++;
	}
	return numChars;
}
static int hexTo_int(char const**const ptr, int *const intValue)
{
	int numChars = 0;
	int hexValue;

	*intValue = 0;

	while (**ptr) {
		hexValue = hex(**ptr);
		if (0 > hexValue) {
			break;
		}
		*intValue = (*intValue << 4) | hexValue;
		numChars++;
		(*ptr)++;
	}
	return numChars;
}

static void putpacket(struct stubdata * const s)
{
	struct socket_stubdata* const ss = (struct socket_stubdata*)s;
	int count;
	unsigned char ch;
	unsigned char checksum;
	unsigned char* const buffer = malloc(sizeof(unsigned char)*s->outbuffersize);
	unsigned char* ptr = buffer;
	count    =  0;
	*ptr++   = '$';
	checksum =  0;
	while ((ch = s->outbuffer[count++])) {
		*ptr++    = ch;
		checksum += ch;
	}
	*ptr++ = '#';
	*ptr++ = hexchars[checksum >>  4];
	*ptr++ = hexchars[checksum & 0xf];
	*ptr = 0;
	do {
		write(ss->fd, buffer, ptr-buffer);
		printf("put: %s\n",buffer); // for debug, this line can be removed
	} while ('+' != getDebugChar(s));
	free(buffer);
}

//more or less from gdb's sparc-stub
static size_t getpacket(struct stubdata * const s) {
	unsigned char ch;
	unsigned char checksum;
	unsigned char xmitcsum;
	s->inbuffersize = 0;
	for (;;) {
		while ('$' != (ch = getDebugChar(s))) {}
		int first =  1;
retry:
		checksum  =  0;
		xmitcsum  = -1;
		while (s->inbuffersize < IN_BUFFER_SIZE) {
			if (first) {
				first = 0;
			} else {
				ch = getDebugChar(s); // TODO is this needed?
			}
			if ('$' == ch) { // start marker
				goto retry;
			}
			if ('#' == ch) { // end marker
				break;
			}
			checksum = (checksum + ch) & 0xff;
			s->inbuffer[s->inbuffersize++] = ch;
		}
		s->inbuffer[s->inbuffersize] = 0;
		
		if ('#' == ch) { // calculate checksum
			ch = getDebugChar(s);
			xmitcsum = hex(ch) << 4;
			ch = getDebugChar(s);
			xmitcsum += hex(ch);
			if (checksum != xmitcsum) {
				putDebugChar(s, '-'); /* checksum failed */
			} else {
				putDebugChar(s, '+'); /* successful transfer */
				if (':' == s->inbuffer[2]) {
					putDebugChar(s, s->inbuffer[0]);
					putDebugChar(s, s->inbuffer[1]);
					return 3;
				}
				return 0;
			}
		}
	}
	return 0;
}
void handleUnknownPackage(struct stubdata * const s, const unsigned char * const ptr) {
	printf("unknown package: %s\n", ptr);
	strcpy((char*)s->outbuffer, "OK");
}
void createStopReplyPacket(struct stubdata * const s) {
	const unsigned char sigval = lastsigval();
	s->outbuffer[0] = 'S';
	s->outbuffer[1] = hexchars[sigval >>  4];
	s->outbuffer[2] = hexchars[sigval & 0xf];
	s->outbuffer[3] = 0;
	//TODO handle breakpoints and watchpoints
}
void handlestub(struct stubdata * const s) {
	while (1) {
		s->outbuffer[0] = 0;
		unsigned char const * ptr = s->inbuffer;
		ptr += getpacket(s);
		printf("got: %s\n",ptr); // for debug, this line can be removed
		switch (*ptr++) {
		case '?': { // last signal value
			createStopReplyPacket(s);
			break;
		}
		case '!': // enable extended mode
			if (enableExtendedMode()) {
				strcpy((char*)s->outbuffer, "OK");
			}
			break;
		case 'C': { // continue with signal
			int signal;
			if (hexTo_int((char const**)&ptr, &signal)) {
				setSignalToUse((unsigned char)signal);
			}
			//break; <--- we don't want this as we want to use the 'c' definition
		}
		case 'c': { // continue execution
			target_intptr_t addr;
			if (!hexTo_target_intptr_t((char const**)&ptr, &addr)) {
				addr = -1;
			}
			continueExectuion(addr); // don't return until target halts
			createStopReplyPacket(s);
			break;
		}
		case 'D': { // detach debugger
			int error = detachDebug();
			if (error < 0) {
				strcpy((char*)s->outbuffer, "OK");
			} else {
				s->outbuffer[0] = 'E';
				s->outbuffer[1] = hexchars[error >>  4];
				s->outbuffer[2] = hexchars[error & 0xf];
				s->outbuffer[3] = 0;
			}
			putpacket(s); // force put packet since we are bailing out!
			return;
		}
		case 'H': { // select thread, just ignore that for now
			strcpy((char*)s->outbuffer, "OK");
			break;
		}

		case 'A': { // set arguments
			/*
			size_t streamlen;
			size_t argc;
			char** argv;
			if (hexToInt(&ptr, &streamlen) &&
				hexToInt(&ptr, &argc))
			{
				argv = malloc(sizeof(char*) * argc);
				argv = malloc(MAX_ARGSIZE)
				for (int i=0; i<argc; ++i) {
					memcpy(argv)
				}
			}
			*/
		}
		case 'B': // set breakpoint, should use Z and z instead
		case 'b': // set baud (avoid, question marks in spec)
		//    bc  // backwards continue
		//    bs  // backwards step
		case 'd': // toggle debug flag (deprecated)
		default:
			handleUnknownPackage(s, ptr-1); // rewind one in buffer
		}
		putpacket(s);
	}
}

void runstub(int port) {
	struct sockaddr_in stSockAddr;
	int SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	if(-1 == SocketFD)
	{
		perror("can not create socket");
		exit(EXIT_FAILURE);
	}

	memset(&stSockAddr, 0, sizeof(stSockAddr));

	stSockAddr.sin_family = AF_INET;
	stSockAddr.sin_port = htons(port);
	stSockAddr.sin_addr.s_addr = INADDR_ANY;

	if(-1 == bind(SocketFD,(struct sockaddr *)&stSockAddr, sizeof(stSockAddr)))
	{
		perror("error bind failed");
		close(SocketFD);
		exit(EXIT_FAILURE);
	}

	if(-1 == listen(SocketFD, 10))
	{
		perror("error listen failed");
		close(SocketFD);
		exit(EXIT_FAILURE);
	}

	for(;;)
	{
		int ConnectFD = accept(SocketFD, NULL, NULL);

		if(0 > ConnectFD)
		{
			perror("error accept failed");
			close(SocketFD);
			exit(EXIT_FAILURE);
		}

		/* perform read write operations ... 
		   read(ConnectFD,buff,size)*/
		union {
			struct stubdata s;
			struct socket_stubdata ss;
		} u;
		u.ss.fd = ConnectFD;
		u.s.inbuffer  = malloc(sizeof(char) * IN_BUFFER_SIZE);
		u.s.outbuffer = malloc(sizeof(char) * OUT_BUFFER_SIZE);
		u.s.inbuffersize  = 0;
		u.s.outbuffersize = 0;
		// TODO handle malloc errors
		handlestub(&u.s);
		free(u.s.inbuffer);
		free(u.s.outbuffer);

		if (-1 == shutdown(ConnectFD, SHUT_RDWR))
		{
			perror("can not shutdown socket");
			close(ConnectFD);
			exit(EXIT_FAILURE);
		}
		close(ConnectFD);
	}
}
