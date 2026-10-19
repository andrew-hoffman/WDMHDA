#ifndef _REGISTRY_H_
#define _REGISTRY_H_

#include "portcls.h"

typedef struct _REG_BOOL_SETTING
	{
		PCWSTR   ValueName;     // Registry value name under \Settings
		BOOLEAN* Variable;      // Storage location to write into
		BOOLEAN  DefaultValue;  // Value used if registry entry is missing/invalid
	} REG_BOOL_SETTING;

namespace Registry {
	
	//global settings
	extern BOOLEAN skipControllerReset;
	extern BOOLEAN skipCodecReset;
	extern BOOLEAN useAltOut;
	extern BOOLEAN useSPDIF;
	extern BOOLEAN useDisabledPins;
	extern BOOLEAN useDmaPos;
	extern BOOLEAN forcePioMode;
	extern BOOLEAN allowHighRate;
	extern BOOLEAN allowHighDepth;

    NTSTATUS OpenDeviceKey(
		OUT PREGISTRYKEY* RegistryKey, 
		IN PDEVICE_OBJECT m_pDeviceObject, 
		IN BOOLEAN DeviceKey, 
		IN ACCESS_MASK DesiredAccess
		);
    NTSTATUS OpenSubKey(
		IN PREGISTRYKEY ParentKey, 
		IN PCWSTR Name, 
		IN ACCESS_MASK DesiredAccess,
		OUT PREGISTRYKEY* RegistryKey,
		OUT PULONG Disposition OPTIONAL
		);
	VOID	 ReadSettings(
		IN PDEVICE_OBJECT DeviceObject
		);
	NTSTATUS ReadValue(
		IN PREGISTRYKEY RegistryKey,
		IN PCWSTR ValueName,
		IN ULONG ValueType,
		OUT PVOID Value,
		IN ULONG ValueLength,
		OUT PULONG ResultLength OPTIONAL
		);
    BOOLEAN  ReadBoolean(
		IN PDEVICE_OBJECT DeviceObject,
		IN PCWSTR ValueName,
		IN BOOLEAN DefaultValue
		);
    NTSTATUS ReadDword(
		IN PREGISTRYKEY RegistryKey, 
		IN PCWSTR ValueName,
		OUT PULONG Value
		);
    NTSTATUS WriteDword(
		IN PREGISTRYKEY RegistryKey,
		IN PCWSTR ValueName,
		IN ULONG Value
		);
}

#endif