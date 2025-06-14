/********************************************************************************
**    Copyright (c) 1998-1999 Microsoft Corporation. All Rights Reserved.
**
**       Portions Copyright (c) 1998-1999 Intel Corporation
**
********************************************************************************/

// Every debug output has "Modulname text".
static char STR_MODULENAME[] = "prophnd: ";

#include <limits.h>
#include "mintopo.h"

// These are the values passed to the property handler in the instance
// parameter that normally represents the channel.
const LONG CHAN_LEFT   = 0;
const LONG CHAN_RIGHT  = 1;
const LONG CHAN_MASTER = -1;

// When the user moves the fader of a volume control to the bottom, the
// system calls the property handler with a -MAXIMUM dB value. That's not
// what you returned at a basic support "data range", but the most negative
// value you can represent with a long.
const long PROP_MOST_NEGATIVE = (long)0x80000000;

// Looking at the structure stMapNodeToReg, it defines a mask for a node.
// A volume node for instance could be used to prg. the left or right channel,
// so we have to mask the mask in stMapNodeToReg with a "left channel mask"
// or a "right channel mask" to only prg. the left or right channel.
const WORD AC97REG_MASK_LEFT  = 0x3F00;
const WORD AC97REG_MASK_RIGHT = 0x003F;
const WORD AC97REG_MASK_MUTE  = 0x8000;


// paged code goes here.
#pragma code_seg("PAGE")


/*****************************************************************************
 * CMiniportTopologyICH::GetDBValues
 *****************************************************************************
 * This function is used internally and does no parameter checking. The only
 * parameter that could be invalid is the node.
 * It returns the dB values (means minimum, maximum, step) of the node control,
 * mainly for the property call "basic support". Sure, the node must be a
 * volume or tone control node, not a mute or mux node.
 */
NTSTATUS CMiniportTopologyICH::GetDBValues
(
    IN PADAPTERCOMMON AdapterCommon,
    IN TopoNodes Node,
    OUT LONG *plMinimum,
    OUT LONG *plMaximum,
    OUT ULONG *puStep
)
{
    DOUT (DBG_PRINT, ("[CMiniportTopologyICH::GetDBValues]"));
    
    // This is going to be simple. Check the node and return the parameters.
    switch (Node)
    {
        // These nodes could have 5bit or 6bit controls, so we first
        // have to check this.
        case NODE_MASTEROUT_VOLUME:
        case NODE_HPOUT_VOLUME:
        case NODE_VIRT_MONOOUT_VOLUME1:
        case NODE_VIRT_MONOOUT_VOLUME2:
            // needed for the config query
            TopoNodeConfig  config;

            // which node to query?
            if (Node == NODE_MASTEROUT_VOLUME)
                config = NODEC_6BIT_MASTER_VOLUME;
            else
                if (Node == NODE_HPOUT_VOLUME)
                    config = NODEC_6BIT_HPOUT_VOLUME;
                else
                    config = NODEC_6BIT_MONOOUT_VOLUME;

            // check if we have 6th bit support.
            if (AdapterCommon->GetNodeConfig (config))
            {
                // 6bit control
                *plMaximum = 0;            // 0 dB
                *plMinimum = 0xFFA18000;   // -94.5 dB
                *puStep    = 0x00018000;   // 1.5 dB
            }
            else
            {
                // 5bit control
                *plMaximum = 0;            // 0 dB
                *plMinimum = 0xFFD18000;   // -46.5 dB
                *puStep    = 0x00018000;   // 1.5 dB
            }
            break;

        case NODE_PCBEEP_VOLUME:
            *plMaximum = 0;            // 0 dB
            *plMinimum = 0xFFD30000;   // -45 dB
            *puStep    = 0x00030000;   // 3 dB
            break;

        case NODE_PHONE_VOLUME:
        case NODE_MICIN_VOLUME:
        case NODE_LINEIN_VOLUME:
        case NODE_CD_VOLUME:
        case NODE_VIDEO_VOLUME:
        case NODE_AUX_VOLUME:
        case NODE_WAVEOUT_VOLUME:
            *plMaximum = 0x000C0000;   // 12 dB
            *plMinimum = 0xFFDD8000;   // -34.5 dB
            *puStep    = 0x00018000;   // 1.5 dB
            break;

    
        case NODE_VIRT_MASTER_INPUT_VOLUME1:
        case NODE_VIRT_MASTER_INPUT_VOLUME2:
        case NODE_VIRT_MASTER_INPUT_VOLUME3:
        case NODE_VIRT_MASTER_INPUT_VOLUME4:
        case NODE_VIRT_MASTER_INPUT_VOLUME5:
        case NODE_VIRT_MASTER_INPUT_VOLUME6:
        case NODE_VIRT_MASTER_INPUT_VOLUME7:
        case NODE_VIRT_MASTER_INPUT_VOLUME8:
        case NODE_MIC_VOLUME:
            *plMaximum = 0x00168000;   // 22.5 dB
            *plMinimum = 0;            // 0 dB
            *puStep    = 0x00018000;   // 1.5 dB
            break;

        case NODE_BASS:
        case NODE_TREBLE:
            *plMaximum = 0x000A8000;   // 10.5 dB
            *plMinimum = 0xFFF58000;   // -10.5 dB
            *puStep    = 0x00018000;   // 1.5 dB
            break;

        // These nodes can be fixed or variable.
        // Normally we won't display a fixed volume slider, but if 3D is
        // supported and both sliders are fixed, we have to display one fixed
        // slider for the advanced control panel.
        case NODE_VIRT_3D_CENTER:
        case NODE_VIRT_3D_DEPTH:
            if (AdapterCommon->GetNodeConfig (NODEC_3D_CENTER_ADJUSTABLE) &&
               (Node == NODE_VIRT_3D_CENTER))
            {
                *plMaximum = 0x000F0000;   // +15 dB
                *plMinimum = 0x00000000;   // 0 dB
                *puStep    = 0x00010000;   // 1 dB
            }
            else
            if (AdapterCommon->GetNodeConfig (NODEC_3D_DEPTH_ADJUSTABLE) &&
               (Node == NODE_VIRT_3D_DEPTH))
            {
                *plMaximum = 0x000F0000;   // +15 dB
                *plMinimum = 0x00000000;   // 0 dB
                *puStep    = 0x00010000;   // 1 dB
            }
            else
            {
                // In case it is fixed, read the value and return it.
                WORD wRegister;

                // read the register
                if (!NT_SUCCESS (AdapterCommon->ReadCodecRegister (
                            AdapterCommon->GetNodeReg (Node), &wRegister)))
                    wRegister = 0;      // in case we fail.

                // mask out the control
                wRegister &= AdapterCommon->GetNodeMask (Node);
                if (Node == NODE_VIRT_3D_CENTER)
                {
                    wRegister >>= 8;
                }
                // calculate the dB value.
                *plMaximum = (DWORD)(-wRegister) << 16;    // fixed value
                *plMinimum = (DWORD)(-wRegister) << 16;    // fixed value
                *puStep    = 0x00010000;   // 1 dB
            }
            break;

        case NODE_INVALID:
        default:
            // poeser pupe, tu.
            DOUT (DBG_ERROR, ("GetDBValues: Invalid node requested."));
            return STATUS_INVALID_PARAMETER;
    }

    return STATUS_SUCCESS;
}
 
/*****************************************************************************
 * CMiniportTopologyICH::PropertyHandler_OnOff
 *****************************************************************************
 * Accesses a KSAUDIO_ONOFF value property.
 * This function (property handler) is called by portcls every time there is a
 * get or a set request for the node. The connection between the node type and
 * the property handler is made in the automation table which is referenced
 * when you register the node.
 * We use this property handler for all nodes that have a checkbox, means mute
 * controls and the special checkbox controls under advanced properties, which
 * are AGC and LOUDNESS.
 */
NTSTATUS CMiniportTopologyICH::PropertyHandler_OnOff
(
    IN      PPCPROPERTY_REQUEST   PropertyRequest
)
{
    PAGED_CODE ();

    ASSERT (PropertyRequest);

    NTSTATUS        ntStatus = STATUS_INVALID_PARAMETER;
    LONG            channel;
    TopoNodes       NodeDef;
    // The major target is the object pointer to the topology miniport.
    CMiniportTopologyICH *that =
        (CMiniportTopologyICH *) PropertyRequest->MajorTarget;

    ASSERT (that);

    DOUT (DBG_PRINT, ("[CMiniportTopologyICH::PropertyHandler_OnOff]"));

    // validate node
    if (PropertyRequest->Node == (ULONG)-1)
        return ntStatus;

    // do the appropriate action for the request.

    // we should do a get or a set?
    if ((PropertyRequest->Verb & KSPROPERTY_TYPE_GET) ||
        (PropertyRequest->Verb & KSPROPERTY_TYPE_SET))
    {
        // validate parameters
        if ((PropertyRequest->InstanceSize < sizeof(LONG)) ||
            (PropertyRequest->ValueSize < sizeof(BOOL)))
            return ntStatus;

        // get channel
        channel = *(PLONG)PropertyRequest->Instance;

        // check channel types, return when unknown
        // as you can see, we have no multichannel support.
        if ((channel != CHAN_LEFT) &&
            (channel != CHAN_RIGHT) &&
            (channel != CHAN_MASTER))
            return ntStatus;

        // get the buffer
        PBOOL OnOff = (PBOOL)PropertyRequest->Value;

        // Switch on the node id. This is just for parameter checking.
        // If something goes wrong, we will immediately return with
        // ntStatus, which is STATUS_INVALID_PARAMETER.
        switch (NodeDef = that->TransNodeNrToNodeDef (PropertyRequest->Node))
        {
            // these are mono channels, don't respond to a right channel
            // request.
            case NODE_PCBEEP_MUTE:
            case NODE_PHONE_MUTE:
            case NODE_MIC_MUTE:
            case NODE_MICIN_MUTE:
                // check type
                if (PropertyRequest->PropertyItem->Id != KSPROPERTY_AUDIO_MUTE)
                    return ntStatus;
                // check channel.
                if (channel == CHAN_RIGHT)
                    return ntStatus;
                break;

            // Well, this one is a AGC, although there is no _automatic_ gain
            // control, but we have a mic boost (which is some kind of manual
            // gain control).
            // The 3D Bypass is a real fake, but that's how you get check boxes
            // on the advanced control panel.
            case NODE_VIRT_WAVEOUT_3D_BYPASS:
            case NODE_MIC_BOOST:
                // check type
                if (PropertyRequest->PropertyItem->Id != KSPROPERTY_AUDIO_AGC)
                    return ntStatus;
                // check channel.
                if (channel == CHAN_RIGHT)
                    return ntStatus;
                break;

            // Simulated Stereo is a AGC control in a stereo path.
            case NODE_SIMUL_STEREO:
                // check type
                if (PropertyRequest->PropertyItem->Id != KSPROPERTY_AUDIO_AGC)
                    return ntStatus;
                break;

            // well, this one is a loudness control, again, it has it's own type
            // we respond to all channels.
            case NODE_LOUDNESS:
                // check type
                if (PropertyRequest->PropertyItem->Id != KSPROPERTY_AUDIO_LOUDNESS)
                    return ntStatus;
                break;

            // For 3D Enable and Mic are mono controls exposed as loudness.
            case NODE_VIRT_3D_ENABLE:
            case NODE_MIC_SELECT:
                // check type
                if (PropertyRequest->PropertyItem->Id != KSPROPERTY_AUDIO_LOUDNESS)
                    return ntStatus;
                // check channel.
                if (channel == CHAN_RIGHT)
                    return ntStatus;
                break;

            // these are stereo channels; a get on left or right or master
            // should return the mute.
            case NODE_WAVEOUT_MUTE:
            case NODE_LINEIN_MUTE:
            case NODE_CD_MUTE:
            case NODE_VIDEO_MUTE:
            case NODE_AUX_MUTE:
            case NODE_MASTEROUT_MUTE:
            case NODE_HPOUT_MUTE:
                // just check the type.
                if (PropertyRequest->PropertyItem->Id != KSPROPERTY_AUDIO_MUTE)
                    return ntStatus;
                break;

            case NODE_INVALID:
            default:
                // Ooops.
                DOUT (DBG_ERROR, ("PropertyHandler_OnOff: Invalid node requested."));
                return ntStatus;
        }

        // Now, do some action!

        if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
        {
            WORD    wRegister;

            // get the register and read it.
            ntStatus = that->AdapterCommon->ReadCodecRegister (
                    that->AdapterCommon->GetNodeReg (NodeDef), &wRegister);
            wRegister &= that->AdapterCommon->GetNodeMask (NodeDef);
            // "normalize"
            *OnOff = wRegister ? TRUE : FALSE;
            PropertyRequest->ValueSize = sizeof(BOOL);
            DOUT (DBG_PROPERTY, ("GET: %s = 0x%x", NodeStrings[NodeDef], *OnOff));
            // ntStatus was set with the read call! whatever this is, return it.
        }
        else    // this must be a set.
        {
            // get the register + mask and write it.
            DOUT (DBG_PROPERTY, ("SET: %s -> 0x%x", NodeStrings[NodeDef], *OnOff));
            ntStatus = that->AdapterCommon->WriteCodecRegister (
                    that->AdapterCommon->GetNodeReg (NodeDef),
                    *OnOff ? -1 : 0,
                    that->AdapterCommon->GetNodeMask (NodeDef));
            // ntStatus was set with the write call! whatever this is, return it.
        }
    }
    return ntStatus;
}

/*****************************************************************************
 * CMiniportTopologyICH::BasicSupportHandler
 *****************************************************************************
 * Assists in BASICSUPPORT accesses on level properties.
 * This function is called internally every time there is a "basic support"
 * request on a volume or tone control. The basic support is used to retrieve
 * some information about the range of the control (from - to dB, steps) and
 * which type of control (tone, volume).
 * Basically, this function just calls GetDBValues to get the range information
 * and fills the rest of the structure with some constants.
 */
NTSTATUS CMiniportTopologyICH::BasicSupportHandler
(
    IN      PPCPROPERTY_REQUEST   PropertyRequest
)
{
    PAGED_CODE ();

    ASSERT (PropertyRequest);

    DOUT (DBG_PRINT, ("[CMiniportTopologyICH::BasicSupportHandler]"));

    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    // The major target is the object pointer to the topology miniport.
    CMiniportTopologyICH *that =
        (CMiniportTopologyICH *) PropertyRequest->MajorTarget;

    ASSERT (that);


    // if there is enough space for a KSPROPERTY_DESCRIPTION information
    if (PropertyRequest->ValueSize >= (sizeof(KSPROPERTY_DESCRIPTION)))
    {
        // we return a KSPROPERTY_DESCRIPTION structure.
        PKSPROPERTY_DESCRIPTION PropDesc = (PKSPROPERTY_DESCRIPTION)PropertyRequest->Value;

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

        // if return buffer can also hold a range description, return it too
        if (PropertyRequest->ValueSize >= (sizeof(KSPROPERTY_DESCRIPTION) +
            sizeof(KSPROPERTY_MEMBERSHEADER) + sizeof(KSPROPERTY_STEPPING_LONG)))
        {
            // fill in the members header
            PKSPROPERTY_MEMBERSHEADER Members = (PKSPROPERTY_MEMBERSHEADER)(PropDesc + 1);

            Members->MembersFlags   = KSPROPERTY_MEMBER_STEPPEDRANGES;
            Members->MembersSize    = sizeof(KSPROPERTY_STEPPING_LONG);
            Members->MembersCount   = 1;
            Members->Flags          = 0;

            // fill in the stepped range
            PKSPROPERTY_STEPPING_LONG Range = (PKSPROPERTY_STEPPING_LONG)(Members + 1);

            ntStatus = GetDBValues (that->AdapterCommon,
                                    that->TransNodeNrToNodeDef (PropertyRequest->Node),
                                    &Range->Bounds.SignedMinimum,
                                    &Range->Bounds.SignedMaximum,
                                    &Range->SteppingDelta);

            Range->Reserved         = 0;

            // set the return value size
            PropertyRequest->ValueSize = sizeof(KSPROPERTY_DESCRIPTION) +
                                         sizeof(KSPROPERTY_MEMBERSHEADER) +
                                         sizeof(KSPROPERTY_STEPPING_LONG);

            DOUT (DBG_PROPERTY, ("BASIC_SUPPORT: %s max=0x%x min=0x%x step=0x%x",
                NodeStrings[that->TransNodeNrToNodeDef (PropertyRequest->Node)],
                Range->Bounds.SignedMaximum, Range->Bounds.SignedMinimum,
                Range->SteppingDelta));
        } else
        {
            // we hadn't enough space for the range information; 
            // set the return value size
            PropertyRequest->ValueSize = sizeof(KSPROPERTY_DESCRIPTION);
        }

        ntStatus = STATUS_SUCCESS;
    }
    else if (PropertyRequest->ValueSize >= sizeof(ULONG))
    {
        // if return buffer can hold a ULONG, return the access flags
        PULONG AccessFlags = (PULONG)PropertyRequest->Value;

        *AccessFlags = KSPROPERTY_TYPE_BASICSUPPORT |
                       KSPROPERTY_TYPE_GET |
                       KSPROPERTY_TYPE_SET;

        // set the return value size
        PropertyRequest->ValueSize = sizeof(ULONG);
        ntStatus = STATUS_SUCCESS;
    }

    // In case there was not even enough space for a ULONG in the return buffer,
    // we fail this request with STATUS_INVALID_DEVICE_REQUEST.
    // Any other case will return STATUS_SUCCESS.
    return ntStatus;
}

/*****************************************************************************
 * CMiniportTopologyICH::PropertyHandler_Level
 *****************************************************************************
 * Accesses a KSAUDIO_LEVEL property.
 * This function (property handler) is called by portcls every time there is a
 * get, set or basic support request for the node. The connection between the
 * node type and the property handler is made in the automation table which is
 * referenced when you register the node.
 * We use this property handler for all volume controls (and virtual volume
 * controls for recording).
 */
NTSTATUS CMiniportTopologyICH::PropertyHandler_Level
(
    IN      PPCPROPERTY_REQUEST   PropertyRequest
)
{
    PAGED_CODE ();

    ASSERT (PropertyRequest);

    DOUT (DBG_PRINT, ("[CMiniportTopologyICH::PropertyHandler_Level]"));

    NTSTATUS        ntStatus = STATUS_INVALID_PARAMETER;
    TopoNodes       NodeDef;
    LONG            channel;
    LONG            lMinimum, lMaximum;
    ULONG           uStep;
    // The major target is the object pointer to the topology miniport.
    CMiniportTopologyICH *that =
        (CMiniportTopologyICH *) PropertyRequest->MajorTarget;

    ASSERT (that);

    // validate node
    if (PropertyRequest->Node == (ULONG)-1)
        return ntStatus;

    // do the appropriate action for the request.

    // we should do a get or a set?
    if ((PropertyRequest->Verb & KSPROPERTY_TYPE_GET) ||
        (PropertyRequest->Verb & KSPROPERTY_TYPE_SET))
    {
        // validate parameters
        if ((PropertyRequest->InstanceSize < sizeof(LONG)) ||
            (PropertyRequest->ValueSize < sizeof(LONG)))
            return ntStatus;

        // get channel information
        channel = *((PLONG)PropertyRequest->Instance);

        // check channel types, return when unknown
        // as you can see, we have no multichannel support.
        if ((channel != CHAN_LEFT) &&
            (channel != CHAN_RIGHT) &&
            (channel != CHAN_MASTER))
            return ntStatus;

        // get the buffer
        PLONG Level = (PLONG)PropertyRequest->Value;

        // Switch on the node id. This is just for parameter checking.
        // If something goes wrong, we will immideately return with
        // ntStatus, which is STATUS_INVALID_PARAMETER.
        switch(NodeDef = that->TransNodeNrToNodeDef (PropertyRequest->Node))
        {
            // these are mono channels, don't respond to a right channel
            // request.
            case NODE_PCBEEP_VOLUME:
            case NODE_PHONE_VOLUME:
            case NODE_MIC_VOLUME:
            case NODE_VIRT_MONOOUT_VOLUME1:
            case NODE_VIRT_MONOOUT_VOLUME2:
            case NODE_VIRT_MASTER_INPUT_VOLUME1:
            case NODE_VIRT_MASTER_INPUT_VOLUME7:
            case NODE_VIRT_MASTER_INPUT_VOLUME8:
            case NODE_MICIN_VOLUME:
                // check type
                if (PropertyRequest->PropertyItem->Id != KSPROPERTY_AUDIO_VOLUMELEVEL)
                    return ntStatus;
                // check channel
                if (channel == CHAN_RIGHT)
                    return ntStatus;
                // Well, this is a fake for the routine below that should work
                // for all nodes ... On AC97 the right channel are the LSBs and
                // mono channels have only LSBs used. Windows however thinks that
                // mono channels are left channels (only). So we could say here
                // we have a right channel request (to prg. the LSBs) instead of
                // a left channel request. But we have some controls that are HW-
                // stereo, but exposed to the system as mono. These are the virtual
                // volume controls in front of the wave-in muxer for the MIC, PHONE
                // and MONO MIX signals (see to the switch:
                // NODE_VIRT_MASTER_INPUT_VOLUME1, 7 and 8). Saying we have a MASTER
                // request makes sure the value is prg. for left and right channel,
                // but on HW-mono controls the right channel is prg. only, cause the
                // mask in ac97reg.h leads to a 0-mask for left channel prg. which
                // just does nothing ;)
                channel = CHAN_MASTER;
                break;
            
            // These are stereo channels.
            case NODE_MASTEROUT_VOLUME:
            case NODE_HPOUT_VOLUME:
            case NODE_LINEIN_VOLUME:
            case NODE_CD_VOLUME:
            case NODE_VIDEO_VOLUME:
            case NODE_AUX_VOLUME:
            case NODE_WAVEOUT_VOLUME:
            case NODE_VIRT_MASTER_INPUT_VOLUME2:
            case NODE_VIRT_MASTER_INPUT_VOLUME3:
            case NODE_VIRT_MASTER_INPUT_VOLUME4:
            case NODE_VIRT_MASTER_INPUT_VOLUME5:
            case NODE_VIRT_MASTER_INPUT_VOLUME6:
                // check type
                if (PropertyRequest->PropertyItem->Id != KSPROPERTY_AUDIO_VOLUMELEVEL)
                    return ntStatus;
                // check channel; we don't support a get with master
                if ((channel == CHAN_MASTER) &&
                    (PropertyRequest->Verb & KSPROPERTY_TYPE_GET))
                    return ntStatus;
                break;

            case NODE_INVALID:
            default:
                // Ooops
                DOUT (DBG_ERROR, ("PropertyHandler_Level: Invalid node requested."));
                return ntStatus;
        }

        // Now, do some action!

        // get the registered dB values.
        ntStatus = GetDBValues (that->AdapterCommon, NodeDef, &lMinimum,
                                &lMaximum, &uStep);
        if (!NT_SUCCESS (ntStatus))
            return ntStatus;

        // do a get
        if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
        {
            WORD    wRegister;

            // first get the stuff.
            ntStatus = that->AdapterCommon->ReadCodecRegister (
                    that->AdapterCommon->GetNodeReg (NodeDef), &wRegister);
            if (!NT_SUCCESS (ntStatus))
                return ntStatus;

            // mask out every unused bit and rotate.
            if (channel == CHAN_LEFT)
            {
                wRegister = (wRegister & (that->AdapterCommon->GetNodeMask (NodeDef)
                            & AC97REG_MASK_LEFT)) >> 8;
            }
            else    // here goes mono or stereo-right
            {
                wRegister &= (that->AdapterCommon->GetNodeMask (NodeDef) &
                              AC97REG_MASK_RIGHT);
            }

            // Oops - NODE_PCBEEP_VOLUME doesn't use bit0. We have to adjust.
            if (NodeDef == NODE_PCBEEP_VOLUME)
                wRegister >>= 1;

            // we have to translate the reg to dB.dB value.

            switch (NodeDef)
            {
                // for record, we calculate it reverse.
                case NODE_VIRT_MASTER_INPUT_VOLUME1:
                case NODE_VIRT_MASTER_INPUT_VOLUME2:
                case NODE_VIRT_MASTER_INPUT_VOLUME3:
                case NODE_VIRT_MASTER_INPUT_VOLUME4:
                case NODE_VIRT_MASTER_INPUT_VOLUME5:
                case NODE_VIRT_MASTER_INPUT_VOLUME6:
                case NODE_VIRT_MASTER_INPUT_VOLUME7:
                case NODE_VIRT_MASTER_INPUT_VOLUME8:
                case NODE_MICIN_VOLUME:
                    *Level = lMinimum + uStep * wRegister;
                    break;
                default:
                    *Level = lMaximum - uStep * wRegister;
                    break;
            }

            // For the virtual controls, which are in front of a muxer, there
            // is no mute control displayed. But we have a HW mute control, so
            // what we do is enabling this mute when the user moves the slider
            // down to the bottom and disabling it on every other position.
            // The system will send us a PROP_MOST_NEGATIVE value when the slider
            // is moved to the bottom. To be gentle, we return it when the mute
            // is set.
            if (((NodeDef >= NODE_VIRT_MASTER_INPUT_VOLUME1) &&
                 (NodeDef <= NODE_VIRT_MASTER_INPUT_VOLUME8)) ||
                 (NodeDef == NODE_VIRT_MONOOUT_VOLUME1) ||
                 (NodeDef == NODE_VIRT_MONOOUT_VOLUME2))
            {
                // read the register again.
                ntStatus = that->AdapterCommon->ReadCodecRegister (
                           that->AdapterCommon->GetNodeReg (NodeDef), &wRegister);
                if (!NT_SUCCESS (ntStatus))
                    return ntStatus;
                // return most negative value in case it is checked.
                if (wRegister & AC97REG_MASK_MUTE)
                    *Level = PROP_MOST_NEGATIVE;
            }

            // when we have cache information then return this instead
            // of the calculated value. if we don't, store the calculated
            // value.
            if (channel == CHAN_LEFT)
            {
                if (that->stNodeCache[NodeDef].bLeftValid)
                    *Level = that->stNodeCache[NodeDef].lLeft;
                else
                {
                    that->stNodeCache[NodeDef].lLeft = *Level;
                    that->stNodeCache[NodeDef].bLeftValid = -1;
                }
            }
            else    // here goes mono or stereo-right
            {
                if (that->stNodeCache[NodeDef].bRightValid)
                    *Level = that->stNodeCache[NodeDef].lRight;
                else
                {
                    that->stNodeCache[NodeDef].lRight = *Level;
                    that->stNodeCache[NodeDef].bRightValid = -1;
                }
            }

            // thats all, good bye.
            PropertyRequest->ValueSize = sizeof(LONG);
            DOUT (DBG_PROPERTY, ("GET: %s(%s) = 0x%x",NodeStrings[NodeDef],
                    channel==CHAN_LEFT ? "L" : "R", *Level));
            // ntStatus was set with the read call! whatever this is, return it.
        }
        else        // this must be a set
        {
            WORD    wRegister;
            LONG    lLevel = *Level;

            // calculate the dB.dB value.
            // check borders.
            if (lLevel > lMaximum) lLevel = lMaximum;
            if (lLevel < lMinimum) lLevel = lMinimum;

            // The nodes are calculated differently.
            switch (NodeDef)
            {
                // for record controls we calculate it 'reverse'.
                case NODE_VIRT_MASTER_INPUT_VOLUME1:
                case NODE_VIRT_MASTER_INPUT_VOLUME2:
                case NODE_VIRT_MASTER_INPUT_VOLUME3:
                case NODE_VIRT_MASTER_INPUT_VOLUME4:
                case NODE_VIRT_MASTER_INPUT_VOLUME5:
                case NODE_VIRT_MASTER_INPUT_VOLUME6:
                case NODE_VIRT_MASTER_INPUT_VOLUME7:
                case NODE_VIRT_MASTER_INPUT_VOLUME8:
                    // read the wavein selector.
                    ntStatus = that->AdapterCommon->ReadCodecRegister (
                            that->AdapterCommon->GetNodeReg (NODE_WAVEIN_SELECT),
                            &wRegister);
                    if (!NT_SUCCESS (ntStatus))
                        return ntStatus;

                    // mask out every unused bit.
                    wRegister &= (that->AdapterCommon->GetNodeMask (
                            NODE_WAVEIN_SELECT) & AC97REG_MASK_RIGHT);

                    // check if the volume that we change belongs to the active
                    // (selected) virtual channel.
                    // Tricky: If the virtual nodes are not defined continuesly
                    // this comparision will fail.
                    if ((NodeDef - NODE_VIRT_MASTER_INPUT_VOLUME1) != wRegister)
                    {
                        // Nope, its not. Just store the level and return.
                        if ((channel == CHAN_LEFT) || (channel == CHAN_MASTER))
                        {
                            // write the value to the node cache.
                            that->stNodeCache[NodeDef].lLeft = *Level;
                            that->stNodeCache[NodeDef].bLeftValid = -1;
                        }
                        if ((channel == CHAN_RIGHT) || (channel == CHAN_MASTER))
                        {
                            // write the value to the node cache.
                            that->stNodeCache[NodeDef].lRight = *Level;
                            that->stNodeCache[NodeDef].bRightValid = -1;
                        }
                        return ntStatus;
                    }
                    // fall through for calculation.

                case NODE_MICIN_VOLUME:
                    wRegister = (WORD)(((lLevel + uStep / 2) - lMinimum) / uStep);
                    break;

                case NODE_VIRT_MONOOUT_VOLUME1:
                case NODE_VIRT_MONOOUT_VOLUME2:
                    // read the monoout selector.
                    ntStatus = that->AdapterCommon->ReadCodecRegister (
                            that->AdapterCommon->GetNodeReg (NODE_MONOOUT_SELECT),
                            &wRegister);
                    if (!NT_SUCCESS (ntStatus))
                        return ntStatus;

                    // mask out every unused bit.
                    wRegister &= that->AdapterCommon->GetNodeMask (NODE_MONOOUT_SELECT);

                    // check if the volume that we change belongs to the active
                    // (selected) virtual channel.
                    // Note: Monout select is set if we want to prg. MIC (Volume2).
                    if ((!wRegister && (NodeDef == NODE_VIRT_MONOOUT_VOLUME2)) ||
                        (wRegister && (NodeDef == NODE_VIRT_MONOOUT_VOLUME1)))
                    {
                        // Nope, its not. Just store the level and return.
                        // These are mono controls, so just store the into right.
                        that->stNodeCache[NodeDef].lRight = *Level;
                        that->stNodeCache[NodeDef].bRightValid = -1;
                        return ntStatus;
                    }
                    // fall through for calculation.

                default:
                    wRegister = (WORD)(((lMaximum + uStep / 2) - lLevel) / uStep);
                    break;
            }

            // Oops - NODE_PCBEEP_VOLUME doesn't use bit0. We have to adjust.
            if (NodeDef == NODE_PCBEEP_VOLUME)
                wRegister <<= 1;

            // write the stuff (with mask!).
            // Note: mono channels are 'master' here (see fake above).
            // this makes sure that left and right channel is prg. for the virt.
            // controls. On controls that only have the right channel, the left
            // channel programming does nothing cause the mask will be zero.
            if ((channel == CHAN_LEFT) || (channel == CHAN_MASTER))
            {
                // write only left.
                ntStatus = that->AdapterCommon->WriteCodecRegister (
                    that->AdapterCommon->GetNodeReg (NodeDef),
                    wRegister << 8,
                    that->AdapterCommon->GetNodeMask (NodeDef) & AC97REG_MASK_LEFT);
                // immediately return on error
                if (!NT_SUCCESS (ntStatus))
                    return ntStatus;
                // write the value to the node cache.
                that->stNodeCache[NodeDef].lLeft = *Level;
                that->stNodeCache[NodeDef].bLeftValid = -1;
            }

            if ((channel == CHAN_RIGHT) || (channel == CHAN_MASTER))
            {
                // write only right.
                ntStatus = that->AdapterCommon->WriteCodecRegister (
                    that->AdapterCommon->GetNodeReg (NodeDef),
                    wRegister,
                    that->AdapterCommon->GetNodeMask (NodeDef) & AC97REG_MASK_RIGHT);
                // immediately return on error
                if (!NT_SUCCESS (ntStatus))
                    return ntStatus;
                // write the value to the node cache.
                that->stNodeCache[NodeDef].lRight = *Level;
                that->stNodeCache[NodeDef].bRightValid = -1;
            }

            // For the virtual controls, which are in front of a muxer, there
            // is no mute control displayed. But we have a HW mute control, so
            // what we do is enabling this mute when the user moves the slider
            // down to the bottom and disabling it on every other position.
            // The system will send us a PROP_MOST_NEGATIVE value when the slider
            // is moved to the bottom. To be gentle, we return it when the mute
            // is set.
            // Tricky: Master input virtual controls must be defined continuesly.
            if (((NodeDef >= NODE_VIRT_MASTER_INPUT_VOLUME1) &&
                 (NodeDef <= NODE_VIRT_MASTER_INPUT_VOLUME8)) ||
                 (NodeDef == NODE_VIRT_MONOOUT_VOLUME1) ||
                 (NodeDef == NODE_VIRT_MONOOUT_VOLUME2))
            {
                // Set or clear the mute!
                switch (NodeDef)
                {
                    // For the mono controls we can just set or clear it.
                    case NODE_VIRT_MONOOUT_VOLUME1:
                    case NODE_VIRT_MONOOUT_VOLUME2:
                    case NODE_VIRT_MASTER_INPUT_VOLUME1:
                    case NODE_VIRT_MASTER_INPUT_VOLUME7:
                    case NODE_VIRT_MASTER_INPUT_VOLUME8:
                        // Check for most negative value
                        if (*Level == PROP_MOST_NEGATIVE)
                        {
                            // set the mute
                            ntStatus = that->AdapterCommon->WriteCodecRegister (
                                that->AdapterCommon->GetNodeReg (NodeDef),
                                AC97REG_MASK_MUTE, AC97REG_MASK_MUTE);
                        }
                        else
                        {
                            // clear the mute
                            ntStatus = that->AdapterCommon->WriteCodecRegister (
                                that->AdapterCommon->GetNodeReg (NodeDef),
                                0, AC97REG_MASK_MUTE);
                        }
                        break;

                    default:
                        // we have stereo channels and first want to check the
                        // other channel
                        if ((that->stNodeCache[NodeDef].bLeftValid &&
                            (that->stNodeCache[NodeDef].lLeft == PROP_MOST_NEGATIVE)) &&
                            (that->stNodeCache[NodeDef].bRightValid &&
                            (that->stNodeCache[NodeDef].lRight == PROP_MOST_NEGATIVE)))
                        {
                            // set the mute
                            ntStatus = that->AdapterCommon->WriteCodecRegister (
                                that->AdapterCommon->GetNodeReg (NodeDef),
                                AC97REG_MASK_MUTE, AC97REG_MASK_MUTE);
                        }
                        else
                        {
                            // clear the mute
                            ntStatus = that->AdapterCommon->WriteCodecRegister (
                                that->AdapterCommon->GetNodeReg (NodeDef),
                                0, AC97REG_MASK_MUTE);
                        }
                }
            }
            
            DOUT (DBG_PROPERTY, ("SET: %s(%s) -> 0x%x", NodeStrings[NodeDef],
                    channel==CHAN_LEFT ? "L" : channel==CHAN_RIGHT ? "R" : "M",
                    *Level));
            // ntStatus was set with the read call! whatever this is, return it.
        }
    }
    else
    {
        if (PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
        {
            ntStatus = BasicSupportHandler (PropertyRequest);
        }
    }

    return ntStatus;
}

/*****************************************************************************
 * CMiniportTopologyICH::PropertyHandler_Tone
 *****************************************************************************
 * Accesses a KSAUDIO_TONE property.
 * This function (property handler) is called by portcls every time there is a
 * get, set or basic support request for the node. The connection between the
 * node type and the property handler is made in the automation table which is
 * referenced when you register the node.
 * We use this property handler for all tone controls displayed at the advanced
 * property dialog in sndvol32 and the 3D controls displayed and exposed as
 * normal volume controls.
 */
NTSTATUS CMiniportTopologyICH::PropertyHandler_Tone
(
    IN      PPCPROPERTY_REQUEST   PropertyRequest
)
{
    PAGED_CODE ();

    ASSERT (PropertyRequest);

    DOUT (DBG_PRINT, ("[CMiniportTopologyICH::PropertyHandler_Tone]"));

    NTSTATUS        ntStatus = STATUS_INVALID_PARAMETER;
    TopoNodes       NodeDef;
    LONG            lMinimum, lMaximum;
    ULONG           uStep;
     // The major target is the object pointer to the topology miniport.
   CMiniportTopologyICH *that =
        (CMiniportTopologyICH *) PropertyRequest->MajorTarget;

    ASSERT (that);

    // validate node
    if (PropertyRequest->Node == (ULONG)-1)
        return ntStatus;

    // do the appropriate action for the request.

    // we should do a get or a set?
    if ((PropertyRequest->Verb & KSPROPERTY_TYPE_GET) ||
        (PropertyRequest->Verb & KSPROPERTY_TYPE_SET))
    {
        // validate parameters
        if ((PropertyRequest->InstanceSize < sizeof(LONG)) ||
            (PropertyRequest->ValueSize < sizeof(LONG)))
            return ntStatus;

        // get the buffer
        PLONG Level = (PLONG)PropertyRequest->Value;

        // Switch on the node id. This is just for parameter checking.
        // If something goes wrong, we will immideately return with
        // ntStatus, which is STATUS_INVALID_PARAMETER.
        switch(NodeDef = that->TransNodeNrToNodeDef (PropertyRequest->Node))
        {
            case NODE_BASS:
                // check type.
                if (PropertyRequest->PropertyItem->Id != KSPROPERTY_AUDIO_BASS)
                    return ntStatus;
                break;

            case NODE_TREBLE:
                // check type.
                if (PropertyRequest->PropertyItem->Id != KSPROPERTY_AUDIO_TREBLE)
                    return ntStatus;
                break;

            case NODE_VIRT_3D_CENTER:
            case NODE_VIRT_3D_DEPTH:
                // check 3D control
                if (!that->AdapterCommon->GetNodeConfig (NODEC_3D_CENTER_ADJUSTABLE)
                    && (NodeDef == NODE_VIRT_3D_CENTER))
                    return ntStatus;
                if (!that->AdapterCommon->GetNodeConfig (NODEC_3D_DEPTH_ADJUSTABLE)
                    && (NodeDef == NODE_VIRT_3D_DEPTH))
                    return ntStatus;
                // check type
                if (PropertyRequest->PropertyItem->Id != KSPROPERTY_AUDIO_VOLUMELEVEL)
                    return ntStatus;
                // check channel
                if (*(PLONG(PropertyRequest->Instance)) == CHAN_RIGHT)
                    return ntStatus;
                break;
            
            case NODE_INVALID:
            default:
                // Ooops
                DOUT (DBG_ERROR, ("PropertyHandler_Tone: Invalid node requested."));
                return ntStatus;
        }

        // Now, do some action!

        // get the registered DB values
        ntStatus = GetDBValues (that->AdapterCommon, NodeDef, &lMinimum,
                                &lMaximum, &uStep);
        if (!NT_SUCCESS (ntStatus))
            return ntStatus;

        // do a get
        if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
        {
            WORD    wRegister;

            // first get the stuff.
            ntStatus = that->AdapterCommon->ReadCodecRegister (
                    that->AdapterCommon->GetNodeReg (NodeDef), &wRegister);
            if (!NT_SUCCESS (ntStatus))
                return ntStatus;

            // mask out every unused bit.
            wRegister &= that->AdapterCommon->GetNodeMask (NodeDef);

            // rotate if bass tone control or 3D center control
            if ((NodeDef == NODE_BASS) || (NodeDef == NODE_VIRT_3D_CENTER))
                wRegister >>= 8;

            // convert from reg to dB.dB value.
            if ((NodeDef == NODE_VIRT_3D_CENTER) ||
                (NodeDef == NODE_VIRT_3D_DEPTH))
            {
                // That's for the 3D controls
                *Level = lMinimum + uStep * wRegister;
            }
            else
            {
                if (wRegister == 0x000F)
                    *Level = 0;     // bypass
                else
                    // And that's for the tone controls
                    *Level = lMaximum - uStep * wRegister;
            }
            
            // when we have cache information then return this instead
            // of the calculated value. if we don't, store the calculated
            // value.
            if (that->stNodeCache[NodeDef].bLeftValid)
                *Level = that->stNodeCache[NodeDef].lLeft;
            else
            {
                that->stNodeCache[NodeDef].lLeft = *Level;
                that->stNodeCache[NodeDef].bLeftValid = -1;
            }

            // we return a LONG
            PropertyRequest->ValueSize = sizeof(LONG);
            DOUT (DBG_PROPERTY, ("GET: %s = 0x%x", NodeStrings[NodeDef], *Level));
            // ntStatus was set with the read call! whatever this is, return it.
        }
        else        // that must be a set
        {
            WORD    wRegister;
            LONG    lLevel = *Level;

            // calculate the dB.dB value.
            // check borders.
            if (lLevel > lMaximum) lLevel = lMaximum;
            if (lLevel < lMinimum) lLevel = lMinimum;

            // convert from dB.dB value to reg.
            if ((NodeDef == NODE_VIRT_3D_CENTER) ||
                (NodeDef == NODE_VIRT_3D_DEPTH))
            {
                // For 3D controls
                wRegister = (WORD)(((lLevel + uStep / 2) - lMinimum) / uStep);
            }
            else
            {
                // For tone controls
                wRegister = (WORD)(((lMaximum + uStep / 2) - lLevel) / uStep);
                // We don't prg. 0dB Bass or 0dB Treble, instead we smartly prg.
                // a bypass which is reg. value 0x0F.
                if (wRegister == 7)             // 0 dB
                    wRegister = 0x000F;         // bypass
            }

            // rotate if bass tone control or 3D center control
            if ((NodeDef == NODE_BASS) || (NodeDef == NODE_VIRT_3D_CENTER))
                wRegister <<= 8;

            // write the stuff.
            ntStatus = that->AdapterCommon->WriteCodecRegister (
                    that->AdapterCommon->GetNodeReg (NodeDef),
                    wRegister,
                    that->AdapterCommon->GetNodeMask (NodeDef));

            // write the value to the node cache.
            that->stNodeCache[NodeDef].lLeft = *Level;
            that->stNodeCache[NodeDef].bLeftValid = -1;
            DOUT (DBG_PROPERTY,("SET: %s -> 0x%x", NodeStrings[NodeDef], *Level));
            // ntStatus was set with the write call! whatever this is, return in.
        }
    }
    else
    {
        if (PropertyRequest->Verb & KSPROPERTY_TYPE_BASICSUPPORT)
        {
            ntStatus = BasicSupportHandler (PropertyRequest);
        }
    }

    return ntStatus;
}
            
/*****************************************************************************
 * CMiniportTopologyICH::PropertyHandler_Ulong
 *****************************************************************************
 * Accesses a ULONG value property. For MUX and DEMUX.
 * This function (property handler) is called by portcls every time there is a
 * get, set or basic support request for the node. The connection between the
 * node type and the property handler is made in the automation table which is
 * referenced when you register the node.
 * We use this property handler for all muxer controls.
 */
NTSTATUS CMiniportTopologyICH::PropertyHandler_Ulong
(
    IN      PPCPROPERTY_REQUEST   PropertyRequest
)
{
    PAGED_CODE ();

    ASSERT (PropertyRequest);

    DOUT (DBG_PRINT, ("[CMiniportTopologyICH::PropertyHandler_Ulong]"));

    NTSTATUS        ntStatus = STATUS_INVALID_PARAMETER;
    TopoNodes       NodeDef;
    LONG            lMinimum, lMaximum;
    ULONG           uStep;
    // The major target is the object pointer to the topology miniport.
    CMiniportTopologyICH *that =
        (CMiniportTopologyICH *) PropertyRequest->MajorTarget;

    ASSERT (that);


    // validate node instance
    if (PropertyRequest->Node == (ULONG)-1)
        return ntStatus;

    // if we should do a get or set.
    if ((PropertyRequest->Verb & KSPROPERTY_TYPE_GET) ||
        (PropertyRequest->Verb & KSPROPERTY_TYPE_SET))
    {
        // validate buffer size.
        if (PropertyRequest->ValueSize < sizeof(ULONG))
            return ntStatus;

        // get the pointer to the buffer.
        PLONG PropValue = (PLONG)PropertyRequest->Value;

        // Switch on the node id. This is just for parameter checking.
        // If something goes wrong, we will immideately return with
        // ntStatus, which is STATUS_INVALID_PARAMETER.
        switch(NodeDef = that->TransNodeNrToNodeDef (PropertyRequest->Node))
        {
            case NODE_MONOOUT_SELECT:
            case NODE_WAVEIN_SELECT:
                // check the type
                if (PropertyRequest->PropertyItem->Id != KSPROPERTY_AUDIO_MUX_SOURCE)
                    return ntStatus;
                break;
                
            case NODE_INVALID:
            default:
                // Ooops
                DOUT (DBG_ERROR, ("PropertyHandler_Tone: Invalid node requested."));
                return ntStatus;
        }

        // Now do some action!

        // should we return the value?
        if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
        {
            WORD    wRegister;

            // first get the stuff.
            ntStatus = that->AdapterCommon->ReadCodecRegister (
                    that->AdapterCommon->GetNodeReg (NodeDef), &wRegister);
            if (!NT_SUCCESS (ntStatus))
                return ntStatus;

            // mask out every unused bit.
            wRegister &= that->AdapterCommon->GetNodeMask (NodeDef);

            // calculate the selected pin
            if (NodeDef == NODE_MONOOUT_SELECT)
            {
                // for mono out we have just one bit
                if (wRegister)
                    *PropValue = 2;
                else
                    *PropValue = 1;
            }
            else
            {
                // the wave in muxer is a stereo muxer, so just return the
                // right channel (gives values 0-7) and adjust it by adding 1.
                *PropValue = (wRegister & AC97REG_MASK_RIGHT) + 1;
            }

            // we return a LONG
            PropertyRequest->ValueSize = sizeof(LONG);
            DOUT (DBG_PROPERTY, ("GET: %s = 0x%x", NodeStrings[NodeDef],
                    *PropValue));
            // ntStatus was set with the read call! whatever this is, return it.
        }
        else        // that must be a set
        {
            TopoNodes   VirtNode;
            WORD        wRegister;
            LONG        lLevel = *PropValue;

            // Check the selection first.
            if (NodeDef == NODE_MONOOUT_SELECT)
            {
                if ((lLevel < 1) || (lLevel > 2))
                    return ntStatus;    // STATUS_INVALID_PARAMETER
            }
            else
            {
                if ((lLevel < 1) || (lLevel > 8))
                    return ntStatus;    // STATUS_INVALID_PARAMETER
            }

            // calculate the register value for programming.
            if (NodeDef == NODE_MONOOUT_SELECT)
            {
                // for mono out we have just one bit
                if (lLevel == 2)
                    // the mask will make sure we only prg. one bit.
                    wRegister = -1;
                else
                    // lLevel == 1
                    wRegister = 0;
            }
            else
            {
                // *257 is the same as: (lLevel << 8) + lLevel
                wRegister = (WORD)(lLevel - 1) * 257;
            }

            // write the stuff.
            ntStatus = that->AdapterCommon->WriteCodecRegister (
                    that->AdapterCommon->GetNodeReg (NodeDef),
                    wRegister,
                    that->AdapterCommon->GetNodeMask (NodeDef));

            // Store the virt. node for later use.
            // Tricky: Master input virtual controls must be defined continuesly.
            if (NodeDef == NODE_MONOOUT_SELECT)
                VirtNode = (TopoNodes)(NODE_VIRT_MONOOUT_VOLUME1 + (lLevel - 1));
            else
                VirtNode = (TopoNodes)(NODE_VIRT_MASTER_INPUT_VOLUME1 + (lLevel - 1));

            // Virtual controls make our life more complicated. When the user
            // changes the input source say from CD to LiniIn, then the system just
            // sends a message to the input muxer that the selection changed.
            // Cause we have only one HW register for the input muxer, all volumes
            // displayed for the user are "virtualized", means they are not there,
            // and when the selection changes, we have to prg. the volume of the
            // selected input to the HW register. That's what we do now.
            
            // get the registered DB values
            ntStatus = GetDBValues (that->AdapterCommon, VirtNode,
                                    &lMinimum, &lMaximum, &uStep);
            if (!NT_SUCCESS (ntStatus))
                return ntStatus;

            // we can override lLevel now with the volume control value.

            // We can be silly here and don't check for mono controls. Reason
            // is that the level handler writes the volume value for mono
            // controls into both the left and right node cache ;))
            
            if (that->stNodeCache[VirtNode].bLeftValid &&
                that->stNodeCache[VirtNode].bRightValid)
            {
                // prg. left channel
                lLevel = that->stNodeCache[VirtNode].lLeft;

                // check borders.
                if (lLevel > lMaximum) lLevel = lMaximum;
                if (lLevel < lMinimum) lLevel = lMinimum;

                // calculate the dB.dB value.
                if (NodeDef == NODE_MONOOUT_SELECT)
                    wRegister = (WORD)(((lMaximum + uStep / 2) - lLevel) / uStep);
                else
                    wRegister = (WORD)(((lLevel + uStep / 2) - lMinimum) / uStep);

                // write left channel.
                ntStatus = that->AdapterCommon->WriteCodecRegister (
                    that->AdapterCommon->GetNodeReg (VirtNode),
                    wRegister << 8,
                    that->AdapterCommon->GetNodeMask (VirtNode) & AC97REG_MASK_LEFT);

                // prg. right channel
                lLevel = that->stNodeCache[VirtNode].lRight;

                // check borders.
                if (lLevel > lMaximum) lLevel = lMaximum;
                if (lLevel < lMinimum) lLevel = lMinimum;

                // calculate the dB.dB value.
                if (NodeDef == NODE_MONOOUT_SELECT)
                    wRegister = (WORD)(((lMaximum + uStep / 2) - lLevel) / uStep);
                else
                    wRegister = (WORD)(((lLevel + uStep / 2) - lMinimum) / uStep);

                // write left channel.
                ntStatus = that->AdapterCommon->WriteCodecRegister (
                    that->AdapterCommon->GetNodeReg (VirtNode),
                    wRegister,
                    that->AdapterCommon->GetNodeMask (VirtNode) & AC97REG_MASK_RIGHT);
                
                // For the virtual controls, which are in front of a muxer, there
                // is no mute control displayed. But we have a HW mute control, so
                // what we do is enabling this mute when the user moves the slider
                // down to the bottom and disabling it on every other position.
                // The system will send us a PROP_MOST_NEGATIVE value when the slider
                // is moved to the bottom.
                if ((that->stNodeCache[VirtNode].lLeft == PROP_MOST_NEGATIVE) &&
                    (that->stNodeCache[VirtNode].lRight == PROP_MOST_NEGATIVE))
                {
                    // set the mute
                    ntStatus = that->AdapterCommon->WriteCodecRegister (
                        that->AdapterCommon->GetNodeReg (VirtNode),
                        AC97REG_MASK_MUTE, AC97REG_MASK_MUTE);
                }
                else
                {
                    // clear the mute
                    ntStatus = that->AdapterCommon->WriteCodecRegister (
                        that->AdapterCommon->GetNodeReg (VirtNode),
                        0, AC97REG_MASK_MUTE);
                }
            }                
            DOUT (DBG_PROPERTY, ("SET: %s -> 0x%x", NodeStrings[NodeDef],
                    *PropValue));
            // ntStatus was set with the write call! whatever this is, return it.
        }
    }

    return ntStatus;
}

/*****************************************************************************
 * CMiniportTopologyICH::PropertyHandler_CpuResources
 *****************************************************************************
 * Propcesses a KSPROPERTY_AUDIO_CPU_RESOURCES request
 * This property handler is called by the system for every node and every node
 * must support this property. Basically, this property is for performance
 * monitoring and we just say here that every function we claim to have has HW
 * support (which by the way is true).
 */
NTSTATUS CMiniportTopologyICH::PropertyHandler_CpuResources
(
    IN      PPCPROPERTY_REQUEST   PropertyRequest
)
{
    PAGED_CODE ();

    ASSERT (PropertyRequest);

    DOUT (DBG_PRINT, ("[CMiniportTopologyICH::PropertyHandler_CpuResources]"));

    CMiniportTopologyICH *that =
        (CMiniportTopologyICH *) PropertyRequest->MajorTarget;
    NTSTATUS ntStatus = STATUS_INVALID_DEVICE_REQUEST;

    ASSERT (that);

    // validate node
    if (PropertyRequest->Node == (ULONG)-1)
        return ntStatus;

    // validate the node def.
    if (that->TransNodeNrToNodeDef (PropertyRequest->Node) == NODE_INVALID)
        return ntStatus;
    
    // we should do a get
    if (PropertyRequest->Verb & KSPROPERTY_TYPE_GET)
    {
        // just return the flag.
        if (PropertyRequest->ValueSize >= sizeof(LONG))
        {
            *((PLONG)PropertyRequest->Value) = KSAUDIO_CPU_RESOURCES_NOT_HOST_CPU;
            PropertyRequest->ValueSize = sizeof(LONG);
            ntStatus = STATUS_SUCCESS;
        }
        else    // not enough buffer.
        {
            ntStatus = STATUS_BUFFER_TOO_SMALL;
        }
    }

    return ntStatus;
}
