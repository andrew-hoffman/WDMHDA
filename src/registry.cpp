/*****************************************************************************
 * registry.cpp - Registry reading & writing.
 *****************************************************************************
 * Copyright (c) 2025 Drew Hoffman
 * Released under MIT License
 * Code from Microsoft's driver samples used under MIT license. 
 */

#include "registry.h"
#include <wchar.h>

#define ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))

namespace Registry {
	//driver global settings read via registry keys
	BOOLEAN skipControllerReset;
	BOOLEAN skipCodecReset;
	BOOLEAN useAltOut;
	BOOLEAN useSPDIF;
	BOOLEAN useDisabledPins;
	BOOLEAN useDmaPos;
	BOOLEAN forcePioMode;
	BOOLEAN allowHighRate;
	BOOLEAN allowHighDepth;

	static REG_BOOL_SETTING g_BooleanSettings[] =
	{
		{ L"SkipControllerReset", &skipControllerReset, FALSE },
		{ L"SkipCodecReset",      &skipCodecReset,      FALSE },
		{ L"UseAltOut",           &useAltOut,           FALSE },
		{ L"UseSPDIF",            &useSPDIF,            TRUE  },
		{ L"UseDisabledPins",     &useDisabledPins,     FALSE },
		{ L"UseDmaPos",           &useDmaPos,           FALSE },
		{ L"ForcePioMode",        &forcePioMode,        FALSE },
		{ L"AllowHighRate",       &allowHighRate,       FALSE },
		{ L"AllowHighDepth",      &allowHighDepth,      FALSE }
	};
}

//#pragma code_seg("PAGE")

/*****************************************************************************
 * Registry helpers
 *
 * Keep PortCls key creation, subkey creation, and DWORD access in one place
 * so future registry users share the same lifetime and error handling.
 *****************************************************************************/
NTSTATUS
Registry::OpenDeviceKey(
	OUT PREGISTRYKEY* RegistryKey,
	IN PDEVICE_OBJECT m_pDeviceObject, 
	IN BOOLEAN DeviceKey, 
	//true= device key false= driver key
	IN ACCESS_MASK DesiredAccess
)
{
	if (RegistryKey == NULL) {
		return STATUS_INVALID_PARAMETER;
	}

	*RegistryKey = NULL;
	return PcNewRegistryKey(RegistryKey,
		NULL,
		DeviceKey ? DeviceRegistryKey : DriverRegistryKey,
		DesiredAccess,
		m_pDeviceObject,
		NULL,
		NULL,
		0,
		NULL);
}

NTSTATUS
Registry::OpenSubKey(
	IN PREGISTRYKEY ParentKey,
	IN PCWSTR Name,
	IN ACCESS_MASK DesiredAccess,
	OUT PREGISTRYKEY* RegistryKey,
	OUT PULONG Disposition OPTIONAL
)
{
	UNICODE_STRING KeyName;
	ULONG LocalDisposition;

	if (ParentKey == NULL || Name == NULL || RegistryKey == NULL) {
		return STATUS_INVALID_PARAMETER;
	}

	*RegistryKey = NULL;
	RtlInitUnicodeString(&KeyName, Name);
	return ParentKey->NewSubKey(RegistryKey,
		NULL,
		DesiredAccess,
		&KeyName,
		REG_OPTION_NON_VOLATILE,
		Disposition ? Disposition : &LocalDisposition);
}

VOID Registry::ReadSettings (IN PDEVICE_OBJECT DeviceObject){
	for (ULONG i = 0; i < ARRAY_COUNT(g_BooleanSettings); ++i){
		*g_BooleanSettings[i].Variable =
            ReadBoolean(DeviceObject, g_BooleanSettings[i].ValueName, 
			g_BooleanSettings[i].DefaultValue);
    }
}

NTSTATUS
Registry::ReadValue(
	IN PREGISTRYKEY RegistryKey,
	IN PCWSTR ValueName,
	IN ULONG ValueType,
	OUT PVOID Value,
	IN ULONG ValueLength,
	OUT PULONG ResultLength OPTIONAL
)
{
	UNICODE_STRING Name;
	typedef struct _REGISTRY_VALUE_BUFFER {
		KEY_VALUE_PARTIAL_INFORMATION Information;
		ULONG Data;
	} REGISTRY_VALUE_BUFFER;
	REGISTRY_VALUE_BUFFER Buffer;
	ULONG Length;
	NTSTATUS Status;

	if (RegistryKey == NULL || ValueName == NULL || Value == NULL || ValueLength == 0) {
		return STATUS_INVALID_PARAMETER;
	}

	RtlInitUnicodeString(&Name, ValueName);
	Status = RegistryKey->QueryValueKey(&Name,
		KeyValuePartialInformation,
		&Buffer.Information,
		sizeof(Buffer),
		&Length);
	if (!NT_SUCCESS(Status)) {
		return Status;
	}
	if (Buffer.Information.Type != ValueType) {
		return STATUS_OBJECT_TYPE_MISMATCH;
	}
	if (Buffer.Information.DataLength > ValueLength) {
		return STATUS_BUFFER_TOO_SMALL;
	}

	RtlCopyMemory(Value, Buffer.Information.Data, Buffer.Information.DataLength);
	if (ResultLength) *ResultLength = Buffer.Information.DataLength;
	return STATUS_SUCCESS;
}

// Reads any Boolean value from the Registry Settings key.
// If nonexistent, returns DefaultValue
BOOLEAN
Registry::ReadBoolean(
	IN PDEVICE_OBJECT DeviceObject,
    IN  PCWSTR   ValueName,
    IN  BOOLEAN  DefaultValue
	)
{
    PAGED_CODE();

    PREGISTRYKEY   DriverKey   = NULL;
    PREGISTRYKEY   SettingsKey = NULL;
    NTSTATUS       Status;
    BOOLEAN        Result = DefaultValue;
	ULONG Value;

	Status = OpenDeviceKey(&DriverKey, DeviceObject, FALSE, KEY_READ);
	if (!NT_SUCCESS(Status)) goto Exit;

	Status = OpenSubKey(DriverKey, L"Settings", KEY_READ, &SettingsKey, NULL);
	if (!NT_SUCCESS(Status)) goto Exit;

	Status = ReadDword(SettingsKey, ValueName, &Value);
	if (NT_SUCCESS(Status)) Result = Value ? TRUE : FALSE;

Exit:
    if (SettingsKey) SettingsKey->Release();
    if (DriverKey)   DriverKey->Release();

    return Result;
}

NTSTATUS
Registry::ReadDword(
	IN PREGISTRYKEY RegistryKey,
	IN PCWSTR ValueName,
	OUT PULONG Value
)
{
	ULONG ResultLength;
	NTSTATUS Status;

	Status = ReadValue(RegistryKey, ValueName, REG_DWORD, Value,
		sizeof(*Value), &ResultLength);
	return (NT_SUCCESS(Status) && ResultLength != sizeof(*Value)) ?
		STATUS_OBJECT_TYPE_MISMATCH : Status;
}

NTSTATUS
Registry::WriteDword(
	IN PREGISTRYKEY RegistryKey,
	IN PCWSTR ValueName,
	IN ULONG Value
)
{
	UNICODE_STRING Name;

	if (RegistryKey == NULL || ValueName == NULL) {
		return STATUS_INVALID_PARAMETER;
	}

	RtlInitUnicodeString(&Name, ValueName);
	return RegistryKey->SetValueKey(&Name, REG_DWORD, &Value, sizeof(Value));
}
