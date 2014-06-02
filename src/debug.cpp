#include "debug.h"

#if defined ( NDEBUG )
void PrintMsg( const char* pFormat, ... )
{
	va_list args;
	va_start( args, pFormat );
	(void)vprintf( pFormat, args );
	va_end( args );
}
void LogMsg( FILE* logFilePointer, const char* pFormat, ... )
{
	va_list args;
	va_start( args, pFormat );
	(void)fprintf( logFilePointer, pFormat, args );
	va_end( args );
}
#else
void PrintMsg( const char*, ... )
{
}
void LogMsg( FILE* logFilePointer, const char* pFormat, ... )
{
}
#endif
