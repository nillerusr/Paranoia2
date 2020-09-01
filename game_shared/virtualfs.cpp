//=======================================================================
//			Copyright (C) XashXT Group 2014
//		  virtualfs.h - Virtual FileSystem that writes into memory 
//=======================================================================
#include "port.h"
#include <stdarg.h>
#include "mathlib.h"
#include "stringlib.h"
#include "virtualfs.h"

size_t CVirtualFS :: Printf( const char *fmt, ... )
{
	size_t result;
	va_list args;

	va_start( args, fmt );
	result = VPrintf( fmt, args );
	va_end( args );

	return result;
}

size_t CVirtualFS :: IPrintf( const char *fmt, ... )
{
	size_t result;
	va_list args;

	va_start( args, fmt );
	result = IVPrintf( fmt, args );
	va_end( args );

	return result;
}
