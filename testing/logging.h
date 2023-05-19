#include <stddef.h>
#include <stdio.h>
// #define LOGGING_INFO

#ifdef LOGGING_INFO
	#define OPENLOG() openLog("C:\\Users\\Owner\\Documents\\GitHub\\CTests\\CybercoCli\\.logs\\LOG.txt")
	#define LOG(...) writeLine(__VA_ARGS__)
	#define LOGVAR(var) LOG(#var ": %d", var)
	#define ENDLOG() closeLog();
#else
	#define OPENLOG()
	#define LOG(...)
	#define LOGVAR(var)
	#define ENDLOG()
#endif

void openLog(const char* file);
void writeLine(const char* str, ...);
void closeLog(void);
