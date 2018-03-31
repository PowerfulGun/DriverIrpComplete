#include	<ntddk.h>


typedef	struct _DeviceExtension
{
	LIST_ENTRY	IrpList;

	KTIMER	timer;
	LARGE_INTEGER	liDueTime;
	KDPC	dpc;
}DEV_EXT,*PDEV_EXT;


VOID	_UnloadDispatch( PDRIVER_OBJECT );
NTSTATUS	_DefaultDispatch( PDEVICE_OBJECT , PIRP );
NTSTATUS	_AsyncCreateCloseDispatch( PDEVICE_OBJECT , PIRP );
NTSTATUS	_AsyncReadDispatch( PDEVICE_OBJECT , PIRP );
VOID	_CustomDpc( PKDPC , PVOID , PVOID , PVOID );


NTSTATUS	DriverEntry(
	IN	PDRIVER_OBJECT	_pDriverObject ,
	IN	PUNICODE_STRING	_pRegistryPath
)
{
	UNICODE_STRING	DeviceName , Win32DeivceName;
	PDEVICE_OBJECT	pDeviceObject = NULL;
	NTSTATUS	status;
	PDEV_EXT		pDevExt = NULL;
	HANDLE		hThread;
	OBJECT_ATTRIBUTES	ObjectAttributes;
	CLIENT_ID	CID;

	//_asm	int 3
	RtlInitUnicodeString( &DeviceName , L"\\Device\\Async0" );
	RtlInitUnicodeString( &Win32DeivceName , L"\\DosDevices\\Async0" );

	for (ULONG i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
		_pDriverObject->MajorFunction[i] = _DefaultDispatch;

	_pDriverObject->MajorFunction[IRP_MJ_CREATE] = _AsyncCreateCloseDispatch;
	_pDriverObject->MajorFunction[IRP_MJ_CLOSE] = _AsyncCreateCloseDispatch;
	_pDriverObject->MajorFunction[IRP_MJ_READ] = _AsyncReadDispatch;
	_pDriverObject->DriverUnload = _UnloadDispatch;

	//����һ���Զ�����չ����СΪsizeof��DEVEXT��
	status = IoCreateDevice(
		_pDriverObject ,
		sizeof( DEV_EXT ) ,
		&DeviceName ,
		FILE_DEVICE_UNKNOWN ,
		0 ,
		FALSE ,
		&pDeviceObject );
	if (!NT_SUCCESS( status ))
		return	status;
	if (!pDeviceObject)
		return	STATUS_UNEXPECTED_IO_ERROR;

	pDeviceObject->Flags |= DO_DIRECT_IO;		//ֱ��i/o��MDL
	pDeviceObject->AlignmentRequirement = FILE_WORD_ALIGNMENT;	//���ݴ���ʱ��ַУ���С
	status = IoCreateSymbolicLink( &Win32DeivceName , &DeviceName );

	pDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	pDevExt = (PDEV_EXT)pDeviceObject->DeviceExtension;

	//��ʼ��IRP����
	InitializeListHead( &pDevExt->IrpList );
	//��ʼ����ʱ��
	KeInitializeTimer( &(pDevExt->timer) );
	//��ʼ��DPC
	KeInitializeDpc( &pDevExt->dpc , (PKDEFERRED_ROUTINE)_CustomDpc , pDevExt );	//pDevExt�Ǵ���_CustomDpc�����Ĳ���

	//���ö�ʱʱ��λ3s
	pDevExt->liDueTime = RtlConvertLongToLargeInteger( -30000000 );
	//������ʱ��
	KeSetTimer( &pDevExt->timer , pDevExt->liDueTime , &pDevExt->dpc );

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

NTSTATUS	_AsyncCreateCloseDispatch(
	IN	PDEVICE_OBJECT	_pDevcieObject ,
	IN	PIRP			_pIrp
)
{
	_pIrp->IoStatus.Status = STATUS_SUCCESS;
	_pIrp->IoStatus.Information = 0;
	IoCompleteRequest( _pIrp , IO_NO_INCREMENT );
	return	_pIrp->IoStatus.Status;
}

NTSTATUS	_AsyncReadDispatch(
	IN	PDEVICE_OBJECT	_pDeviceObject ,
	IN	PIRP			_pIrp
)
{
	NTSTATUS	status;
	PIO_STACK_LOCATION	pIrpStack = IoGetCurrentIrpStackLocation( _pIrp );
	PDEV_EXT	pDevExt = (PDEV_EXT)_pDeviceObject->DeviceExtension;

	//����IRPλPendingδ���������Ժ���ɸ�IRP
	IoMarkIrpPending( _pIrp );

	//��IRP�����Զ���������,�������ListEntry
	InsertTailList( &pDevExt->IrpList , &_pIrp->Tail.Overlay.ListEntry );

	//����pending�����巵�ظ�I/O��������ֵ�����IRP��Pending��־λһ��
	//������iomarkirppending�ͷ���ֵҪһ�� 
	return	STATUS_PENDING;
}

VOID	_UnloadDispatch(
	IN	PDRIVER_OBJECT	_pDriverObject
)
{
	UNICODE_STRING	Win32DeviceName;
	PDEV_EXT	pDevExt = (PDEV_EXT)_pDriverObject->DeviceObject->DeviceExtension;

	RtlInitUnicodeString( &Win32DeviceName , L"\\DosDevices\\Async0" );
	//ɾ����ʱ��
	KeCancelTimer( &pDevExt->timer );
	//ɾ���������豸
	IoDeleteDevice( _pDriverObject->DeviceObject );
}

VOID	_CustomDpc(
	IN	PKDPC	Dpc ,
	IN	PVOID	DeferredContext ,
	IN	PVOID	SystemArgument1 ,
	IN	PVOID	SystemArgument2
)
{
	PIRP	pIrp;
	PDEV_EXT	pDevExt = (PDEV_EXT)DeferredContext;
	PVOID	pBuffer = NULL;
	ULONG	uBufferLen = 0;
	PIO_STACK_LOCATION	pIrpStack = NULL;

	KdPrint( ("[CustomDpc]\n") );

	do
	{
		if (!pDevExt)
		{
			KdPrint( ("!pDevExt\n") );
			break;
		}

		//���δ��IRP�����Ƿ�Ϊ��
		if (IsListEmpty( &pDevExt->IrpList ))
		{
			KdPrint( ("IsListEmpty\n") );
			break;
		}
		//��IRP������ȡ��һ��IRP����ɸ�IRP��ȡ������ListEntry�ĵ�ַ
		KdPrint( ("[CustomDpc] Dequeue one irp from IrpList and complete it!\n") );

		PLIST_ENTRY	pListEntry = (PLIST_ENTRY)RemoveHeadList( &pDevExt->IrpList );
		if (!pListEntry)
			break;
		pIrp = (PIRP)CONTAINING_RECORD( pListEntry , IRP , Tail.Overlay.ListEntry );
		pIrpStack = IoGetCurrentIrpStackLocation( pIrp );

		KdPrint( ("CustomDpc Irp: 0x%x\n" , pIrp) );

		//��������Ķ�д��ʽλֱ��I/O
		pBuffer = MmGetSystemAddressForMdl( pIrp->MdlAddress );
		if (pBuffer == NULL)
		{
			pIrp->IoStatus.Status = STATUS_UNSUCCESSFUL;
			pIrp->IoStatus.Information = 0;
			IoCompleteRequest( pIrp , IO_NO_INCREMENT );

			break;
		}
		uBufferLen = pIrpStack->Parameters.Read.Length;

		KdPrint( ("CustomDpc Read uBufferLen: %d\n" , uBufferLen) );

		//֧��5�ֽ�һ�µĶ�����
		uBufferLen = uBufferLen > 5 ? 5 : uBufferLen;

		//������������
		RtlCopyMemory( pBuffer , "hello" , uBufferLen );

		pIrp->IoStatus.Status = STATUS_SUCCESS;
		pIrp->IoStatus.Information = uBufferLen;

		//��ɸ�IRP
		IoCompleteRequest( pIrp , IO_NO_INCREMENT );

		KdPrint( ("[CustomDpc] Finish complete!\n") );
	} while (FALSE);

		KdPrint( ("Set Next Timer.\n") );

	//�������ö�ʱ��
	KeSetTimer( &pDevExt->timer , pDevExt->liDueTime , &pDevExt->dpc );
}