#ifndef __comm__
#define __comm__

struct stubdata {
	unsigned char * inbuffer;
	size_t inbuffersize;
	unsigned char * outbuffer;
	size_t outbuffersize;
};

void runstub(const int port);
void putDebugString(struct stubdata* const s, unsigned char const * const str);

// implementation specific functions
void handlestub(struct stubdata * const s);
size_t getpacket(struct stubdata * const s);
void putDebugChar(struct stubdata * const s, const unsigned char ch);
unsigned char getDebugChar(struct stubdata * const s);

#endif
