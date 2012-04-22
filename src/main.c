#include <stdlib.h>
#include "comm.h"

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

