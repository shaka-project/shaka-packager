#include "happyhttp.h"
#include <cstdio>
#include <cstring>

#ifdef WIN32
#include <winsock2.h>
#endif // WIN32

int count=0;

void OnBegin( const happyhttp::Response* r, void* userdata )
{
	printf( "BEGIN (%d %s)\n", r->getstatus(), r->getreason() );
	count = 0;
}

void OnData( const happyhttp::Response* r, void* userdata, const unsigned char* data, int n )
{
	fwrite( data,1,n, stdout );
	count += n;
}

void OnComplete( const happyhttp::Response* r, void* userdata )
{
	printf( "COMPLETE (%d bytes)\n", count );
}



void Test1()
{
	puts("-----------------Test1------------------------" );
	// simple simple GET
	happyhttp::Connection conn( "scumways.com", 80 );
	conn.setcallbacks( OnBegin, OnData, OnComplete, 0 );

	conn.request( "GET", "/happyhttp/test.php", 0, 0,0 );

	while( conn.outstanding() )
		conn.pump();
}



void Test2()
{
	puts("-----------------Test2------------------------" );
	// POST using high-level request interface

	const char* headers[] = 
	{
		"Connection", "close",
		"Content-type", "application/x-www-form-urlencoded",
		"Accept", "text/plain",
		0
	};

	const char* body = "answer=42&name=Bubba";
	
	happyhttp::Connection conn( "scumways.com", 80 );
	conn.setcallbacks( OnBegin, OnData, OnComplete, 0 );
	conn.request( "POST",
			"/happyhttp/test.php",
			headers,
			(const unsigned char*)body,
			strlen(body) );

	while( conn.outstanding() )
		conn.pump();
}

void Test3()
{
	puts("-----------------Test3------------------------" );
	// POST example using lower-level interface

	const char* params = "answer=42&foo=bar";
	int l = strlen(params);

	happyhttp::Connection conn( "scumways.com", 80 );
	conn.setcallbacks( OnBegin, OnData, OnComplete, 0 );

	conn.putrequest( "POST", "/happyhttp/test.php" );
	conn.putheader( "Connection", "close" );
	conn.putheader( "Content-Length", l );
	conn.putheader( "Content-type", "application/x-www-form-urlencoded" );
	conn.putheader( "Accept", "text/plain" );
	conn.endheaders();
	conn.send( (const unsigned char*)params, l );

	while( conn.outstanding() )
		conn.pump();
}




int main( int argc, char* argv[] )
{
#ifdef WIN32
    WSAData wsaData;
    int code = WSAStartup(MAKEWORD(1, 1), &wsaData);
	if( code != 0 )
	{
		fprintf(stderr, "shite. %d\n",code);
		return 0;
	}
#endif //WIN32
	try
	{
		Test1();
		Test2();
		Test3();
	}

	catch( happyhttp::Wobbly& e )
	{
		fprintf(stderr, "Exception:\n%s\n", e.what() );
	}
	
#ifdef WIN32
    WSACleanup();
#endif // WIN32

	return 0;
}



