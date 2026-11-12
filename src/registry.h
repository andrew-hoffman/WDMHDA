#ifndef _REGISTRY_H_
#define _REGISTRY_H_

#include "portcls.h"

typedef struct _REG_BOOL_SETTING
	{
		PCWSTR   ValueName;     // Registry value name under \Settings
		BOOLEAN* Variable;      // Storage location to write into
		BOOLEAN  DefaultValue;  // Value used if registry entry is missing/invalid
	} REG_BOOL_SETTING;

	//global settings
	typedef struct _HDA_SETTINGS {
		BOOLEAN skipControllerReset;
		BOOLEAN skipCodecReset;
		BOOLEAN useAltOut;
		BOOLEAN useSPDIF;
		BOOLEAN useDisabledPins;
		BOOLEAN useDmaPos;
		BOOLEAN forcePioMode;
		BOOLEAN allowHighRate;
		BOOLEAN allowHighDepth;
		BOOLEAN allowBlindProbe;
		BOOLEAN isWin2kOrLater;
		//BOOLEAN hasSSE2;
	} HDA_SETTINGS, *PHDA_SETTINGS;

namespace Registry {

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
	HDA_SETTINGS ReadSettings(
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
	BOOLEAN  DetectOsVersion();
}

#endif