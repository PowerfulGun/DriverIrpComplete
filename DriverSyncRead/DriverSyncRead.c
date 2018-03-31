#include	<ntddk.h>


VOID	_UnloadDispatch( PDRIVER_OBJECT );
NTSTATUS	_DefaultDispatch( PDEVICE_OBJECT , PIRP );
NTSTATUS	_SyncCreateCloseDispatch( PDEVICE_OBJECT , PIRP );
NTSTATUS	_SyncReadDispatch( PDEVICE_OBJECT , PIRP );


NTSTATUS	DriverEntry(
	IN	PDRIVER_OBJECT	_pDriverObject ,
	IN	PUNICODE_STRING	_pRegistryPath
)
{
	UNICODE_STRING	DeviceName , Win32DeivceName;
	PDEVICE_OBJECT	pDeviceObject = NULL;
	NTSTATUS	status;
	HANDLE		hThread;
	OBJECT_ATTRIBUTES	ObjectAttributes;


	RtlInitUnicodeString( &DeviceName , L"\\Device\\Sync0" );
	RtlInitUnicodeString( &Win32DeivceName , L"\\DosDevices\\Sync0" );

	for (ULONG i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
		_pDriverObject->MajorFunction[i] = _DefaultDispatch;

	_pDriverObject->MajorFunction[IRP_MJ_CREATE] = _SyncCreateCloseDispatch;
	_pDriverObject->MajorFunction[IRP_MJ_CLOSE] = _SyncCreateCloseDispatch;
	_pDriverObject->MajorFunction[IRP_MJ_READ] = _SyncReadDispatch;
	_pDriverObject->DriverUnload = _UnloadDispatch;

	//分配一个自定义扩展，大小为sizeof（DEVEXT）
	status = IoCreateDevice(
		_pDriverObject ,
		0,
		&DeviceName ,
		FILE_DEVICE_UNKNOWN ,
		0 ,
		FALSE ,
		&pDeviceObject );
	if (!NT_SUCCESS( status ))
		return	status;
	if (!pDeviceObject)
		return	STATUS_UNEXPECTED_IO_ERROR;

	pDeviceObject->Flags |= DO_DIRECT_IO;		//直接i/o，MDL
	pDeviceObject->AlignmentRequirement = FILE_WORD_ALIGNMENT;	//数据传输时地址校验大小
	status = IoCreateSymbolicLink( &Win32DeivceName , &DeviceName );

	pDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	return	STATUS_SUCCESS;
}

NTSTATUS	_DefaultDispatch(
	IN	PDEVICE_OBJECT	_pDeviceObject ,
	IN	PIRP			_pIrp
)
{
	_pIrp->IoStatus.Status = STATUS_NOT_SUPPORTED;
	_pIrp->IoStatus.Information = 0;
	IoCompleteRequest( _pIrp , IO_NO_INCREMENT );
	return	_pIrp->IoStatus.Status;
}

NTSTATUS	_SyncCreateCloseDispatch(
	IN	PDEVICE_OBJECT	_pDevcieObject ,
	IN	PIRP			_pIrp
)
{
	_pIrp->IoStatus.Status = STATUS_SUCCESS;
	_pIrp->IoStatus.Information = 0;
	IoCompleteRequest( _pIrp , IO_NO_INCREMENT );
	return	_pIrp->IoStatus.Status;
}

NTSTATUS	_SyncReadDispatch(
	IN	PDEVICE_OBJECT	_pDeviceObject ,
	IN	PIRP			_pIrp
)
{
	NTSTATUS	status = STATUS_SUCCESS;
	PIO_STACK_LOCATION	pIrpStack = IoGetCurrentIrpStackLocation( _pIrp );
	PVOID	pBuffer = NULL;
	ULONG	uBufferLen = 0;

	KdPrint( ("[_SyncReadDispatch]") );

	do
	{
		//读写请求使用的是直接I/O方式
		pBuffer = MmGetSystemAddressForMdl( _pIrp->MdlAddress );
		if (pBuffer == NULL)
		{
			status = STATUS_UNSUCCESSFUL;
			break;
		}
		uBufferLen = pIrpStack->Parameters.Read.Length;

		KdPrint( ("Read Len: %d\n" , uBufferLen) );

		//最大支持5字节读请求
		uBufferLen = uBufferLen >= 5 ? 5 : uBufferLen;

		//简单地向读请求缓冲区写入
		RtlCopyMemory( pBuffer , "hello" , uBufferLen );
	} while (FALSE);

	//填写返回状态及返回大小
	_pIrp->IoStatus.Status = status;
	_pIrp->IoStatus.Information = uBufferLen;

	//完成IRP
	IoCompleteRequest( _pIrp , IO_NO_INCREMENT );

	return	status;
}

VOID	_UnloadDispatch(
	IN	PDRIVER_OBJECT	_pDriverObject
)
{
	UNICODE_STRING	Win32DeviceName;

	RtlInitUnicodeString( &Win32DeviceName , L"\\DosDevices\\Async0" );
	//删除创建的设备
	IoDeleteDevice( _pDriverObject->DeviceObject );
}
