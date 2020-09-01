//=======================================================================
//			Copyright (C) XashXT Group 2014
//		  virtualfs.h - Virtual FileSystem that writes into memory 
//=======================================================================
#ifndef VIRTUALFS_H
#define VIRTUALFS_H

#include <stdio.h>
#include <stdarg.h>
#define FS_MEM_BLOCK	65535
#define FS_MSG_BLOCK	8192

class CVirtualFS
{
public:
	CVirtualFS();
	CVirtualFS( const byte *file, size_t size );
	~CVirtualFS();

	size_t Read( void *out, size_t size );
	size_t Write( const void *in, size_t size );
	size_t Insert( const void *in, size_t size );
	size_t Print( const char *message );
	size_t IPrint( const char *message );
	size_t Printf( const char *fmt, ... );
	size_t IPrintf( const char *fmt, ... );
	size_t VPrintf( const char *fmt, va_list ap );
	size_t IVPrintf( const char *fmt, va_list ap );
	char *GetBuffer( void ) { return (char *)m_pBuffer; };
	size_t GetSize( void ) { return m_iLength; };
	size_t Tell( void ) { return m_iOffset; }
	bool Eof( void ) { return (m_iOffset == m_iLength) ? true : false; }
	int Seek( size_t offset, int whence );
	int Gets( char *string, size_t size );
	int Getc( void );
private:
	byte	*m_pBuffer;	// file buffer
	size_t	m_iBuffSize;	// real buffer size
	size_t	m_iOffset;	// buffer position
	size_t	m_iLength;	// buffer current size
};

_forceinline CVirtualFS :: CVirtualFS()
{
	m_iBuffSize = FS_MEM_BLOCK; // can be resized later
	m_pBuffer = new byte[m_iBuffSize];
	memset( m_pBuffer, 0, m_iBuffSize );
	m_iLength = m_iOffset = 0;
}

_forceinline CVirtualFS :: CVirtualFS( const byte *file, size_t size )
{
	if( !file || size <= 0 )
	{
		m_iBuffSize = m_iOffset = m_iLength = 0;
		m_pBuffer = NULL;
		return;
          }

	m_iLength = m_iBuffSize = size;
	m_pBuffer = new byte[m_iBuffSize];	
	memcpy( m_pBuffer, file, m_iBuffSize );
	m_iOffset = 0;
}

_forceinline CVirtualFS :: ~CVirtualFS()
{
	delete [] m_pBuffer;
}

_forceinline size_t CVirtualFS :: Read( void *out, size_t size )
{
	if( !m_pBuffer || !out || size <= 0 )
		return 0;

	// check for enough room
	if( m_iOffset >= m_iLength )
		return 0; // hit EOF

	size_t read_size = 0;

	if( m_iOffset + size <= m_iLength )
	{
		memcpy( out, m_pBuffer + m_iOffset, size );
		m_iOffset += size;
		read_size = size;
	}
	else
	{
		int reduced_size = m_iLength - m_iOffset;
		memcpy( out, m_pBuffer + m_iOffset, reduced_size );
		m_iOffset += reduced_size;
		read_size = reduced_size;
	}

	return read_size;
}

_forceinline size_t CVirtualFS :: Write( const void *in, size_t size )
{
	if( !m_pBuffer ) return -1;

	if( m_iOffset + size >= m_iBuffSize )
	{
		size_t newsize = m_iOffset + size + FS_MEM_BLOCK;

		if( m_iBuffSize < newsize )
		{
			// reallocate buffer now
			m_pBuffer = (byte *)realloc( m_pBuffer, newsize );
			memset( m_pBuffer + m_iBuffSize, 0, newsize - m_iBuffSize );
			m_iBuffSize = newsize; // update buffsize
		}
	}

	// write into buffer
	memcpy( m_pBuffer + m_iOffset, in, size );
	m_iOffset += size;

	if( m_iOffset > m_iLength ) 
		m_iLength = m_iOffset;

	return m_iLength;
}

_forceinline size_t CVirtualFS :: Insert( const void *in, size_t size )
{
	if( !m_pBuffer ) return -1;

	if( m_iLength + size >= m_iBuffSize )
	{
		size_t newsize = m_iLength + size + FS_MEM_BLOCK;

		if( m_iBuffSize < newsize )
		{
			// reallocate buffer now
			m_pBuffer = (byte *)realloc( m_pBuffer, newsize );
			memset( m_pBuffer + m_iBuffSize, 0, newsize - m_iBuffSize );
			m_iBuffSize = newsize; // update buffsize
		}
	}

	// backup right part
	size_t rp_size = m_iLength - m_iOffset;
	byte *backup = new byte[rp_size];
	memcpy( backup, m_pBuffer + m_iOffset, rp_size );

	// insert into buffer
	memcpy( m_pBuffer + m_iOffset, in, size );
	m_iOffset += size;

	// write right part buffer
	memcpy( m_pBuffer + m_iOffset, backup, rp_size );
	delete [] backup;

	if(( m_iOffset + rp_size ) > m_iLength ) 
		m_iLength = m_iOffset + rp_size;

	return m_iLength;
}

_forceinline size_t CVirtualFS :: Print( const char *message )
{
	return Write( message, Q_strlen( message ));
}

_forceinline size_t CVirtualFS :: IPrint( const char *message )
{
	return Insert( message, Q_strlen( message ));
}

_forceinline size_t CVirtualFS :: VPrintf( const char *fmt, va_list ap )
{
	size_t	buff_size = FS_MSG_BLOCK;
	char	*tempbuff;
	size_t	len;

	while( 1 )
	{
		tempbuff = new char[buff_size];
		len = Q_vsprintf( tempbuff, fmt, ap );
		if( len >= 0 && len < buff_size )
			break;
		delete [] tempbuff;
		buff_size <<= 1;
	}

	len = Write( tempbuff, len );
	delete [] tempbuff;

	return len;
}

_forceinline size_t CVirtualFS :: IVPrintf( const char *fmt, va_list ap )
{
	size_t	buff_size = FS_MSG_BLOCK;
	char	*tempbuff;
	size_t	len;

	while( 1 )
	{
		tempbuff = new char[buff_size];
		len = Q_vsprintf( tempbuff, fmt, ap );
		if( len >= 0 && len < buff_size )
			break;
		delete [] tempbuff;
		buff_size <<= 1;
	}

	len = Insert( tempbuff, len );
	delete [] tempbuff;

	return len;
}

_forceinline int CVirtualFS :: Getc( void )
{
	char c;

	if( !Read( &c, 1 ))
		return EOF;
	return (byte)c;
}

_forceinline int CVirtualFS :: Gets( char *string, size_t size )
{
	size_t	end = 0;
	int	c;

	while( 1 )
	{
		c = Getc();

		if( c == '\r' || c == '\n' || c < 0 )
			break;

		if( end < ( size - 1 ))
			string[end++] = c;
	}

	string[end] = 0;

	// remove \n following \r
	if( c == '\r' )
	{
		c = Getc();
		if( c != '\n' ) Seek( -1, SEEK_CUR ); // rewind
	}

	return c;
}

_forceinline int CVirtualFS :: Seek( size_t offset, int whence )
{
	// Compute the file offset
	switch( whence )
	{
	case SEEK_CUR:
		offset += m_iOffset;
		break;
	case SEEK_SET:
		break;
	case SEEK_END:
		offset += m_iLength;
		break;
	default: 
		return -1;
	}

	if(( offset < 0 ) || ( offset > m_iLength ))
		return -1;

	m_iOffset = offset;

	return 0;
}

#endif//VIRTUALFS_H
