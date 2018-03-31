#include	<stdio.h>
#include	<Windows.h>

int		main()
{
	HANDLE	hFile;
	char	Buffer[10] = { 0 };
	DWORD	dwRet = 0;
	BOOL	bRet;

	hFile = CreateFileA(
		"\\\\.\\Sync0" ,
		GENERIC_READ ,
		FILE_SHARE_READ ,
		NULL , OPEN_EXISTING ,
		FILE_ATTRIBUTE_NORMAL ,
		NULL );
	if (hFile == INVALID_HANDLE_VALUE)
		return	0;

	bRet = ReadFile( hFile , Buffer , 10 , &dwRet , NULL );
	if (!bRet)
	{
		CloseHandle( hFile );
		return	0;
	}
	printf( "ConsoleTestSyncRead-----\n\
				Read Buffer: %s\n\
				Read Length: %d\n" , Buffer , dwRet );

	return	0;
}