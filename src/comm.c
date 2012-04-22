// more or less from gdb's sparc-stub

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "target.h"
#include "comm.h"

static const char hexchars[]="0123456789abcdef";

static int hex(const unsigned char ch) {
	if (ch >= 'a' && ch <= 'f')
		return ch-'a'+10;
	if (ch >= '0' && ch <= '9')
		return ch-'0';
	if (ch >= 'A' && ch <= 'F')
		return ch-'A'+10;
	return -1;
}

union intval {
	int8_t   t_int8;
	int16_t  t_int16;
	int32_t  t_int32;

	uint8_t  t_uint8;
	uint16_t t_uint16;
	uint32_t t_uint32;

	target_intmax_t      t_intmax;

	target_intptr_t      t_intptr;
	target_uintptr_t     t_uintptr;

	target_intlargest_t  t_intlargest;
	target_uintlargest_t t_uintlargest;
};
/*
 * While we find nice hex chars, build an int.
 * Return number of chars processed.
 */
static int hexToInt(char const**const ptr, union intval* const intValuePtr) {
	int numChars = 0;
	int hexValue;

	intValuePtr->t_intlargest = 0;

	while (**ptr) {
		hexValue = hex(**ptr);
		if (0 > hexValue) {
			break;
		}
		intValuePtr->t_intlargest = (intValuePtr->t_intlargest << 4) | hexValue;
		numChars++;
		(*ptr)++;
	}
	return numChars;
}

size_t getpacket(struct stubdata * const s) {
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

static void createStopReplyPacket(struct stubdata * const s) {
	const unsigned char sigval = lastsigval();
	s->outbuffer[0] = 'S';
	s->outbuffer[1] = hexchars[sigval >>  4];
	s->outbuffer[2] = hexchars[sigval & 0xf];
	s->outbuffer[3] = 0;
	//TODO handle breakpoints and watchpoints
}

static void handleUnknownPackage(struct stubdata * const s, const unsigned char * const ptr) {
	printf("unknown package: %s\n", ptr);
	strcpy((char*)s->outbuffer, "OK");
}

static void putpacket(struct stubdata * const s) {
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
		putDebugString(s, buffer);
		printf("put: %s\n",buffer); // for debug, this line can be removed
	} while ('+' != getDebugChar(s));
	free(buffer);
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
			union intval signal;
			if (hexToInt((char const**)&ptr, &signal)) {
				setSignalToUse(signal.t_uint8);
			}
			//break; <--- we don't want this as we want to use the 'c' definition
		}
		case 'c': { // continue execution
			union intval addr;
			if (!hexToInt((char const**)&ptr, &addr)) {
				addr.t_intptr = -1;
			}
			continueExectuion(addr.t_intptr); // don't return until target halts
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


