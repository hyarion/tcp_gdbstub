#ifndef __target__
#define __target__

/**
 * @returns last known signal value
 */
unsigned char lastsigval(void);

/**
 * @returns true if extended mode could be entered
 */
int enableExtendedMode(void);

void setSignalToUse(unsigned char sigval);

/**
 * @param addr address to continue from, continue from PC if addr < 0
 */
void continueExectuion(target_intptr_t addr);

/**
 * @returns error value if an error occurred, otherwise -1
 */
int detachDebug(void);


#endif
