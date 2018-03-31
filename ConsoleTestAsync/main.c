#include	<stdio.h>
#include	<Windows.h>


int		main()
{
	HANDLE	hFile;
	char	Buffer[3][10] = { 0 };
	DWORD	dwRet[3] = { 0 };
	OVERLAPPED	ol[3] = { 0 };
	HANDLE	hEvent[3] = { 0 };

	hFile = CreateFileA(
		"\\\\.\\Async0" ,
		GENERIC_READ ,
		FILE_SHARE_READ ,
		NULL , OPEN_EXISTING ,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED ,	//指定为异步方式
		NULL );
	if (INVALID_HANDLE_VALUE == hFile)
		return	0;

	hEvent[0] = CreateEvent( NULL , TRUE , FALSE , NULL );	//event用来通知请求完成
	ol[0].hEvent = hEvent[0];

	hEvent[1] = CreateEvent( NULL , TRUE , FALSE , NULL );	//event用来通知请求完成
	ol[1].hEvent = hEvent[1];

	hEvent[2] = CreateEvent( NULL , TRUE , FALSE , NULL );	//event用来通知请求完成
	ol[2].hEvent = hEvent[2];

	ReadFile( hFile , Buffer[0] , 10 , &dwRet[0] , &ol[0] );
	ReadFile( hFile , Buffer[1] , 10 , &dwRet[1] , &ol[1] );
	ReadFile( hFile , Buffer[2] , 10 , &dwRet[2] , &ol[2] );

	//do some other work here

	WaitForMultipleObjects( 3 , hEvent , TRUE , INFINITE );


	printf( "TestAsyncRead-----\nRead Buffer1: %s\n\
							Read Buffer2: %s\n\
							Read Buffer3: %s\n" ,
		Buffer[0] , Buffer[1] , Buffer[2] );

	CloseHandle( hFile );

	return	0;
}