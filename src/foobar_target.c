#include <stdio.h>
unsigned char lastsigval(void)
{
	printf("returning last signal value 1\n");
	return 1;
}
int enableExtendedMode(void)
{
	printf("enabling extended mode\n");
	return 1;
}
void setSignalToUse(unsigned char sigval)
{
	printf("sets signal to%d\n",sigval);
}
void continueExectuion(target_intptr_t addr)
{
	if (addr < 0) {
		printf("continues from old value on PC\n");
	}
	printf("continues from address %lx\n", addr);
}
int detachDebug(void)
{
	printf("detaches debugger, and returns 1\n");
	return 1;
}

