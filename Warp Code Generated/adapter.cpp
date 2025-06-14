/*****************************************************************************
 * SendVerb Implementation
 */

NTSTATUS
CHdaCodec::SendVerbInternal(
    ULONG Codec,
    ULONG Node,
    ULONG Verb,
    ULONG Command,
    PULONG Response
)
{
    // Use CORB/RIRB if initialized, otherwise fall back to PIO
    if (m_bCORBInitialized && m_bRIRBInitialized) {
        return SendVerbCORB(Codec, Node, Verb, Command, Response);
    } else {
        return SendVerbPIO(Codec, Node, Verb, Command, Response);
    }
}

NTSTATUS
CHdaCodec::SendVerbCORB(
    ULONG Codec,
    ULONG Node,
    ULONG Verb,
    ULONG Command,
    PULONG Response
)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG value = ((Codec << 28) | (Node << 20) | (Verb << 8) | (Command));
    
    // Write verb to CORB
    m_DevExt.CorbMem[m_DevExt.CorbPointer] = value;
    
    // Move CORB write pointer
    WriteWord(HDA_REG_CORBWP, m_DevExt.CorbPointer);
    
    // Wait for response
    for (int i = 0; i < 50; i++) {
        if (ReadWord(HDA_REG_RIRBWP) == m_DevExt.CorbPointer) {
            break;
        }
        KeStallExecutionProcessor(10);
    }
    
    // Check if we got a response
    if (ReadWord(HDA_REG_RIRBWP) != m_DevExt.CorbPointer) {
        DPF(D_ERROR, ("HD Audio: no response from codec"));
        m_DevExt.CommunicationType = HDA_UNINITALIZED;
        return STATUS_DEVICE_NOT_READY;
    }
    
    // Read response
    ULONG response = m_DevExt.RirbMem[m_DevExt.RirbPointer * 2];
    
    // Move pointers
    m_DevExt.CorbPointer++;
    if (m_DevExt.CorbPointer == m_DevExt.CorbNumberOfEntries) {
        m_DevExt.CorbPointer = 0;
    }
    
    m_DevExt.RirbPointer++;
    if (m_DevExt.RirbPointer == m_DevExt.RirbNumberOfEntries) {
        m_DevExt.RirbPointer = 0;
    }
    
    // Return response
    if (Response) {
        *Response = response;
    }
    
    return STATUS_SUCCESS;
}

NTSTATUS
CHdaCodec::SendVerbPIO(
    ULONG Codec,
    ULONG Node,
    ULONG Verb,
    ULONG Command,
    PULONG Response
)
{
    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG value = ((Codec << 28) | (Node << 20) | (Verb << 8) | (Command));
    
    // Clear Immediate Result Valid bit
    WriteWord(HDA_REG_ICIS, 0x2);
    
    // Write verb
    WriteDword(HDA_REG_ICOI, value);
    
    // Start verb transfer
    WriteWord(HDA_REG_ICIS, 0x1);
    
    // Poll for response
    for (int i = 0; i < 300; i++) {
        // Wait for Immediate Result Valid bit = set and Immediate Command Busy bit = clear
        if ((ReadWord(HDA_REG_ICIS) & 0x3) == 0x2) {
            // Clear Immediate Result Valid bit
            WriteWord(HDA_REG_ICIS, 0x2);
            
            // Return response
            if (Response) {
                *Response = ReadDword(HDA_REG_ICII);
            }
            
            return STATUS_SUCCESS;
        }
        KeStallExecutionProcessor(10);
    }
    
    // No response after timeout
    DPF(D_ERROR, ("HD Audio: no response from codec via PIO"));
    m_DevExt.CommunicationType = HDA_UNINITALIZED;
    return STATUS_DEVICE_NOT_READY;
}

/*****************************************************************************
 * Codec Initialization
 */

NTSTATUS
CHdaCodec::FindAndInitCodec(
    ULONG SoundCardNumber
)
{
    PAGED_CODE();
    
    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG codecId = 0;
    
    // Set the selected card
    m_DevExt.SelectedHdaCard = SoundCardNumber;
    
    // Try communicating with codecs using CORB/RIRB interface
    m_DevExt.CommunicationType = HDA_CORB_RIRB;
    
    // Try to find a codec (maximum 16 codecs in spec)
    for (ULONG codecNumber = 0; codecNumber < 16; codecNumber++) {
        ntStatus = SendVerbInternal(codecNumber, 0, HDA_VERB_GET_PARAM, HDA_PARAM_VENDOR_ID, &codecId);
        
        if (NT_SUCCESS(ntStatus) && codecId != 0) {
            DPF(D_TERSE, ("HD Audio: CORB/RIRB communication interface"));
            return InitializeCodec(SoundCardNumber, codecNumber);
        }
    }
    
    // If CORB/RIRB failed, try PIO interface
    DPF(D_TERSE, ("HD Audio: CORB/RIRB communication failed, trying PIO"));
    
    // Stop CORB and RIRB
    WriteByte(HDA_REG_CORBCTL, 0);
    WriteByte(HDA_REG_RIRBCTL, 0);
    
    m_DevExt.CommunicationType = HDA_PIO;
    
    // Try to find a codec using PIO
    for (ULONG codecNumber = 0; codecNumber < 16; codecNumber++) {
        ntStatus = SendVerbInternal(codecNumber, 0, HDA_VERB_GET_PARAM, HDA_PARAM_VENDOR_ID, &codecId);
        
        if (NT_SUCCESS(ntStatus) && codecId != 0) {
            DPF(D_TERSE, ("HD Audio: PIO communication interface"));
            return InitializeCodec(SoundCardNumber, codecNumber);
        }
    }
    
    DPF(D_ERROR, ("HD Audio: No codec found"));
    return STATUS_DEVICE_NOT_FOUND;
}

NTSTATUS
CHdaCodec::InitializeCodec(
    ULONG SoundCardNumber,
    ULONG CodecNumber
)
{
    PAGED_CODE();
    
    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG codecId = 0;
    
    // Verify codec exists
    ntStatus = SendVerbInternal(CodecNumber, 0, HDA_VERB_GET_PARAM, HDA_PARAM_VENDOR_ID, &codecId);
    if (!NT_SUCCESS(ntStatus) || codecId == 0) {
        DPF(D_ERROR, ("HD Audio: Codec not responding"));
        return STATUS_DEVICE_NOT_FOUND;
    }
    
    m_DevExt.CodecNumber = CodecNumber;
    
    // Log basic codec info
    DPF(D_TERSE, ("Codec %d", CodecNumber));
    DPF(D_TERSE, ("Vendor: 0x%04x", (codecId >> 16)));
    DPF(D_TERSE, ("Device: 0x%04x", (codecId & 0xFFFF)));
    
    // Find Audio Function Groups
    ULONG nodeCountResponse = 0;
    ntStatus = SendVerbInternal(CodecNumber, 0, HDA_VERB_GET_PARAM, HDA_PARAM_NODE_COUNT, &nodeCountResponse);
    
    if (!NT_SUCCESS(ntStatus)) {
        DPF(D_ERROR, ("HD Audio: Failed to get node count"));
        return ntStatus;
    }
    
    ULONG firstNode = (nodeCountResponse >> 16) & 0xFF;
    ULONG nodeCount = nodeCountResponse & 0xFF;
    
    DPF(D_VERBOSE, ("First Group node: %d Number of Groups: %d", firstNode, nodeCount));
    
    // Look for Audio Function Groups
    for (ULONG node = firstNode; node < (firstNode + nodeCount); node++) {
        ULONG functionType = 0;
        ntStatus = SendVerbInternal(CodecNumber, node, HDA_VERB_GET_PARAM, HDA_PARAM_FUNCTION_TYPE, &functionType);
        
        if (NT_SUCCESS(ntStatus) && (functionType & 0x7F) == 0x01) {
            // This is an Audio Function Group
            return InitializeAudioFunctionGroup(SoundCardNumber, node);
        }
    }
    
    DPF(D_ERROR, ("HD Audio: No Audio Function Group found"));
    return STATUS_DEVICE_NOT_FOUND;
}

NTSTATUS
CHdaCodec::InitializeAudioFunctionGroup(
    ULONG SoundCardNumber,
    ULONG AfgNodeNumber
)
{
    PAGED_CODE();
    
    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG response = 0;
    
    // Reset AFG
    ntStatus = SendVerbInternal(m_DevExt.CodecNumber, AfgNodeNumber, 0x7FF, 0x00, &response);
    if (!NT_SUCCESS(ntStatus)) {
        DPF(D_ERROR, ("Failed to reset Audio Function Group"));
        return ntStatus;
    }
    
    // Enable power for AFG
    ntStatus = SendVerbInternal(m_DevExt.CodecNumber, AfgNodeNumber, HDA_VERB_SET_POWER_STATE, 0x00, &response);
    if (!NT_SUCCESS(ntStatus)) {
        DPF(D_ERROR, ("Failed to power up Audio Function Group"));
        return ntStatus;
    }
    
    // Disable unsolicited responses
    ntStatus = SendVerbInternal(m_DevExt.CodecNumber, AfgNodeNumber, 0x708, 0x00, &response);
    if (!NT_SUCCESS(ntStatus)) {
        DPF(D_ERROR, ("Failed to disable unsolicited responses"));
        return ntStatus;
    }
    
    // Read available info
    ntStatus = SendVerbInternal(m_DevExt.CodecNumber, AfgNodeNumber, HDA_VERB_GET_PARAM, HDA_PARAM_PCM, &m_DevExt.AfgNodeSampleCapabilities);
    ntStatus = SendVerbInternal(m_DevExt.CodecNumber, AfgNodeNumber, HDA_VERB_GET_PARAM, HDA_PARAM_STREAM, &m_DevExt.AfgNodeStreamFormatCapabilities);
    ntStatus = SendVerbInternal(m_DevExt.CodecNumber, AfgNodeNumber, HDA_VERB_GET_PARAM, HDA_PARAM_IN_AMP_CAP, &m_DevExt.AfgNodeInputAmpCapabilities);
    ntStatus = SendVerbInternal(m_DevExt.CodecNumber, AfgNodeNumber, HDA_VERB_GET_PARAM, HDA_PARAM_OUT_AMP_CAP, &m_DevExt.AfgNodeOutputAmpCapabilities);
    
    DPF(D_VERBOSE, ("Audio Function Group node %d", AfgNodeNumber));
    DPF(D_VERBOSE, ("AFG sample capabilities: 0x%x", m_DevExt.AfgNodeSampleCapabilities));
    DPF(D_VERBOSE, ("AFG stream format capabilities: 0x%x", m_DevExt.AfgNodeStreamFormatCapabilities));
    DPF(D_VERBOSE, ("AFG input amp capabilities: 0x%x", m_DevExt.AfgNodeInputAmpCapabilities));
    DPF(D_VERBOSE, ("AFG output amp capabilities: 0x%x", m_DevExt.AfgNodeOutputAmpCapabilities));
    
    // Enumerate nodes to find output pins
    ULONG nodeCountResponse = 0;
    ntStatus = SendVerbInternal(m_DevExt.CodecNumber, AfgNodeNumber, HDA_VERB_GET_PARAM, HDA_PARAM_NODE_COUNT, &nodeCountResponse);
    
    if (!NT_SUCCESS(ntStatus)) {
        DPF(D_ERROR, ("Failed to get node count for AFG"));
        return ntStatus;
    }
    
    ULONG firstNode = (nodeCountResponse >> 16) & 0xFF;
    ULONG nodeCount = nodeCountResponse & 0xFF;
    
    // Set up variables to track available output pins
    ULONG pinSpeakerDefault = 0;
    ULONG pinSpeaker = 0;
    ULONG pinHeadphone = 0;
    ULONG pinAlternative = 0;
    
    // Enumerate all nodes to find pins
    for (ULONG node = firstNode; node < (firstNode + nodeCount); node++) {
        ULONG nodeType = 0;
        
        // Get type of node
        ntStatus = SendVerbInternal(m_DevExt.CodecNumber, node, HDA_VERB_GET_PARAM, HDA_PARAM_AUDIO_WIDGET_CAP, &nodeType);
        
        if (!NT_SUCCESS(ntStatus)) {
            continue;
        }
        
        // Extract widget type from bits 20-23
        UCHAR widgetType = (UCHAR)((nodeType >> 20) & 0xF);
        
        DPF(D_VERBOSE, ("Node %d Type %d", node, widgetType));
        
        // Check for pin widgets (type 4)
        if (widgetType == HDA_WIDGET_PIN_COMPLEX) {
            // Get pin configuration
            ULONG pinConfig = 0;
            ntStatus = SendVerbInternal(m_DevExt.CodecNumber, node, HDA_VERB_GET_CONFIG_DEFAULT, 0x00, &pinConfig);
            
            if (!NT_SUCCESS(ntStatus)) {
                continue;
            }
            
            // Extract device type from bits 20-24
            UCHAR pinType = (UCHAR)((pinConfig >> 20) & 0xF);
            
            // Check for output pins
            switch (pinType) {
                case HDA_PIN_SPEAKER:
                    DPF(D_VERBOSE, ("Node %d is Speaker", node));
                    // First speaker node becomes default
                    if (pinSpeakerDefault == 0) {
                        pinSpeakerDefault = node;
                    }
                    
                    // Check if device is connected
                    if ((nodeType & 0x4) == 0x4) {
                        // Check if device is output capable
                        ULONG deviceCaps = 0;
                        ntStatus = SendVerbInternal(m_DevExt.CodecNumber, node, HDA_VERB_GET_PARAM, 0x0C, &deviceCaps);
                        
                        if (NT_SUCCESS(ntStatus) && (deviceCaps & 0x10) == 0x10) {
                            // Found connected output device
                            if (pinSpeaker == 0) {
                                pinSpeaker = node;
                            }
                        }
                    }
                    break;
                    
                case HDA_PIN_HEADPHONE_OUT:
                    DPF(D_VERBOSE, ("Node %d is Headphone", node));
                    pinHeadphone = node;
                    break;
                    
                case HDA_PIN_LINE_OUT:
                case HDA_PIN_SPDIF_OUT:
                case HDA_PIN_DIGITAL_OTHER_OUT:
                    DPF(D_VERBOSE, ("Node %d is Line Out/Digital Out", node));
                    pinAlternative = node;
                    break;
            }
        }
    }
    
    // Initialize output pins
    m_DevExt.is_initalized_useful_output = 0;
    
    // Reset variables
    m_DevExt.second_audio_output_node_number = 0;
    m_DevExt.second_audio_output_node_sample_capabilities = 0;
    m_DevExt.second_audio_output_node_stream_format_capabilities = 0;
    m_DevExt.second_output_amp_node_number = 0;
    m_DevExt.second_output_amp_node_capabilities = 0;
    
    // Try to initialize speaker output first
    if (pinSpeakerDefault != 0) {
        DPF(D_TERSE, ("Initializing speaker output"));
        m_DevExt.is_initalized_useful_output = 1;
        
        if (pinSpeaker != 0) {
            ntStatus = InitializeOutputPin(SoundCardNumber, pinSpeaker);
            if (NT_SUCCESS(ntStatus)) {
                m_DevExt.pin_output_node_number = pinSpeaker;
            }
        } else {
            ntStatus = InitializeOutputPin(SoundCardNumber, pinSpeakerDefault);
            if (NT_SUCCESS(ntStatus)) {
                m_DevExt.pin_output_node_number = pinSpeakerDefault;
            }
        }
        
        // Save speaker path
        m_DevExt.second_audio_output_node_number = m_DevExt.audio_output_node_number;
        m_DevExt.second_audio_output_node_sample_capabilities = m_DevExt.audio_output_node_sample_capabilities;
        m_DevExt.second_audio_output_node_stream_format_capabilities = m_DevExt.audio_output_node_stream_format_capabilities;
        m_DevExt.second_output_amp_node_number = m_DevExt.output_amp_node_number;
        m_DevExt.second_output_amp_node_capabilities = m_DevExt.output_amp_node_capabilities;
        
        // If codec has headphone output, initialize it
        if (pinHeadphone != 0) {
            DPF(D_TERSE, ("Initializing headphone output"));
            ntStatus = InitializeOutputPin(SoundCardNumber, pinHeadphone);
            if (NT_SUCCESS(ntStatus)) {
                m_DevExt.pin_headphone_node_number = pinHeadphone;
            }
            
            // Check if both paths share nodes
            if (m_DevExt.audio_output_node_number == m_DevExt.second_audio_output_node_number) {
                m_DevExt.second_audio_output_node_number = 0;
            }
            
            if (m_DevExt.output_amp_node_number == m_DevExt.second_output_amp_node_number) {
                m_DevExt.second_output_amp_node_number = 0;
            }
            
            // Check headphone connection
            if (IsHeadphoneConnected()) {
                ntStatus = DisablePinOutput(m_DevExt.CodecNumber, m_DevExt.pin_output_node_number);
                m_DevExt.selected_output_node = m_DevExt.pin_headphone_node_number;
            } else {
                m_DevExt.selected_output_node = m_DevExt.pin_output_node_number;
            }
        }
    } else if (pinHeadphone != 0) {
        // Only headphone output available
        DPF(D_TERSE, ("Initializing headphone-only output"));
        m_DevExt.is_initalized_useful_output = 1;
        ntStatus = InitializeOutputPin(SoundCardNumber, pinHeadphone);
        if (NT_SUCCESS(ntStatus)) {
            m_DevExt.pin_output_node_number = pinHeadphone;
        }
    } else if (pinAlternative != 0) {
        // Only alternative output available
        DPF(D_TERSE, ("Initializing alternative output"));
        m_DevExt.is_initalized_useful_output = 0;
        ntStatus = InitializeOutputPin(SoundCardNumber, pinAlternative);
        if (NT_SUCCESS(ntStatus)) {
            m_DevExt.pin_output_node_number = pinAlternative;
        }
    } else {
        DPF(D_ERROR, ("HD Audio: No output pins found"));
        return STATUS_DEVICE_NOT_FOUND;
    }
    
    return STATUS_SUCCESS;
}

NTSTATUS
CHdaCodec::InitializeOutputPin(
    ULONG SoundCardNumber,
    ULONG PinNodeNumber
)
{
    PAGED_CODE();
    
    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG response = 0;
    
    DPF(D_VERBOSE, ("Initializing PIN %d", PinNodeNumber));
    
    // Reset variables
    m_DevExt.audio_output_node_number = 0;
    m_DevExt.audio_output_node_sample_capabilities = 0;
    m_DevExt.audio_output_node_stream_format_capabilities = 0;
    m_DevExt.output_amp_node_number = 0;
    m_DevExt.output_amp_node_capabilities = 0;
    
    // Turn on power for PIN
    ntStatus = SendVerbInternal(m_DevExt.CodecNumber, PinNodeNumber, HDA_VERB_SET_POWER_STATE, 0x00, &response);
    if (!NT_SUCCESS(ntStatus)) {
        DPF(D_ERROR, ("Failed to power up PIN node"));
        return ntStatus;
    }
    
    // Disable unsolicited responses
    ntStatus = SendVerbInternal(m_DevExt.CodecNumber, PinNodeNumber, 0x708, 0x00, &response);
    
    // Disable any processing
    ntStatus = SendVerbInternal(m_DevExt.CodecNumber, PinNodeNumber, HDA_VERB_SET_PROC_STATE, 0x00, &response);
    
    // Enable PIN with output
    ntStatus = SendVerbInternal(m_DevExt.CodecNumber, PinNodeNumber, HDA_VERB_GET_PIN_WIDGET_CTL, 0x00, &response);
    ntStatus = SendVerbInternal(m_DevExt.CodecNumber, PinNodeNumber, HDA_VERB_SET_PIN_WIDGET_CTL, 
                               (response | HDA_PIN_WIDGET_CTRL_OUT_EN | HDA_PIN_WIDGET_CTRL_HP_EN), &response);
    
    // Enable EAPD + L-R swap
    ntStatus = SendVerbInternal(m_DevExt.CodecNumber, PinNodeNumber, HDA_VERB_SET_EAPD_BTL, 
                               (HDA_EAPD_BTL_ENABLE | HDA_EAPD_BTL_LRSWAP), &response);
    
    // Set maximal volume for PIN
    ntStatus = SendVerbInternal(m_DevExt.CodecNumber, PinNodeNumber, HDA_VERB_GET_PARAM, HDA_PARAM_OUT_AMP_CAP, 
                               &m_DevExt.output_amp_node_capabilities);
    
    if (NT_SUCCESS(ntStatus) && m_DevExt.output_amp_node_capabilities != 0) {
        // Set maximum gain
        ntStatus = SetNodeGain(m_DevExt.CodecNumber, PinNodeNumber, HDA_OUTPUT_NODE, 
                              m_DevExt.output_amp_node_capabilities, 100);
        
        // We will control volume by PIN node
        m_DevExt.output_amp_node_number = PinNodeNumber;
    }
    
    // Get first connected node to follow the path
    m_DevExt.length_of_node_path = 0;
    
    // Select first node
    ntStatus = SendVerbInternal(m_DevExt.CodecNumber, PinNodeNumber, HDA_VERB_SET_CONN_SELECT, 0x00, &response);
    
    // Get first connection
    USHORT firstNode = GetNodeConnectionEntry(m_DevExt.CodecNumber, PinNodeNumber, 0);
    if (firstNode == 0) {
        DPF(D_ERROR, ("PIN has no connections"));
        return STATUS_DEVICE_NOT_FOUND;
    }
    
    // Get type of first node
    UCHAR firstNodeType = GetNodeType(m_DevExt.CodecNumber, firstNode);
    
    // Initialize based on node type
    if (firstNodeType == HDA_WIDGET_AUDIO_OUTPUT) {
        return InitializeAudioOutput(SoundCardNumber, firstNode);
    } else {
        DPF(D_ERROR, ("PIN connected to unsupported node type %d", firstNodeType));
        return STATUS_NOT_SUPPORTED;
    }
}

NTSTATUS
CHdaCodec::InitializeAudioOutput(
    ULONG SoundCardNumber,
    ULONG AudioOutputNodeNumber
)
{
    PAGED_CODE();
    
    NTSTATUS ntStatus = STATUS_SUCCESS;
    ULONG response = 0;
    
    DPF(D_VERBOSE, ("Initializing Audio Output %d", AudioOutputNodeNumber));
    m_DevExt.audio_output_node_number = AudioOutputNodeNumber;
    
    // Turn on power for Audio Output
    ntStatus = SendVerbInternal(m_DevExt.CodecNumber, AudioOutputNodeNumber, HDA_VERB_SET_POWER_STATE, 0x00, &response);
    if (!NT_SUCCESS(ntStatus)) {
        DPF(D_ERROR, ("Failed to power up Audio Output node"));
        return ntStatus;
    }
    
    // Disable unsolicited responses
    ntStatus = SendVerbInternal(m_DevExt.CodecNumber, AudioOutputNodeNumber, 0x708, 0x00, &response);
    
    // Disable any processing
    ntStatus = SendVerbInternal(m_DevExt.CodecNumber, AudioOutputNodeNumber, HDA_VERB_SET_PROC_STATE, 0x00, &response);
    
    // Connect Audio Output to stream 1 channel 0
    ntStatus = SendVerbInternal(m_DevExt.CodecNumber, AudioOutputNodeNumber, 0x706, 0x10, &response);
    
    // Set maximal volume for Audio Output
    ULONG audioOutputAmpCapabilities = 0;
    ntStatus = SendVerbInternal(m_DevExt.CodecNumber, AudioOutputNodeNumber, HDA_VERB_GET_PARAM, HDA_PARAM_OUT_AMP_CAP, 
                               &audioOutputAmpCapabilities);
    
    if (NT_SUCCESS(ntStatus) && audioOutputAmpCapabilities != 0) {
        // Set gain to maximum
        ntStatus = SetNodeGain(m_DevExt.CodecNumber, AudioOutputNodeNumber, HDA_OUTPUT_NODE, 
                              audioOutputAmpCapabilities, 100);
        
        // We will control volume by Audio Output node
        m_DevExt.output_amp_node_number = AudioOutputNodeNumber;
        m_DevExt.output_amp_node_capabilities = audioOutputAmpCapabilities;
    }
    
    // Read audio capabilities
    ULONG audioOutputSampleCapabilities = 0;
    ntStatus = SendVerbInternal(m_DevExt.CodecNumber, AudioOutputNodeNumber, HDA_VERB_GET_PARAM, HDA_PARAM_PCM, 
                               &audioOutputS

/********************************************************************************
**    Copyright (c) 2025 HD Audio Driver Project. All Rights Reserved.
**
**    Ported from BleskOS HD Audio Driver to Windows Driver Model
**
********************************************************************************/

#include <wdm.h>
#include <portcls.h>
#include <stdunk.h>
#include "debug.h"
#include "hdaudio.h"

#define STR_MODULENAME "HDAUDIO: "

/*****************************************************************************
 * HD Audio Register Definitions
 */

// HD Audio controller registers
#define HDA_REG_GCAP                0x00    // Global Capabilities
#define HDA_REG_VMIN                0x02    // Minor Version
#define HDA_REG_VMAJ                0x03    // Major Version
#define HDA_REG_OUTPAY              0x04    // Output Payload Capability
#define HDA_REG_INPAY               0x06    // Input Payload Capability
#define HDA_REG_GCTL                0x08    // Global Control
#define HDA_REG_WAKEEN              0x0C    // Wake Enable
#define HDA_REG_STATESTS            0x0E    // State Change Status
#define HDA_REG_GSTS                0x10    // Global Status
#define HDA_REG_INTCTL              0x20    // Interrupt Control
#define HDA_REG_INTSTS              0x24    // Interrupt Status
#define HDA_REG_WALCLK              0x30    // Wall Clock Counter
#define HDA_REG_SSYNC               0x34    // Stream Synchronization
#define HDA_REG_CORBLBASE           0x40    // CORB Lower Base Address
#define HDA_REG_CORBUBASE           0x44    // CORB Upper Base Address
#define HDA_REG_CORBWP              0x48    // CORB Write Pointer
#define HDA_REG_CORBRP              0x4A    // CORB Read Pointer
#define HDA_REG_CORBCTL             0x4C    // CORB Control
#define HDA_REG_CORBSTS             0x4D    // CORB Status
#define HDA_REG_CORBSIZE            0x4E    // CORB Size
#define HDA_REG_RIRBLBASE           0x50    // RIRB Lower Base Address
#define HDA_REG_RIRBUBASE           0x54    // RIRB Upper Base Address
#define HDA_REG_RIRBWP              0x58    // RIRB Write Pointer
#define HDA_REG_RINTCNT             0x5A    // Response Interrupt Count
#define HDA_REG_RIRBCTL             0x5C    // RIRB Control
#define HDA_REG_RIRBSTS             0x5D    // RIRB Status
#define HDA_REG_RIRBSIZE            0x5E    // RIRB Size
#define HDA_REG_ICOI                0x60    // Immediate Command Output Interface
#define HDA_REG_ICII                0x64    // Immediate Command Input Interface
#define HDA_REG_ICIS                0x68    // Immediate Command Status

// Stream registers offset (relative to stream base)
#define HDA_STREAM_REG_CTL          0x00    // Stream Control
#define HDA_STREAM_REG_STS          0x03    // Stream Status
#define HDA_STREAM_REG_LPIB         0x04    // Link Position in Buffer
#define HDA_STREAM_REG_CBL          0x08    // Cyclic Buffer Length
#define HDA_STREAM_REG_LVI          0x0C    // Last Valid Index
#define HDA_STREAM_REG_FIFOS        0x10    // FIFO Size
#define HDA_STREAM_REG_FMT          0x12    // Format
#define HDA_STREAM_REG_BDPL         0x18    // Buffer Descriptor List Lower Base
#define HDA_STREAM_REG_BDPU         0x1C    // Buffer Descriptor List Upper Base

// CORBCTL bits
#define HDA_CORBCTL_CMEIE           0x01    // CORB Memory Error Interrupt Enable
#define HDA_CORBCTL_CORBRUN         0x02    // CORB DMA Run

// RIRBCTL bits
#define HDA_RIRBCTL_RINTCTL         0x01    // Response Interrupt Control
#define HDA_RIRBCTL_RIRBDMAEN       0x02    // RIRB DMA Enable

// GCTL bits
#define HDA_GCTL_CRST               0x01    // Controller Reset
#define HDA_GCTL_FCNTRL             0x02    // Flush Control
#define HDA_GCTL_UNSOL              0x100   // Accept Unsolicited Response

// Stream Control bits
#define HDA_SD_CTL_SRST             0x01    // Stream Reset
#define HDA_SD_CTL_RUN              0x02    // Stream Run
#define HDA_SD_CTL_IOCE             0x04    // Interrupt on Completion Enable
#define HDA_SD_CTL_DEIE             0x10    // Descriptor Error Interrupt Enable
#define HDA_SD_CTL_STRIPE           0x80    // Stripe Control

// Verb and command constants
#define HDA_VERB_GET_PARAM          0xF00   // Get Parameter
#define HDA_PARAM_VENDOR_ID         0x00    // Vendor ID
#define HDA_PARAM_SUBSYSTEM_ID      0x01    // Subsystem ID
#define HDA_PARAM_REV_ID            0x02    // Revision ID
#define HDA_PARAM_NODE_COUNT        0x04    // Subordinate Node Count
#define HDA_PARAM_FUNCTION_TYPE     0x05    // Function Type
#define HDA_PARAM_AUDIO_FG_CAP      0x08    // Audio Function Group Capabilities
#define HDA_PARAM_AUDIO_WIDGET_CAP  0x09    // Audio Widget Capabilities
#define HDA_PARAM_PCM               0x0A    // PCM Size and Rates
#define HDA_PARAM_STREAM            0x0B    // Stream Formats
#define HDA_PARAM_PIN_CAP           0x0C    // Pin Capabilities
#define HDA_PARAM_IN_AMP_CAP        0x0D    // Input Amplifier Capabilities
#define HDA_PARAM_OUT_AMP_CAP       0x12    // Output Amplifier Capabilities
#define HDA_PARAM_CONN_LIST_LEN     0x0E    // Connection List Length
#define HDA_PARAM_POWER_STATE       0x0F    // Supported Power States
#define HDA_PARAM_PROC_CAP          0x10    // Processing Capabilities
#define HDA_PARAM_GPIO_CAP          0x11    // GPIO Capabilities

#define HDA_VERB_GET_CONN_LIST      0xF02   // Get Connection List Entry
#define HDA_VERB_GET_CONN_SELECT    0xF01   // Get Connection Select Control
#define HDA_VERB_SET_CONN_SELECT    0x701   // Set Connection Select Control

#define HDA_VERB_SET_POWER_STATE    0x705   // Set Power State
#define HDA_VERB_GET_POWER_STATE    0xF05   // Get Power State

#define HDA_VERB_GET_CONFIG_DEFAULT 0xF1C   // Get Configuration Default
#define HDA_VERB_GET_PIN_WIDGET_CTL 0xF07   // Get Pin Widget Control
#define HDA_VERB_SET_PIN_WIDGET_CTL 0x707   // Set Pin Widget Control

#define HDA_VERB_GET_VOLUME_KNOB    0xF0F   // Get Volume Knob Control
#define HDA_VERB_SET_VOLUME_KNOB    0x70F   // Set Volume Knob Control

#define HDA_VERB_GET_BEEP_CTL       0xF0A   // Get Beep Control
#define HDA_VERB_SET_BEEP_CTL       0x70A   // Set Beep Control

#define HDA_VERB_GET_EAPD_BTL       0xF0C   // Get EAPD/BTL Enable
#define HDA_VERB_SET_EAPD_BTL       0x70C   // Set EAPD/BTL Enable

#define HDA_VERB_GET_DIGI_CONVERT   0xF0D   // Get Digital Converter
#define HDA_VERB_SET_DIGI_CONVERT_1 0x70D   // Set Digital Converter 1
#define HDA_VERB_SET_DIGI_CONVERT_2 0x70E   // Set Digital Converter 2

#define HDA_VERB_GET_STREAM_FORMAT  0xF20   // Get Stream Format
#define HDA_VERB_SET_STREAM_FORMAT  0x720   // Set Stream Format

#define HDA_VERB_GET_AMP_GAIN_MUTE  0xF3F   // Get Amplifier Gain/Mute
#define HDA_VERB_SET_AMP_GAIN_MUTE  0x300   // Set Amplifier Gain/Mute

#define HDA_VERB_GET_PROC_COEF      0xF04   // Get Processing Coefficient
#define HDA_VERB_SET_PROC_COEF      0x704   // Set Processing Coefficient

#define HDA_VERB_GET_PROC_STATE     0xF03   // Get Processing State
#define HDA_VERB_SET_PROC_STATE     0x703   // Set Processing State

// Pin Widget Control bits
#define HDA_PIN_WIDGET_CTRL_VREF    0x07    // VREF control
#define HDA_PIN_WIDGET_CTRL_IN_EN   0x20    // Input Enable
#define HDA_PIN_WIDGET_CTRL_OUT_EN  0x40    // Output Enable
#define HDA_PIN_WIDGET_CTRL_HP_EN   0x80    // Headphone Enable

// Amp control bits
#define HDA_AMP_GAIN_MUTE_MUTE      0x80    // Mute
#define HDA_AMP_GAIN_MUTE_RIGHT     0x00    // Right channel
#define HDA_AMP_GAIN_MUTE_LEFT      0x01    // Left channel
#define HDA_AMP_GAIN_MUTE_OUTPUT    0x80    // Output direction
#define HDA_AMP_GAIN_MUTE_INPUT     0x40    // Input direction

// EAPD/BTL control bits
#define HDA_EAPD_BTL_ENABLE         0x02    // EAPD Enable
#define HDA_EAPD_BTL_LRSWAP         0x04    // Left/Right Swap

// Status constants
#define STATUS_ERROR                0
#define STATUS_FALSE                0
#define STATUS_TRUE                 1
#define STATUS_GOOD                 1

/*****************************************************************************
 * Class Definitions
 */

class CHdaCodec : 
    public IHdaCodec,
    public CUnknown
{
private:
    PDEVICE_OBJECT                  m_pDeviceObject;     // Device object
    PDEVICE_OBJECT                  m_pPhysicalDeviceObject; // PDO
    PVOID                           m_pHDARegisters;     // MMIO registers
    HDA_DEVICE_EXTENSION            m_DevExt;            // Device extension
    BOOLEAN                         m_bDMAInitialized;   // DMA initialized flag
    BOOLEAN                         m_bCORBInitialized;  // CORB initialized flag
    BOOLEAN                         m_bRIRBInitialized;  // RIRB initialized flag
    
    NTSTATUS InitHardware(PDEVICE_OBJECT DeviceObject, PRESOURCELIST ResourceList);
    NTSTATUS ResetController();
    NTSTATUS InitCORBandRIRB();
    NTSTATUS InitStreamDescriptors();
    
    // Register access functions
    UCHAR ReadByte(ULONG offset);
    USHORT ReadWord(ULONG offset);
    ULONG ReadDword(ULONG offset);
    void WriteByte(ULONG offset, UCHAR data);
    void WriteWord(ULONG offset, USHORT data);
    void WriteDword(ULONG offset, ULONG data);
    
    // Stream register access functions
    UCHAR ReadStreamByte(ULONG streamBase, ULONG offset);
    USHORT ReadStreamWord(ULONG streamBase, ULONG offset);
    ULONG ReadStreamDword(ULONG streamBase, ULONG offset);
    void WriteStreamByte(ULONG streamBase, ULONG offset, UCHAR data);
    void WriteStreamWord(ULONG streamBase, ULONG offset, USHORT data);
    void WriteStreamDword(ULONG streamBase, ULONG offset, ULONG data);
    
    // CORB/RIRB helper functions
    NTSTATUS SendVerbInternal(ULONG Codec, ULONG Node, ULONG Verb, ULONG Command, PULONG Response);
    NTSTATUS SendVerbPIO(ULONG Codec, ULONG Node, ULONG Verb, ULONG Command, PULONG Response);
    NTSTATUS SendVerbCORB(ULONG Codec, ULONG Node, ULONG Verb, ULONG Command, PULONG Response);
    
    // Codec initialization
    NTSTATUS FindAndInitCodec(ULONG SoundCardNumber);
    NTSTATUS InitializeCodec(ULONG SoundCardNumber, ULONG CodecNumber);
    NTSTATUS InitializeAudioFunctionGroup(ULONG SoundCardNumber, ULONG AfgNodeNumber);
    NTSTATUS InitializeOutputPin(ULONG SoundCardNumber, ULONG PinNodeNumber);
    NTSTATUS InitializeAudioOutput(ULONG SoundCardNumber, ULONG AudioOutputNodeNumber);

public:
    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(CHdaCodec);
    ~CHdaCodec();

    IMP_IHdaCodec;
    
    friend NTSTATUS NewHdaCodec(
        OUT PUNKNOWN* Unknown,
        IN  REFCLSID,
        IN  PUNKNOWN   UnknownOuter OPTIONAL,
        IN  POOL_TYPE  PoolType);
};

/*****************************************************************************
 * Implementation
 */

NTSTATUS
NewHdaCodec(
    OUT PUNKNOWN* Unknown,
    IN  REFCLSID,
    IN  PUNKNOWN   UnknownOuter OPTIONAL,
    IN  POOL_TYPE  PoolType
)
{
    PAGED_CODE();
    ASSERT(Unknown);

    NTSTATUS ntStatus = STATUS_SUCCESS;
    CHdaCodec* pCodec = NULL;

    pCodec = new(PoolType, 'cdHA') CHdaCodec(UnknownOuter);
    
    if (pCodec) {
        *Unknown = (PUNKNOWN)(PHDACODEC)pCodec;
        (*Unknown)->AddRef();
    } else {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }

    return ntStatus;
}

CHdaCodec::~CHdaCodec(void)
{
    PAGED_CODE();
    
    // Disable controller
    if (m_pHDARegisters) {
        WriteDword(HDA_REG_GCTL, 0);
        
        // Unmap the memory
        MmUnmapIoSpace(m_pHDARegisters, PAGE_SIZE);
        m_pHDARegisters = NULL;
    }
    
    // Free allocated DMA buffers
    if (m_DevExt.CorbMem) {
        ExFreePool(m_DevExt.CorbMem);
        m_DevExt.CorbMem = NULL;
    }
    
    if (m_DevExt.RirbMem) {
        ExFreePool(m_DevExt.RirbMem);
        m_DevExt.RirbMem = NULL;
    }
    
    if (m_DevExt.OutputBufferList) {
        ExFreePool(m_DevExt.OutputBufferList);
        m_DevExt.OutputBufferList = NULL;
    }
    
    DPF(D_TERSE, ("HD Audio codec released"));
}

/*****************************************************************************
 * Register access functions
 */

UCHAR CHdaCodec::ReadByte(ULONG offset)
{
    return READ_PORT_UCHAR((PUCHAR)((ULONG_PTR)m_pHDARegisters + offset));
}

USHORT CHdaCodec::ReadWord(ULONG offset)
{
    return READ_PORT_USHORT((PUSHORT)((ULONG_PTR)m_pHDARegisters + offset));
}

ULONG CHdaCodec::ReadDword(ULONG offset)
{
    return READ_PORT_ULONG((PULONG)((ULONG_PTR)m_pHDARegisters + offset));
}

void CHdaCodec::WriteByte(ULONG offset, UCHAR data)
{
    WRITE_PORT_UCHAR((PUCHAR)((ULONG_PTR)m_pHDARegisters + offset), data);
}

void CHdaCodec::WriteWord(ULONG offset, USHORT data)
{
    WRITE_PORT_USHORT((PUSHORT)((ULONG_PTR)m_pHDARegisters + offset), data);
}

void CHdaCodec::WriteDword(ULONG offset, ULONG data)
{
    WRITE_PORT_ULONG((PULONG)((ULONG_PTR)m_pHDARegisters + offset), data);
}

/*****************************************************************************
 * Stream register access functions
 */

UCHAR CHdaCodec::ReadStreamByte(ULONG streamBase, ULONG offset)
{
    return READ_PORT_UCHAR((PUCHAR)((ULONG_PTR)m_pHDARegisters + streamBase + offset));
}

USHORT CHdaCodec::ReadStreamWord(ULONG streamBase, ULONG offset)
{
    return READ_PORT_USHORT((PUSHORT)((ULONG_PTR)m_pHDARegisters + streamBase + offset));
}

ULONG CHdaCodec::ReadStreamDword(ULONG streamBase, ULONG offset)
{
    return READ_PORT_ULONG((PULONG)((ULONG_PTR)m_pHDARegisters + streamBase + offset));
}

void CHdaCodec::WriteStreamByte(ULONG streamBase, ULONG offset, UCHAR data)
{
    WRITE_PORT_UCHAR((PUCHAR)((ULONG_PTR)m_pHDARegisters + streamBase + offset), data);
}

void CHdaCodec::WriteStreamWord(ULONG streamBase, ULONG offset, USHORT data)
{
    WRITE_PORT_USHORT((PUSHORT)((ULONG_PTR)m_pHDARegisters + streamBase + offset), data);
}

void CHdaCodec::WriteStreamDword(ULONG streamBase, ULONG offset, ULONG data)
{
    WRITE_PORT_ULONG((PULONG)((ULONG_PTR)m_pHDARegisters + streamBase + offset), data);
}

/*****************************************************************************
 * Hardware Initialization
 */

NTSTATUS
CHdaCodec::ResetController()
{
    PAGED_CODE();
    
    // Reset the controller
    WriteDword(HDA_REG_GCTL, 0);
    
    // Wait for reset to complete
    for (int i = 0; i < 100; i++) {
        if ((ReadDword(HDA_REG_GCTL) & HDA_GCTL_CRST) == 0) {
            break;
        }
        KeStallExecutionProcessor(10);
    }
    
    // Verify reset completed
    if ((ReadDword(HDA_REG_GCTL) & HDA_GCTL_CRST) != 0) {
        DPF(D_ERROR, ("HD Audio controller failed to reset"));
        return STATUS_DEVICE_NOT_READY;
    }
    
    // Set operational state
    WriteDword(HDA_REG_GCTL, HDA_GCTL_CRST);
    
    // Wait for operational state
    for (int i = 0; i < 100; i++) {
        if ((ReadDword(HDA_REG_GCTL) & HDA_GCTL_CRST) != 0) {
            break;
        }
        KeStallExecutionProcessor(10);
    }
    
    // Verify operational state
    if ((ReadDword(HDA_REG_GCTL) & HDA_GCTL_CRST) == 0) {
        DPF(D_ERROR, ("HD Audio controller failed to enter operational state"));
        return STATUS_DEVICE_NOT_READY;
    }
    
    return STATUS_SUCCESS;
}

NTSTATUS
CHdaCodec::InitCORBandRIRB()
{
    PAGED_CODE();
    
    // Stop CORB and RIRB DMA engines
    WriteByte(HDA_REG_CORBCTL, 0);
    WriteByte(HDA_REG_RIRBCTL, 0);
    
    // Allocate memory for CORB (256 entries, 4 bytes each)
    m_DevExt.CorbMem = (PULONG)ExAllocatePoolWithTag(NonPagedPool, 256 * 4, 'BroC');
    if (!m_DevExt.CorbMem) {
        DPF(D_ERROR, ("Failed to allocate CORB memory"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(m_DevExt.CorbMem, 256 * 4);
    
    // Set CORB buffer address
    WriteDword(HDA_REG_CORBLBASE, MmGetPhysicalAddress(m_DevExt.CorbMem).LowPart);
    WriteDword(HDA_REG_CORBUBASE, MmGetPhysicalAddress(m_DevExt.CorbMem).HighPart);
    
    // Determine CORB size
    UCHAR corbSizeReg = ReadByte(HDA_REG_CORBSIZE);
    if (corbSizeReg & 0x40) {
        // 256 entries supported
        m_DevExt.CorbNumberOfEntries = 256;
        WriteByte(HDA_REG_CORBSIZE, 0x2);
        DPF(D_VERBOSE, ("CORB: 256 entries"));
    } else if (corbSizeReg & 0x20) {
        // 16 entries supported
        m_DevExt.CorbNumberOfEntries = 16;
        WriteByte(HDA_REG_CORBSIZE, 0x1);
        DPF(D_VERBOSE, ("CORB: 16 entries"));
    } else if (corbSizeReg & 0x10) {
        // 2 entries supported
        m_DevExt.CorbNumberOfEntries = 2;
        WriteByte(HDA_REG_CORBSIZE, 0x0);
        DPF(D_VERBOSE, ("CORB: 2 entries"));
    } else {
        // CORB not supported
        DPF(D_ERROR, ("CORB: no size allowed"));
        return STATUS_NOT_SUPPORTED;
    }
    
    // Reset CORB Read Pointer
    WriteWord(HDA_REG_CORBRP, 0x8000);
    
    // Wait for reset to complete
    for (int i = 0; i < 50; i++) {
        if ((ReadWord(HDA_REG_CORBRP) & 0x8000) != 0) {
            break;
        }
        KeStallExecutionProcessor(10);
    }
    
    // Verify reset
    if ((ReadWord(HDA_REG_CORBRP) & 0x8000) == 0) {
        DPF(D_ERROR, ("CORB read pointer reset failed"));
        return STATUS_DEVICE_NOT_READY;
    }
    
    // Exit reset state
    WriteWord(HDA_REG_CORBRP, 0);
    
    // Wait for normal state
    for (int i = 0; i < 50; i++) {
        if ((ReadWord(HDA_REG_CORBRP) & 0x8000) == 0) {
            break;
        }
        KeStallExecutionProcessor(10);
    }
    
    // Initialize CORB Write Pointer
    WriteWord(HDA_REG_CORBWP, 0);
    m_DevExt.CorbPointer = 1;
    
    // Allocate memory for RIRB (256 entries, 8 bytes each)
    m_DevExt.RirbMem = (PULONG)ExAllocatePoolWithTag(NonPagedPool, 256 * 8, 'BriR');
    if (!m_DevExt.RirbMem) {
        DPF(D_ERROR, ("Failed to allocate RIRB memory"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(m_DevExt.RirbMem, 256 * 8);
    
    // Set RIRB buffer address
    WriteDword(HDA_REG_RIRBLBASE, MmGetPhysicalAddress(m_DevExt.RirbMem).LowPart);
    WriteDword(HDA_REG_RIRBUBASE, MmGetPhysicalAddress(m_DevExt.RirbMem).HighPart);
    
    // Determine RIRB size
    UCHAR rirbSizeReg = ReadByte(HDA_REG_RIRBSIZE);
    if (rirbSizeReg & 0x40) {
        // 256 entries supported
        m_DevExt.RirbNumberOfEntries = 256;
        WriteByte(HDA_REG_RIRBSIZE, 0x2);
        DPF(D_VERBOSE, ("RIRB: 256 entries"));
    } else if (rirbSizeReg & 0x20) {
        // 16 entries supported
        m_DevExt.RirbNumberOfEntries = 16;
        WriteByte(HDA_REG_RIRBSIZE, 0x1);
        DPF(D_VERBOSE, ("RIRB: 16 entries"));
    } else if (rirbSizeReg & 0x10) {
        // 2 entries supported
        m_DevExt.RirbNumberOfEntries = 2;
        WriteByte(HDA_REG_RIRBSIZE, 0x0);
        DPF(D_VERBOSE, ("RIRB: 2 entries"));
    } else {
        // RIRB not supported
        DPF(D_ERROR, ("RIRB: no size allowed"));
        return STATUS_NOT_SUPPORTED;
    }
    
    // Reset RIRB Write Pointer
    WriteWord(HDA_REG_RIRBWP, 0x8000);
    KeStallExecutionProcessor(100);
    
    // Disable RIRB interrupts
    WriteWord(HDA_REG_RINTCNT, 0);
    m_DevExt.RirbPointer = 1;
    
    // Start CORB and RIRB DMA engines
    WriteByte(HDA_REG_CORBCTL, HDA_CORBCTL_CORBRUN);
    WriteByte(HDA_REG_RIRBCTL, HDA_RIRBCTL_RIRBDMAEN);
    
    m_bCORBInitialized = TRUE;
    m_bRIRBInitialized = TRUE;
    
    return STATUS_SUCCESS;
}

NTSTATUS
CHdaCodec::InitStreamDescriptors()
{
    PAGED_CODE();
    
    // Allocate memory for Output Buffer List (16 entries, 2 bytes each)
    m_DevExt.OutputBufferList = (PULONG)ExAllocatePoolWithTag(NonPagedPool, 16 * 8, 'OHda');
    if (!m_DevExt.OutputBufferList) {
        DPF(D_ERROR, ("Failed to allocate Output Buffer List memory"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(m_DevExt.OutputBufferList, 16 * 8);
    
    return STATUS_SUCCESS;
}

NTSTATUS
CHdaCodec::InitHardware(
    PDEVICE_OBJECT DeviceObject,
    PRESOURCELIST ResourceList
)
{
    PAGED_CODE();
    
    NTSTATUS ntStatus = STATUS_SUCCESS;
    PHYSICAL_ADDRESS physAddr = {0};
    ULONG memLength = 0;
    
    DPF(D_VERBOSE, ("Initializing HD Audio hardware"));
    
    // Get memory resource
    for (ULONG i = 0; i < ResourceList->NumberOfEntries(); i++) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR desc = ResourceList->FindTranslatedEntry(CmResourceTypeMemory, i);
        if (desc) {
            physAddr = desc->u.Memory.Start;
            memLength = desc->u.Memory.Length;
            break;
        }
    }
    
    // Map the device memory into kernel space
    m_pHDARegisters = MmMapIoSpace(physAddr, memLength, MmNonCached);
    if (!m_pHDARegisters) {
        DPF(D_ERROR, ("Failed to map HD Audio registers"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    // Store base address
    m_DevExt.Base = (ULONG)(ULONG_PTR)m_pHDARegisters;
    
    // Reset the controller
    ntStatus = ResetController();
    if (!NT_SUCCESS(ntStatus)) {
        DPF(D_ERROR, ("Failed to reset HD Audio controller: 0x%08X", ntStatus));
        return ntStatus;
    }
    
    // Read capabilities
    USHORT gcap = ReadWord(HDA_REG_GCAP);
    UCHAR major = ReadByte(HDA_REG_VMAJ);
    UCHAR minor = ReadByte(HDA_REG_VMIN);
    
    DPF(D_TERSE, ("HD Audio controller version %d.%d", major, minor));
    DPF(D_VERBOSE, ("Input streams: %d, Output streams: %d, Bidirectional streams: %d",
                   (gcap & 0xF), ((gcap >> 4) & 0xF), ((gcap >> 8) & 0xF)));
    
    // Set up stream bases
    m_DevExt.InputStreamBase = m_DevExt.Base + 0x80;
    m_DevExt.OutputStreamBase = m_DevExt.Base + 0x80 + (0x20 * ((gcap >> 8) & 0xF));
    
    // Disable interrupts
    WriteDword(HDA_REG_INTCTL, 0);
    
    // Turn off DMA position transfer
    WriteDword(HDA_REG_WALCLK, 0);
    WriteDword(HDA_REG_SSYNC, 0);
    
    // Initialize CORB and RIRB
    ntStatus = InitCORBandRIRB();
    if (!NT_SUCCESS(ntStatus)) {
        DPF(D_ERROR, ("Failed to initialize CORB and RIRB: 0x%08X", ntStatus));
        return ntStatus;
    }
    
    // Initialize stream descriptors
    ntStatus = InitStreamDescriptors();
    if (!NT_SUCCESS(ntStatus)) {
        DPF(D_ERROR, ("Failed to initialize stream descriptors: 0x%08X", ntStatus));
        return ntStatus;
    }
    
    return STATUS_SUCCESS;
}

