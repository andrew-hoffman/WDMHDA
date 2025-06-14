#include <wdm.h>

NTSTATUS
DriverEntry(
	PDRIVER_OBJECT ptDriverObject,
	PUNICODE_STRING pusRegistryPath
)
{
	if (!IoIsWdmVersionAvailable(1, 0x10))
	{
		DbgPrint("Hello from Windows 9x!\r\n");
	}
	else
	{
		DbgPrint("Hello from Windows NT!\r\n");
	}
	return STATUS_UNSUCCESSFUL;
}
