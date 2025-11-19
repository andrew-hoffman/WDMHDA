/*****************************************************************************
 * mintopo.cpp - HDA topology miniport implementation
 *****************************************************************************
 * Copyright (c) 1997-1999 Microsoft Corporation. All Rights Reserved.
 */

#include "limits.h"
#include "mintopo.h"

#define STR_MODULENAME "HDAtopo: "

#define CHAN_LEFT       0
#define CHAN_RIGHT      1
#define CHAN_MASTER     (-1)


#pragma code_seg("PAGE")


/*****************************************************************************
 * CreateMiniportTopologyHDA()
 *****************************************************************************
 * Creates a topology miniport object for the HDA adapter.  This uses a
 * macro from STDUNK.H to do all the work.
 */
NTSTATUS
CreateMiniportTopologyHDA
(
    OUT     PUNKNOWN *  Unknown,
    IN      REFCLSID,
    IN      PUNKNOWN    UnknownOuter    OPTIONAL,
    IN      POOL_TYPE   PoolType
)
{
    PAGED_CODE();

    ASSERT(Unknown);

    STD_CREATE_BODY(CMiniportTopologyHDA,Unknown,UnknownOuter,PoolType);
}

/*****************************************************************************
 * CMiniportTopologyHDA::ProcessResources()
 *****************************************************************************
 * Processes the resource list.
 */
NTSTATUS
CMiniportTopologyHDA::
ProcessResources
(
    IN  PRESOURCELIST   ResourceList
)
{
    PAGED_CODE();

    ASSERT(ResourceList);

    _DbgPrintF(DEBUGLVL_VERBOSE,("[CMiniportTopologyHDA::ProcessResources]"));

    //
    // Get counts for the types of resources.
    //
    ULONG       countIO     = ResourceList->NumberOfPorts();
    ULONG       countIRQ    = ResourceList->NumberOfInterrupts();
    ULONG       countDMA    = ResourceList->NumberOfDmas();

    NTSTATUS    ntStatus    = STATUS_SUCCESS;


	/* HACK - let me stay running long enough to make a noise please!!
    //
    // Make sure we have the expected number of resources.
    //
    if  (   (countIO != 1)
        ||  (countIRQ != 0)
        ||  (countDMA != 0)
        )
    {
        _DbgPrintF(DEBUGLVL_TERSE,("unknown configuraton; check your code!"));
        ntStatus = STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    if (NT_SUCCESS(ntStatus))
    {
        //
        // Get the port address.
        //
        PortBase =
            PUCHAR(ResourceList->FindTranslatedPort(0)->u.Port.Start.LowPart);
    }
	*/
    return ntStatus;
}

/*****************************************************************************
 * CMiniportTopologyHDA::NonDelegatingQueryInterface()
 *****************************************************************************
 * Obtains an interface.  This function works just like a COM QueryInterface
 * call and is used if the object is not being aggregated.
 */
STDMETHODIMP
CMiniportTopologyHDA::
NonDelegatingQueryInterface
(
    IN      REFIID  Interface,
    OUT     PVOID * Object
)
{
    PAGED_CODE();

    ASSERT(Object);

    _DbgPrintF(DEBUGLVL_VERBOSE,("[CMiniportTopologyHDA::NonDelegatingQueryInterface]"));

    if (IsEqualGUIDAligned(Interface,IID_IUnknown))
    {
        *Object = PVOID(PUNKNOWN(this));
    }
    else
    if (IsEqualGUIDAligned(Interface,IID_IMiniport))
    {
        *Object = PVOID(PMINIPORT(this));
    }
    else
    if (IsEqualGUIDAligned(Interface,IID_IMiniportTopology))
    {
        *Object = PVOID(PMINIPORTTOPOLOGY(this));
    }
    else
    {
        *Object = NULL;
    }

    if (*Object)
    {
        //
        // We reference the interface for the caller.
        //
        PUNKNOWN(*Object)->AddRef();
        return STATUS_SUCCESS;
    }

    return STATUS_INVALID_PARAMETER;
}

/*****************************************************************************
 * CMiniportTopologyHDA::~CMiniportTopologyHDA()
 *****************************************************************************
 * Destructor.
 */
CMiniportTopologyHDA::
~CMiniportTopologyHDA
(   void
)
{
    PAGED_CODE();

    _DbgPrintF(DEBUGLVL_VERBOSE,("[CMiniportTopologyHDA::~CMiniportTopologyHDA]"));

    if (Port)
    {
        Port->Release();
    }
    if (AdapterCommon)
    {
        AdapterCommon->SaveMixerSettingsToRegistry();
        AdapterCommon->Release();
    }
}

/*****************************************************************************
 * CMiniportTopologyHDA::Init()
 *****************************************************************************
 * Initializes a the miniport.
 */
STDMETHODIMP
CMiniportTopologyHDA::
Init
(
    IN      PUNKNOWN        UnknownAdapter,
    IN      PRESOURCELIST   ResourceList,
    IN      PPORTTOPOLOGY   Port_
)
{
    PAGED_CODE();

    ASSERT(UnknownAdapter);
    ASSERT(ResourceList);
    ASSERT(Port_);

    _DbgPrintF(DEBUGLVL_VERBOSE,("[CMiniportTopologyHDA::Init]"));

    //
    // AddRef() is required because we are keeping this pointer.
    //
    Port = Port_;
    Port->AddRef();

    NTSTATUS ntStatus =
        UnknownAdapter->QueryInterface
        (
            IID_IAdapterCommon,
            (PVOID *) &AdapterCommon
        );

    if (NT_SUCCESS(ntStatus))
    {
        ntStatus = ProcessResources(ResourceList);

        if (NT_SUCCESS(ntStatus))
        {
         //   AdapterCommon->MixerReset();
        }
    }

    if( !NT_SUCCESS(ntStatus) )
    {
        //
        // clean up our mess
        //

        // clean up AdapterCommon
        if( AdapterCommon )
        {
            AdapterCommon->Release();
            AdapterCommon = NULL;
        }

        // release the port
        Port->Release();
        Port = NULL;
    }

    return ntStatus;
}

/*****************************************************************************
 * CMiniportTopologyHDA::GetDescription()
 *****************************************************************************
 * Gets the topology.
 */
STDMETHODIMP
CMiniportTopologyHDA::
GetDescription
(
    OUT     PPCFILTER_DESCRIPTOR *  OutFilterDescriptor
)
{
    PAGED_CODE();

    ASSERT(OutFilterDescriptor);

    _DbgPrintF(DEBUGLVL_VERBOSE,("[CMiniportTopologyHDA::GetDescription]"));

    *OutFilterDescriptor = &MiniportFilterDescriptor;

    return STATUS_SUCCESS;
}

/*****************************************************************************
 * PropertyHandler_OnOff()
 *****************************************************************************
 * Accesses a KSAUDIO_ONOFF value property.
 */
static
NTSTATUS
PropertyHandler_OnOff
(
    IN      PPCPROPERTY_REQUEST   PropertyRequest
)
{
    PAGED_CODE();

    ASSERT(PropertyRequest);

    _DbgPrintF(DEBUGLVL_VERBOSE,("[CMiniportTopologyHDA::PropertyHandler_OnOff]"));

    CMiniportTopologyHDA *that =
        (CMiniportTopologyHDA *) PropertyRequest->MajorTarget;

    NTSTATUS        ntStatus = STATUS_INVALID_PARAMETER;
    BYTE            data;
    LONG            channel;

    // validate node
    if (PropertyRequest->Node != ULONG(-1))
    {
        if(PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
        {
            // get the instance channel parameter
            if(PropertyRequest->InstanceSize >= sizeof(LONG))
            {
                channel = *(PLONG(PropertyRequest->Instance));

                // validate and get the output parameter
                if (PropertyRequest->ValueSize >= sizeof(BOOL))
                {
                    PBOOL OnOff = PBOOL(PropertyRequest->Value);
    
                    // switch on node id
                    switch(PropertyRequest->Node)
                    {
                        case MIC_AGC:   // Microphone AGC Control (mono)
                            // check if AGC property request on mono/left channel
                            if( ( PropertyRequest->PropertyItem->Id == KSPROPERTY_AUDIO_AGC ) &&
                                ( channel == CHAN_LEFT ) )
                            {
                                data = that->ReadBitsFromMixer( DSP_MIX_AGCIDX,
                                                          1,
                                                          MIXBIT_MIC_AGC );
                                *OnOff = data ? FALSE : TRUE;
                                PropertyRequest->ValueSize = sizeof(BOOL);
                                ntStatus = STATUS_SUCCESS;
                            }
                            break;
    
                        case MIC_LINEOUT_MUTE:  // Microphone Lineout Mute Control (mono)
                            // check if MUTE property request on mono/left channel
                            if( ( PropertyRequest->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE ) &&
                                ( channel == CHAN_LEFT ) )
                            {
                                data = that->ReadBitsFromMixer( DSP_MIX_OUTMIXIDX,
                                                          1,
                                                          MIXBIT_MIC_LINEOUT );
                                *OnOff = data ? FALSE : TRUE;
                                PropertyRequest->ValueSize = sizeof(BOOL);
                                ntStatus = STATUS_SUCCESS;
                            }
                            break;
                    }
                }
            }

        } else if(PropertyRequest->Verb & KSPROPERTY_TYPE_SET)
        {
            // get the instance channel parameter
            if(PropertyRequest->InstanceSize >= sizeof(LONG))
            {
                channel = *(PLONG(PropertyRequest->Instance));
                
                // validate and get the input parameter
                if (PropertyRequest->ValueSize == sizeof(BOOL))
                {
                    BYTE value = *(PBOOL(PropertyRequest->Value)) ? 0 : 1;
    
                    // switch on the node id
                    switch(PropertyRequest->Node)
                    {
                        case MIC_AGC:   // Microphone AGC Control (mono)
                            // check if AGC property request on mono/left channel
                            if( ( PropertyRequest->PropertyItem->Id == KSPROPERTY_AUDIO_AGC ) &&
                                ( channel == CHAN_LEFT ) )
                            {
                                that->WriteBitsToMixer( DSP_MIX_AGCIDX,
                                                  1,
                                                  MIXBIT_MIC_AGC,
                                                  value );
                                ntStatus = STATUS_SUCCESS;
                            }
                            break;
    
                        case MIC_LINEOUT_MUTE:  // Microphone Lineout Mute Control (mono)
                            // check if MUTE property request on mono/left channel
                            if( ( PropertyRequest->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE ) &&
                                ( channel == CHAN_LEFT ) )
                            {
                                that->WriteBitsToMixer( DSP_MIX_OUTMIXIDX,
                                                  1,
                                                  MIXBIT_MIC_LINEOUT,
                                                  value );
                                ntStatus = STATUS_SUCCESS;
                            }
                            break;
                    }
                }
            }
        } else if(PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
        {
            if ( ( (PropertyRequest->Node == MIC_AGC) && (PropertyRequest->PropertyItem->Id == KSPROPERTY_AUDIO_AGC) ) ||
                 ( (PropertyRequest->Node == MIC_LINEOUT_MUTE) && (PropertyRequest->PropertyItem->Id == KSPROPERTY_AUDIO_MUTE) ) )
            {
                if(PropertyRequest->ValueSize >= (sizeof(KSPROPERTY_DESCRIPTION)))
                {
                    // if return buffer can hold a KSPROPERTY_DESCRIPTION, return it
                    PKSPROPERTY_DESCRIPTION PropDesc = PKSPROPERTY_DESCRIPTION(PropertyRequest->Value);

                    PropDesc->AccessFlags       = KSPROPERTY_TYPE_BASICSUPPORT |
                                                  KSPROPERTY_TYPE_GET |
                                                  KSPROPERTY_TYPE_SET;
                    PropDesc->DescriptionSize   = sizeof(KSPROPERTY_DESCRIPTION);
                    PropDesc->PropTypeSet.Set   = KSPROPTYPESETID_General;
                    PropDesc->PropTypeSet.Id    = VT_BOOL;
                    PropDesc->PropTypeSet.Flags = 0;
                    PropDesc->MembersListCount  = 0;
                    PropDesc->Reserved          = 0;

                    // set the return value size
                    PropertyRequest->ValueSize = sizeof(KSPROPERTY_DESCRIPTION);
                    ntStatus = STATUS_SUCCESS;
                } else if(PropertyRequest->ValueSize >= sizeof(ULONG))
                {
                    // if return buffer can hold a ULONG, return the access flags
                    PULONG AccessFlags = PULONG(PropertyRequest->Value);
            
                    *AccessFlags = KSPROPERTY_TYPE_BASICSUPPORT |
                                   KSPROPERTY_TYPE_GET |
                                   KSPROPERTY_TYPE_SET;
            
                    // set the return value size
                    PropertyRequest->ValueSize = sizeof(ULONG);
                    ntStatus = STATUS_SUCCESS;                    
                }
            }
        }
    }

    return ntStatus;
}

/*****************************************************************************
 * BasicSupportHandler()
 *****************************************************************************
 * Assists in BASICSUPPORT accesses on level properties
 */
static
NTSTATUS
BasicSupportHandler
(
    IN      PPCPROPERTY_REQUEST   PropertyRequest
)
{
    ASSERT(PropertyRequest);

    _DbgPrintF(DEBUGLVL_VERBOSE,("[BasicSupportHandler]"));

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    if(PropertyRequest->ValueSize >= (sizeof(KSPROPERTY_DESCRIPTION)))
    {
        // if return buffer can hold a KSPROPERTY_DESCRIPTION, return it
        PKSPROPERTY_DESCRIPTION PropDesc = PKSPROPERTY_DESCRIPTION(PropertyRequest->Value);

        PropDesc->AccessFlags = KSPROPERTY_TYPE_BASICSUPPORT |
                                KSPROPERTY_TYPE_GET |
                                KSPROPERTY_TYPE_SET;
        PropDesc->DescriptionSize   = sizeof(KSPROPERTY_DESCRIPTION) +
                                      sizeof(KSPROPERTY_MEMBERSHEADER) +
                                      sizeof(KSPROPERTY_STEPPING_LONG);
        PropDesc->PropTypeSet.Set   = KSPROPTYPESETID_General;
        PropDesc->PropTypeSet.Id    = VT_I4;
        PropDesc->PropTypeSet.Flags = 0;
        PropDesc->MembersListCount  = 1;
        PropDesc->Reserved          = 0;

        // if return buffer cn also hold a range description, return it too
        if(PropertyRequest->ValueSize >= (sizeof(KSPROPERTY_DESCRIPTION) +
                                      sizeof(KSPROPERTY_MEMBERSHEADER) +
                                      sizeof(KSPROPERTY_STEPPING_LONG)))
        {
            // fill in the members header
            PKSPROPERTY_MEMBERSHEADER Members = PKSPROPERTY_MEMBERSHEADER(PropDesc + 1);

            Members->MembersFlags   = KSPROPERTY_MEMBER_STEPPEDRANGES;
            Members->MembersSize    = sizeof(KSPROPERTY_STEPPING_LONG);
            Members->MembersCount   = 1;
            Members->Flags          = 0;

            // fill in the stepped range
            PKSPROPERTY_STEPPING_LONG Range = PKSPROPERTY_STEPPING_LONG(Members + 1);

            switch(PropertyRequest->Node)
            {
                case WAVEOUT_VOLUME:
                case SYNTH_VOLUME:
                case CD_VOLUME:
                case LINEIN_VOLUME:
                case MIC_VOLUME:
                case LINEOUT_VOL:
                    Range->Bounds.SignedMaximum = 0;            // 0   (dB) * 0x10000
                    Range->Bounds.SignedMinimum = 0xFFC20000;   // -62 (dB) * 0x10000
                    Range->SteppingDelta        = 0x20000;      // 2   (dB) * 0x10000
                    break;

                case LINEOUT_GAIN:
                case WAVEIN_GAIN:
                    Range->Bounds.SignedMaximum = 0x120000;     // 18  (dB) * 0x10000
                    Range->Bounds.SignedMinimum = 0;            // 0   (dB) * 0x10000
                    Range->SteppingDelta        = 0x60000;      // 6   (dB) * 0x10000
                    break;

                case LINEOUT_BASS:
                case LINEOUT_TREBLE:
                    Range->Bounds.SignedMaximum = 0xE0000;      // 14  (dB) * 0x10000
                    Range->Bounds.SignedMinimum = 0xFFF20000;   // -14 (dB) * 0x10000
                    Range->SteppingDelta        = 0x20000;      // 2   (dB) * 0x10000
                    break;

            }
            Range->Reserved         = 0;

            _DbgPrintF(DEBUGLVL_BLAB, ("---Node: %d  Max: 0x%X  Min: 0x%X  Step: 0x%X",PropertyRequest->Node,
                                                                                       Range->Bounds.SignedMaximum,
                                                                                       Range->Bounds.SignedMinimum,
                                                                                       Range->SteppingDelta));

            // set the return value size
            PropertyRequest->ValueSize = sizeof(KSPROPERTY_DESCRIPTION) +
                                         sizeof(KSPROPERTY_MEMBERSHEADER) +
                                         sizeof(KSPROPERTY_STEPPING_LONG);
        } else
        {
            // set the return value size
            PropertyRequest->ValueSize = sizeof(KSPROPERTY_DESCRIPTION);
        }
        ntStatus = STATUS_SUCCESS;

    } else if(PropertyRequest->ValueSize >= sizeof(ULONG))
    {
        // if return buffer can hold a ULONG, return the access flags
        PULONG AccessFlags = PULONG(PropertyRequest->Value);

        *AccessFlags = KSPROPERTY_TYPE_BASICSUPPORT |
                       KSPROPERTY_TYPE_GET |
                       KSPROPERTY_TYPE_SET;

        // set the return value size
        PropertyRequest->ValueSize = sizeof(ULONG);
        ntStatus = STATUS_SUCCESS;

    }

    return ntStatus;
}


/*****************************************************************************
 * PropertyHandler_SuperMixCaps()
 *****************************************************************************
 * Handles supermixer caps accesses
 */
static
NTSTATUS
PropertyHandler_SuperMixCaps
(
    IN      PPCPROPERTY_REQUEST   PropertyRequest
)
{
    PAGED_CODE();

    ASSERT(PropertyRequest);

    _DbgPrintF(DEBUGLVL_VERBOSE,("[CMiniportTopologyHDA::PropertyHandler_SuperMixCaps]"));

    CMiniportTopologyHDA *that =
        (CMiniportTopologyHDA *) PropertyRequest->MajorTarget;

    NTSTATUS        ntStatus = STATUS_INVALID_PARAMETER;
    ULONG           count;

    // validate node
    if(PropertyRequest->Node != ULONG(-1))
    {
        if(PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
        {
            switch(PropertyRequest->Node)
            {
                // Full 2x2 Switches
                case SYNTH_WAVEIN_SUPERMIX:
                case CD_WAVEIN_SUPERMIX:
                case LINEIN_WAVEIN_SUPERMIX:
                    if(!PropertyRequest->ValueSize)
                    {
                        PropertyRequest->ValueSize = 2 * sizeof(ULONG) + 4 * sizeof(KSAUDIO_MIX_CAPS);
                        ntStatus = STATUS_BUFFER_OVERFLOW;
                    } else if(PropertyRequest->ValueSize == 2 * sizeof(ULONG))
                    {
                        PKSAUDIO_MIXCAP_TABLE MixCaps = (PKSAUDIO_MIXCAP_TABLE)PropertyRequest->Value;
                        MixCaps->InputChannels = 2;
                        MixCaps->OutputChannels = 2;
                        ntStatus = STATUS_SUCCESS;
                    } else if(PropertyRequest->ValueSize >= 2 * sizeof(ULONG) + 4 * sizeof(KSAUDIO_MIX_CAPS))
                    {
                        PropertyRequest->ValueSize = 2 * sizeof(ULONG) + 4 * sizeof(KSAUDIO_MIX_CAPS);

                        PKSAUDIO_MIXCAP_TABLE MixCaps = (PKSAUDIO_MIXCAP_TABLE)PropertyRequest->Value;
                        MixCaps->InputChannels = 2;
                        MixCaps->OutputChannels = 2;
                        for(count = 0; count < 4; count++)
                        {
                            MixCaps->Capabilities[count].Mute = TRUE;
                            MixCaps->Capabilities[count].Minimum = 0;
                            MixCaps->Capabilities[count].Maximum = 0;
                            MixCaps->Capabilities[count].Reset = 0;
                        }
                        ntStatus = STATUS_SUCCESS;
                    }
                    break;

                // Limited 2x2 Switches
                case CD_LINEOUT_SUPERMIX:
                case LINEIN_LINEOUT_SUPERMIX:
                    if(!PropertyRequest->ValueSize)
                    {
                        PropertyRequest->ValueSize = 2 * sizeof(ULONG) + 4 * sizeof(KSAUDIO_MIX_CAPS);
                        ntStatus = STATUS_BUFFER_OVERFLOW;
                    } else if(PropertyRequest->ValueSize == 2 * sizeof(ULONG))
                    {
                        PKSAUDIO_MIXCAP_TABLE MixCaps = (PKSAUDIO_MIXCAP_TABLE)PropertyRequest->Value;
                        MixCaps->InputChannels = 2;
                        MixCaps->OutputChannels = 2;
                        ntStatus = STATUS_SUCCESS;
                    } else if(PropertyRequest->ValueSize >= 2 * sizeof(ULONG) + 4 * sizeof(KSAUDIO_MIX_CAPS))
                    {
                        PropertyRequest->ValueSize = 2 * sizeof(ULONG) + 4 * sizeof(KSAUDIO_MIX_CAPS);

                        PKSAUDIO_MIXCAP_TABLE MixCaps = (PKSAUDIO_MIXCAP_TABLE)PropertyRequest->Value;
                        MixCaps->InputChannels = 2;
                        MixCaps->OutputChannels = 2;
                        for(count = 0; count < 4; count++)
                        {
                            if((count == 0) || (count == 3))
                            {
                                MixCaps->Capabilities[count].Mute = TRUE;
                                MixCaps->Capabilities[count].Minimum = 0;
                                MixCaps->Capabilities[count].Maximum = 0;
                                MixCaps->Capabilities[count].Reset = 0;
                            } else
                            {
                                MixCaps->Capabilities[count].Mute = FALSE;
                                MixCaps->Capabilities[count].Minimum = LONG_MIN;
                                MixCaps->Capabilities[count].Maximum = LONG_MIN;
                                MixCaps->Capabilities[count].Reset = LONG_MIN;
                            }
                        }
                        ntStatus = STATUS_SUCCESS;
                    }
                    break;


                // 1x2 Switch
                case MIC_WAVEIN_SUPERMIX:
                    if(!PropertyRequest->ValueSize)
                    {
                        PropertyRequest->ValueSize = 2 * sizeof(ULONG) + 2 * sizeof(KSAUDIO_MIX_CAPS);
                        ntStatus = STATUS_BUFFER_OVERFLOW;
                    } else if(PropertyRequest->ValueSize == 2 * sizeof(ULONG))
                    {
                        PKSAUDIO_MIXCAP_TABLE MixCaps = (PKSAUDIO_MIXCAP_TABLE)PropertyRequest->Value;
                        MixCaps->InputChannels = 1;
                        MixCaps->OutputChannels = 2;
                        ntStatus = STATUS_SUCCESS;
                    } else if(PropertyRequest->ValueSize >= 2 * sizeof(ULONG) + 2 * sizeof(KSAUDIO_MIX_CAPS))
                    {
                        PropertyRequest->ValueSize = 2 * sizeof(ULONG) + 2 * sizeof(KSAUDIO_MIX_CAPS);

                        PKSAUDIO_MIXCAP_TABLE MixCaps = (PKSAUDIO_MIXCAP_TABLE)PropertyRequest->Value;
                        MixCaps->InputChannels = 1;
                        MixCaps->OutputChannels = 2;
                        for(count = 0; count < 2; count++)
                        {
                            MixCaps->Capabilities[count].Mute = TRUE;
                            MixCaps->Capabilities[count].Minimum = 0;
                            MixCaps->Capabilities[count].Maximum = 0;
                            MixCaps->Capabilities[count].Reset = 0;
                        }
                        ntStatus = STATUS_SUCCESS;
                    }
                    break;
            }

        } else if(PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
        {
            // service basic support request
            switch(PropertyRequest->Node)
            {
                case SYNTH_WAVEIN_SUPERMIX:
                case CD_WAVEIN_SUPERMIX:
                case LINEIN_WAVEIN_SUPERMIX:
                case CD_LINEOUT_SUPERMIX:
                case LINEIN_LINEOUT_SUPERMIX:
                case MIC_WAVEIN_SUPERMIX:
                    if(PropertyRequest->ValueSize >= (sizeof(KSPROPERTY_DESCRIPTION)))
                    {
                        // if return buffer can hold a KSPROPERTY_DESCRIPTION, return it
                        PKSPROPERTY_DESCRIPTION PropDesc = PKSPROPERTY_DESCRIPTION(PropertyRequest->Value);
    
                        PropDesc->AccessFlags       = KSPROPERTY_TYPE_BASICSUPPORT |
                                                      KSPROPERTY_TYPE_GET;
                        PropDesc->DescriptionSize   = sizeof(KSPROPERTY_DESCRIPTION);
                        PropDesc->PropTypeSet.Set   = KSPROPTYPESETID_General;
                        PropDesc->PropTypeSet.Id    = VT_ARRAY;
                        PropDesc->PropTypeSet.Flags = 0;
                        PropDesc->MembersListCount  = 0;
                        PropDesc->Reserved          = 0;
    
                        // set the return value size
                        PropertyRequest->ValueSize = sizeof(KSPROPERTY_DESCRIPTION);
                        ntStatus = STATUS_SUCCESS;
                    } else if(PropertyRequest->ValueSize >= sizeof(ULONG))
                    {
                        // if return buffer can hold a ULONG, return the access flags
                        PULONG AccessFlags = PULONG(PropertyRequest->Value);
                
                        *AccessFlags = KSPROPERTY_TYPE_BASICSUPPORT |
                                       KSPROPERTY_TYPE_GET;
                
                        // set the return value size
                        PropertyRequest->ValueSize = sizeof(ULONG);
                        ntStatus = STATUS_SUCCESS;                    
                    }
                    ntStatus = STATUS_SUCCESS;
                    break;
            }
        }
    }

    return ntStatus;
}

/*****************************************************************************
 * PropertyHandler_SuperMixTable()
 *****************************************************************************
 * Handles supermixer level accesses
 */
static
NTSTATUS
PropertyHandler_SuperMixTable
(
    IN      PPCPROPERTY_REQUEST   PropertyRequest
)
{
    PAGED_CODE();

    ASSERT(PropertyRequest);

    _DbgPrintF(DEBUGLVL_VERBOSE,("[CMiniportTopologyHDA::PropertyHandler_SuperMixTable]"));

    CMiniportTopologyHDA *that =
        (CMiniportTopologyHDA *) PropertyRequest->MajorTarget;

    NTSTATUS        ntStatus = STATUS_INVALID_PARAMETER;
    BYTE            dataL,dataR;

    // validate node
    if(PropertyRequest->Node != ULONG(-1))
    {
        if(PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
        {
            switch(PropertyRequest->Node)
            {
                // Full 2x2 Switches
                case SYNTH_WAVEIN_SUPERMIX:
                case CD_WAVEIN_SUPERMIX:
                case LINEIN_WAVEIN_SUPERMIX:
                    if(!PropertyRequest->ValueSize)
                    {
                        PropertyRequest->ValueSize = 4 * sizeof(KSAUDIO_MIXLEVEL);
                        ntStatus = STATUS_BUFFER_OVERFLOW;
                    } else if(PropertyRequest->ValueSize >= 4 * sizeof(KSAUDIO_MIXLEVEL))
                    {
                        PropertyRequest->ValueSize = 4 * sizeof(KSAUDIO_MIXLEVEL);

                        PKSAUDIO_MIXLEVEL MixLevel = (PKSAUDIO_MIXLEVEL)PropertyRequest->Value;

                        dataL = that->ReadBitsFromMixer( DSP_MIX_ADCMIXIDX_L,
                                                  2,
                                                  AccessParams[PropertyRequest->Node].BaseRegister );
                        dataR = that->ReadBitsFromMixer( DSP_MIX_ADCMIXIDX_R,
                                                  2,
                                                  AccessParams[PropertyRequest->Node].BaseRegister );

                        MixLevel[0].Mute = dataL & 0x2 ? FALSE : TRUE;          // left to left mute
                        MixLevel[0].Level = 0;

                        MixLevel[1].Mute = dataR & 0x2 ? FALSE : TRUE;          // left to right mute
                        MixLevel[1].Level = 0;

                        MixLevel[2].Mute = dataL & 0x1 ? FALSE : TRUE;          // right to left mute
                        MixLevel[2].Level = 0;

                        MixLevel[3].Mute = dataR & 0x1 ? FALSE : TRUE;          // right to right mute
                        MixLevel[3].Level = 0;

                        ntStatus = STATUS_SUCCESS;
                    }
                    break;

                // Limited 2x2 Switches
                case CD_LINEOUT_SUPERMIX:
                case LINEIN_LINEOUT_SUPERMIX:
                    if(!PropertyRequest->ValueSize)
                    {
                        PropertyRequest->ValueSize = 4 * sizeof(KSAUDIO_MIXLEVEL);
                        ntStatus = STATUS_BUFFER_OVERFLOW;
                    } else if(PropertyRequest->ValueSize >= 4 * sizeof(KSAUDIO_MIXLEVEL))
                    {
                        PropertyRequest->ValueSize = 4 * sizeof(KSAUDIO_MIXLEVEL);

                        PKSAUDIO_MIXLEVEL MixLevel = (PKSAUDIO_MIXLEVEL)PropertyRequest->Value;

                        dataL = that->ReadBitsFromMixer( DSP_MIX_OUTMIXIDX,
                                                   2,
                                                   AccessParams[PropertyRequest->Node].BaseRegister );

                        MixLevel[0].Mute = dataL & 0x2 ? FALSE : TRUE;          // left to left mute
                        MixLevel[0].Level = 0;

                        MixLevel[1].Mute = FALSE;
                        MixLevel[1].Level = LONG_MIN;

                        MixLevel[2].Mute = FALSE;
                        MixLevel[2].Level = LONG_MIN;

                        MixLevel[3].Mute = dataL & 0x1 ? FALSE : TRUE;          // right to right mute
                        MixLevel[3].Level = 0;

                        ntStatus = STATUS_SUCCESS;
                    }
                    break;


                // 1x2 Switch
                case MIC_WAVEIN_SUPERMIX:
                    if(!PropertyRequest->ValueSize)
                    {
                        PropertyRequest->ValueSize = 2 * sizeof(KSAUDIO_MIXLEVEL);
                        ntStatus = STATUS_BUFFER_OVERFLOW;
                    } else if(PropertyRequest->ValueSize >= 2 * sizeof(KSAUDIO_MIXLEVEL))
                    {
                        PropertyRequest->ValueSize = 2 * sizeof(KSAUDIO_MIXLEVEL);

                        PKSAUDIO_MIXLEVEL MixLevel = (PKSAUDIO_MIXLEVEL)PropertyRequest->Value;

                        dataL = that->ReadBitsFromMixer( DSP_MIX_ADCMIXIDX_L,
                                                  1,
                                                  MIXBIT_MIC_WAVEIN );
                        dataR = that->ReadBitsFromMixer( DSP_MIX_ADCMIXIDX_R,
                                                  1,
                                                  MIXBIT_MIC_WAVEIN );

                        MixLevel[0].Mute = dataL & 0x1 ? FALSE : TRUE;          // mono to left mute
                        MixLevel[0].Level = 0;

                        MixLevel[1].Mute = dataR & 0x1 ? FALSE : TRUE;          // mono to right mute
                        MixLevel[1].Level = 0;

                        ntStatus = STATUS_SUCCESS;
                    }
                    break;
            }

        } else if(PropertyRequest->Verb & KSPROPERTY_TYPE_SET)
        {
            switch(PropertyRequest->Node)
            {
                // Full 2x2 Switches
                case SYNTH_WAVEIN_SUPERMIX:
                case CD_WAVEIN_SUPERMIX:
                case LINEIN_WAVEIN_SUPERMIX:
                    if(PropertyRequest->ValueSize == 4 * sizeof(KSAUDIO_MIXLEVEL))
                    {
                        PKSAUDIO_MIXLEVEL MixLevel = (PKSAUDIO_MIXLEVEL)PropertyRequest->Value;

                        dataL = MixLevel[0].Mute ? 0x0 : 0x2;
                        dataL |= MixLevel[2].Mute ? 0x0 : 0x1;

                        dataR = MixLevel[1].Mute ? 0x0 : 0x2;
                        dataR |= MixLevel[3].Mute ? 0x0 : 0x1;


                        that->WriteBitsToMixer( DSP_MIX_ADCMIXIDX_L,
                                          2,
                                          AccessParams[PropertyRequest->Node].BaseRegister,
                                          dataL );

                        that->WriteBitsToMixer( DSP_MIX_ADCMIXIDX_R,
                                          2,
                                          AccessParams[PropertyRequest->Node].BaseRegister,
                                          dataR );

                        ntStatus = STATUS_SUCCESS;
                    }
                    break;

                // Limited 2x2 Switches
                case CD_LINEOUT_SUPERMIX:
                case LINEIN_LINEOUT_SUPERMIX:
                    if(PropertyRequest->ValueSize == 4 * sizeof(KSAUDIO_MIXLEVEL))
                    {
                        PKSAUDIO_MIXLEVEL MixLevel = (PKSAUDIO_MIXLEVEL)PropertyRequest->Value;

                        dataL = MixLevel[0].Mute ? 0x0 : 0x2;
                        dataL |= MixLevel[3].Mute ? 0x0 : 0x1;

                        that->WriteBitsToMixer( DSP_MIX_OUTMIXIDX,
                                          2,
                                          AccessParams[PropertyRequest->Node].BaseRegister,
                                          dataL );

                        ntStatus = STATUS_SUCCESS;
                    }
                    break;


                // 1x2 Switch
                case MIC_WAVEIN_SUPERMIX:
                    if(PropertyRequest->ValueSize == 2 * sizeof(KSAUDIO_MIXLEVEL))
                    {
                        PKSAUDIO_MIXLEVEL MixLevel = (PKSAUDIO_MIXLEVEL)PropertyRequest->Value;

                        dataL = MixLevel[0].Mute ? 0x0 : 0x1;
                        dataR = MixLevel[1].Mute ? 0x0 : 0x1;

                        that->WriteBitsToMixer( DSP_MIX_ADCMIXIDX_L,
                                          1,
                                          MIXBIT_MIC_WAVEIN,
                                          dataL );

                        that->WriteBitsToMixer( DSP_MIX_ADCMIXIDX_R,
                                          1,
                                          MIXBIT_MIC_WAVEIN,
                                          dataR );

                        ntStatus = STATUS_SUCCESS;
                    }
                    break;
            }

        } else if(PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
        {
            // service basic support request
            switch(PropertyRequest->Node)
            {
                case SYNTH_WAVEIN_SUPERMIX:
                case CD_WAVEIN_SUPERMIX:
                case LINEIN_WAVEIN_SUPERMIX:
                case CD_LINEOUT_SUPERMIX:
                case LINEIN_LINEOUT_SUPERMIX:
                case MIC_WAVEIN_SUPERMIX:
                    if(PropertyRequest->ValueSize >= (sizeof(KSPROPERTY_DESCRIPTION)))
                    {
                        // if return buffer can hold a KSPROPERTY_DESCRIPTION, return it
                        PKSPROPERTY_DESCRIPTION PropDesc = PKSPROPERTY_DESCRIPTION(PropertyRequest->Value);
    
                        PropDesc->AccessFlags       = KSPROPERTY_TYPE_BASICSUPPORT |
                                                      KSPROPERTY_TYPE_GET |
                                                      KSPROPERTY_TYPE_SET;
                        PropDesc->DescriptionSize   = sizeof(KSPROPERTY_DESCRIPTION);
                        PropDesc->PropTypeSet.Set   = KSPROPTYPESETID_General;
                        PropDesc->PropTypeSet.Id    = VT_ARRAY;
                        PropDesc->PropTypeSet.Flags = 0;
                        PropDesc->MembersListCount  = 0;
                        PropDesc->Reserved          = 0;
    
                        // set the return value size
                        PropertyRequest->ValueSize = sizeof(KSPROPERTY_DESCRIPTION);
                        ntStatus = STATUS_SUCCESS;
                    } else if(PropertyRequest->ValueSize >= sizeof(ULONG))
                    {
                        // if return buffer can hold a ULONG, return the access flags
                        PULONG AccessFlags = PULONG(PropertyRequest->Value);
                
                        *AccessFlags = KSPROPERTY_TYPE_BASICSUPPORT |
                                       KSPROPERTY_TYPE_GET |
                                       KSPROPERTY_TYPE_SET;
                
                        // set the return value size
                        PropertyRequest->ValueSize = sizeof(ULONG);
                        ntStatus = STATUS_SUCCESS;                    
                    }
                    break;
            }
        }
    }

    return ntStatus;
}

/*****************************************************************************
 * PropertyHandler_CpuResources()
 *****************************************************************************
 * Propcesses a KSPROPERTY_AUDIO_CPU_RESOURCES request
 */
static
NTSTATUS
PropertyHandler_CpuResources
(
    IN      PPCPROPERTY_REQUEST   PropertyRequest
)
{
    PAGED_CODE();

    ASSERT(PropertyRequest);

    _DbgPrintF(DEBUGLVL_VERBOSE,("[CMiniportTopologyHDA::PropertyHandler_CpuResources]"));

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    // validate node
    if(PropertyRequest->Node != ULONG(-1))
    {
        if(PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
        {
            if(PropertyRequest->ValueSize >= sizeof(LONG))
            {
                *(PLONG(PropertyRequest->Value)) = KSAUDIO_CPU_RESOURCES_NOT_HOST_CPU;
                PropertyRequest->ValueSize = sizeof(LONG);
                ntStatus = STATUS_SUCCESS;
            } else
            {
                ntStatus = STATUS_BUFFER_TOO_SMALL;
            }
        } else if(PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
        {
            if(PropertyRequest->ValueSize >= (sizeof(KSPROPERTY_DESCRIPTION)))
            {
                // if return buffer can hold a KSPROPERTY_DESCRIPTION, return it
                PKSPROPERTY_DESCRIPTION PropDesc = PKSPROPERTY_DESCRIPTION(PropertyRequest->Value);

                PropDesc->AccessFlags       = KSPROPERTY_TYPE_BASICSUPPORT |
                                              KSPROPERTY_TYPE_GET;
                PropDesc->DescriptionSize   = sizeof(KSPROPERTY_DESCRIPTION);
                PropDesc->PropTypeSet.Set   = KSPROPTYPESETID_General;
                PropDesc->PropTypeSet.Id    = VT_I4;
                PropDesc->PropTypeSet.Flags = 0;
                PropDesc->MembersListCount  = 0;
                PropDesc->Reserved          = 0;

                // set the return value size
                PropertyRequest->ValueSize = sizeof(KSPROPERTY_DESCRIPTION);
                ntStatus = STATUS_SUCCESS;
            } else if(PropertyRequest->ValueSize >= sizeof(ULONG))
            {
                // if return buffer can hold a ULONG, return the access flags
                PULONG AccessFlags = PULONG(PropertyRequest->Value);
        
                *AccessFlags = KSPROPERTY_TYPE_BASICSUPPORT |
                               KSPROPERTY_TYPE_GET |
                               KSPROPERTY_TYPE_SET;
        
                // set the return value size
                PropertyRequest->ValueSize = sizeof(ULONG);
                ntStatus = STATUS_SUCCESS;                    
            }
        }
    }

    return ntStatus;
}


#pragma code_seg()

/*****************************************************************************
 * PropertyHandler_Level()
 *****************************************************************************
 * Accesses a KSAUDIO_LEVEL property.
 * This function got called at IRQL 2 so i'm making it non pageable (and anything it calls too)
 */
static
NTSTATUS
PropertyHandler_Level
(
    IN      PPCPROPERTY_REQUEST   PropertyRequest
)
{
    ASSERT(PropertyRequest);

    _DbgPrintF(DEBUGLVL_VERBOSE,("[CMiniportTopologyHDA::PropertyHandler_Level]"));

    CMiniportTopologyHDA *that =
        (CMiniportTopologyHDA *) PropertyRequest->MajorTarget;

    NTSTATUS        ntStatus = STATUS_INVALID_PARAMETER;
    ULONG           count;
    LONG            channel;

    // validate node
    if(PropertyRequest->Node != ULONG(-1))
    {
        if(PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
        {
            // get the instance channel parameter
            if(PropertyRequest->InstanceSize >= sizeof(LONG))
            {
                channel = *(PLONG(PropertyRequest->Instance));

                // only support get requests on either mono/left (0) or right (1) channels
                if ( (channel == CHAN_LEFT) || (channel == CHAN_RIGHT) )
                {
                    // validate and get the output parameter
                    if (PropertyRequest->ValueSize >= sizeof(LONG))
                    {
                        PLONG Level = (PLONG)PropertyRequest->Value;

                        // switch on node if
                        switch(PropertyRequest->Node)
                        {
                            case WAVEOUT_VOLUME:
                            case SYNTH_VOLUME:
                            case CD_VOLUME:
                            case LINEIN_VOLUME:
                            case MIC_VOLUME:
                            case LINEOUT_VOL:
                                // check if volume property request
                                if(PropertyRequest->PropertyItem->Id == KSPROPERTY_AUDIO_VOLUMELEVEL)
                                {
                                    // bail out if a right channel request on the mono mic volume
                                    if( (PropertyRequest->Node == MIC_VOLUME) && (channel != CHAN_LEFT) )
                                    {
                                        break;
                                    }
                                    *Level = ControlValueCache[ AccessParams[PropertyRequest->Node].CacheOffset + channel ];
                                    PropertyRequest->ValueSize = sizeof(LONG);
                                    ntStatus = STATUS_SUCCESS;
                                }
                                break;
        
                            case LINEOUT_GAIN:
                            case WAVEIN_GAIN:
                                // check if volume property request
                                if(PropertyRequest->PropertyItem->Id == KSPROPERTY_AUDIO_VOLUMELEVEL)
                                {
                                    *Level = ControlValueCache[ AccessParams[PropertyRequest->Node].CacheOffset + channel ];
                                    PropertyRequest->ValueSize = sizeof(LONG);
                                    ntStatus = STATUS_SUCCESS;
                                }
                                break;

                            case LINEOUT_BASS:
                            case LINEOUT_TREBLE:
                                if( ( (PropertyRequest->PropertyItem->Id == KSPROPERTY_AUDIO_BASS) &&
                                      (PropertyRequest->Node == LINEOUT_BASS) ) ||
                                    ( (PropertyRequest->PropertyItem->Id == KSPROPERTY_AUDIO_TREBLE) &&
                                      (PropertyRequest->Node == LINEOUT_TREBLE) ) )
                                {
                                    *Level = ControlValueCache[ AccessParams[PropertyRequest->Node].CacheOffset + channel ];
                                    PropertyRequest->ValueSize = sizeof(LONG);
                                    ntStatus = STATUS_SUCCESS;
                                }
                                break;
                        }
                    }
                }
            }

        } else if(PropertyRequest->Verb & KSPROPERTY_TYPE_SET)
        {
            // get the instance channel parameter
            if(PropertyRequest->InstanceSize >= sizeof(LONG))
            {
                channel = *(PLONG(PropertyRequest->Instance));

                // only support set requests on either mono/left (0), right (1), or master (-1) channels
                if ( (channel == CHAN_LEFT) || (channel == CHAN_RIGHT) || (channel == CHAN_MASTER))
                {
                    // validate and get the input parameter
                    if (PropertyRequest->ValueSize == sizeof(LONG))
                    {
                        PLONG Level = (PLONG)PropertyRequest->Value;

                        // switch on the node id
                        switch(PropertyRequest->Node)
                        {
                            case WAVEOUT_VOLUME:
                            case SYNTH_VOLUME:
                            case CD_VOLUME:
                            case LINEIN_VOLUME:
                            case MIC_VOLUME:
                            case LINEOUT_VOL:
                                if(PropertyRequest->PropertyItem->Id == KSPROPERTY_AUDIO_VOLUMELEVEL)
                                {
                                    // convert the level to register bits
                                    if(*Level <= (-62 << 16))
                                    {
                                        count = 0;
                                    } else if(*Level >= 0)
                                    {
                                        count = 0x1F;
                                    } else
                                    {
                                        count = (((*Level >> 16) + 62) >> 1) & 0x1F;
                                    }

                                    // set right channel if channel requested is right or master
                                    // and node is not mic volume (mono)
                                    if ( ( (channel == CHAN_RIGHT) || (channel == CHAN_MASTER) ) &&
                                         ( PropertyRequest->Node != MIC_VOLUME ) )
                                    {
                                        // cache the commanded control value
                                        ControlValueCache[ AccessParams[PropertyRequest->Node].CacheOffset + CHAN_RIGHT ] = *Level;

                                        that->WriteBitsToMixer( AccessParams[PropertyRequest->Node].BaseRegister+1,
                                                          5,
                                                          3,
                                                          BYTE(count) );
                                        ntStatus = STATUS_SUCCESS;
                                    }
                                    // set the left channel if channel requested is left or master
                                    if ( (channel == CHAN_LEFT) || (channel == CHAN_MASTER) )
                                    {
                                        // cache the commanded control value
                                        ControlValueCache[ AccessParams[PropertyRequest->Node].CacheOffset + CHAN_LEFT ] = *Level;
                                        
                                        that->WriteBitsToMixer( AccessParams[PropertyRequest->Node].BaseRegister,
                                                          5,
                                                          3,
                                                          BYTE(count) );
                                        ntStatus = STATUS_SUCCESS;
                                    }
                                }
                                break;
        
                            case LINEOUT_GAIN:
                            case WAVEIN_GAIN:
                                if(PropertyRequest->PropertyItem->Id == KSPROPERTY_AUDIO_VOLUMELEVEL)
                                {                                                                        
                                    // determine register bits
                                    if(*Level >= (18 << 16))
                                    {
                                        count = 0x3;
                                    } else if(*Level <= 0)
                                    {
                                        count = 0;
                                    } else
                                    {
                                        count = (*Level >> 17) / 3;
                                    }
    
                                    // set right channel if channel requested is right or master
                                    if ( (channel == CHAN_RIGHT) || (channel == CHAN_MASTER) )
                                    {
                                        // cache the commanded control value
                                        ControlValueCache[ AccessParams[PropertyRequest->Node].CacheOffset + CHAN_RIGHT ] = *Level;

                                        that->WriteBitsToMixer( AccessParams[PropertyRequest->Node].BaseRegister+1,
                                                          2,
                                                          6,
                                                          BYTE(count) );
                                        ntStatus = STATUS_SUCCESS;
                                    }
                                    // set the left channel if channel requested is left or master
                                    if ( (channel == CHAN_LEFT) || (channel == CHAN_MASTER) )
                                    {
                                        // cache the commanded control value
                                        ControlValueCache[ AccessParams[PropertyRequest->Node].CacheOffset + CHAN_LEFT ] = *Level;

                                        that->WriteBitsToMixer( AccessParams[PropertyRequest->Node].BaseRegister,
                                                          2,
                                                          6,
                                                          BYTE(count) );
                                        ntStatus = STATUS_SUCCESS;
                                    }
                                }
                                break;
        
                            case LINEOUT_BASS:
                            case LINEOUT_TREBLE:
                                if( ( (PropertyRequest->PropertyItem->Id == KSPROPERTY_AUDIO_BASS) &&
                                      (PropertyRequest->Node == LINEOUT_BASS) ) ||
                                    ( (PropertyRequest->PropertyItem->Id == KSPROPERTY_AUDIO_TREBLE) &&
                                      (PropertyRequest->Node == LINEOUT_TREBLE) ) )
                                {
                                    // determine register bits
                                    if(*Level <= (-14 << 16))
                                    {
                                        count = 0;
                                    } else if(*Level >= (14 << 16))
                                    {
                                        count = 0xF;
                                    } else
                                    {
                                        count = ((*Level >> 16) + 14) >> 1;
                                    }

                                    // set right channel if channel requested is right or master
                                    if ( (channel == CHAN_RIGHT) || (channel == CHAN_MASTER) )
                                    {
                                        // cache the commanded control value
                                        ControlValueCache[ AccessParams[PropertyRequest->Node].CacheOffset + CHAN_RIGHT ] = *Level;
        
                                        that->WriteBitsToMixer( AccessParams[PropertyRequest->Node].BaseRegister + 1,
                                                          4,
                                                          4,
                                                          BYTE(count) );
                                        ntStatus = STATUS_SUCCESS;
                                    }
                                    // set the left channel if channel requested is left or master
                                    if ( (channel == CHAN_LEFT) || (channel == CHAN_MASTER) )
                                    {
                                        // cache the commanded control value
                                        ControlValueCache[ AccessParams[PropertyRequest->Node].CacheOffset + CHAN_LEFT ] = *Level;
                                        
                                        that->WriteBitsToMixer( AccessParams[PropertyRequest->Node].BaseRegister,
                                                          4,
                                                          4,
                                                          BYTE(count) );
                                        ntStatus = STATUS_SUCCESS;
                                    }
                                }
                                break;
                        }
                    }
                }
            }

        } else if(PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
        {
            // service basic support request
            switch(PropertyRequest->Node)
            {
                case WAVEOUT_VOLUME:
                case SYNTH_VOLUME:
                case CD_VOLUME:
                case LINEIN_VOLUME:
                case MIC_VOLUME:
                case LINEOUT_VOL:
                case LINEOUT_GAIN:
                case WAVEIN_GAIN:
                    if(PropertyRequest->PropertyItem->Id == KSPROPERTY_AUDIO_VOLUMELEVEL)
                    {
                        ntStatus = BasicSupportHandler(PropertyRequest);
                    }
                    break;

                case LINEOUT_BASS:
                case LINEOUT_TREBLE:
                    if( ( (PropertyRequest->PropertyItem->Id == KSPROPERTY_AUDIO_BASS) &&
                          (PropertyRequest->Node == LINEOUT_BASS) ) ||
                        ( (PropertyRequest->PropertyItem->Id == KSPROPERTY_AUDIO_TREBLE) &&
                          (PropertyRequest->Node == LINEOUT_TREBLE) ) )
                    {
                        ntStatus = BasicSupportHandler(PropertyRequest);
                    }
                    break;
            }
        }
    }

    return ntStatus;
}

/*****************************************************************************
 * ThisManyOnes()
 *****************************************************************************
 * Returns a byte with the indicated number of ones in the low end.
 */
inline
BYTE
ThisManyOnes
(
    IN      BYTE Ones
)
{
    return ~(BYTE(0xff) << Ones);
}

/*****************************************************************************
 * CMiniportTopologyHDA::ReadBitsFromMixer()
 *****************************************************************************
 * Reads specified bits from a mixer register.
 */
BYTE
CMiniportTopologyHDA::
ReadBitsFromMixer
(
    BYTE Reg,
    BYTE Bits,
    BYTE Shift
)
{
    BYTE data = AdapterCommon->MixerRegRead(Reg);

    return( data >> Shift) & ThisManyOnes(Bits);
}

/*****************************************************************************
 * CMiniportTopologyHDA::WriteBitsToMixer()
 *****************************************************************************
 * Writes specified bits to a mixer register.
 */
void
CMiniportTopologyHDA::
WriteBitsToMixer
(
    BYTE Reg,
    BYTE Bits,
    BYTE Shift,
    BYTE Value
)
{
    BYTE mask = ThisManyOnes(Bits) << Shift;
    BYTE data = AdapterCommon->MixerRegRead(Reg);

    if(Reg < DSP_MIX_MAXREGS)
    {
        AdapterCommon->MixerRegWrite( Reg,
                                      (data & ~mask) | ( (Value << Shift) & mask));
    }
}