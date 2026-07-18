/*****************************************************************************
 * codec.h - Audio Codec object.
 *****************************************************************************
 * Copyright (c) 2026 Drew Hoffman
 * Released under MIT License
 * Code from BleskOS and Microsoft's driver samples used under MIT license. 
 *
 */

#ifndef _CODEC_H_
#define _CODEC_H_

/*****************************************************************************
 * Forward declarations
 */
struct IAdapterCommon;

/*****************************************************************************
 * Typedefs
 */
typedef ULONG (*HDA_SEND_VERB_FUNC)(IAdapterCommon* adapter, ULONG codec, ULONG node, ULONG verb, ULONG command);

#define MAX_OUTPUT_PATHS 8

typedef struct
{	
	ULONG path_type;
	ULONG audio_output_node_number;
    ULONG audio_output_node_sample_capabilities;
    ULONG audio_output_node_stream_format_capabilities;
    ULONG output_amp_node_number;
    ULONG output_amp_node_capabilities;
    ULONG mute_amp_node_number;
    ULONG mute_amp_node_capabilities;
} HDA_NODE_PATH, *PHDA_NODE_PATH;

typedef struct _HDA_OUTPUT_LIST {
    ULONG count;
    HDA_NODE_PATH paths[MAX_OUTPUT_PATHS];
} HDA_OUTPUT_LIST;

/*****************************************************************************
 * Constants
 */

// Widget types
#define HDA_WIDGET_AUDIO_OUTPUT 0x0
#define HDA_WIDGET_AUDIO_INPUT 0x1
#define HDA_WIDGET_AUDIO_MIXER 0x2
#define HDA_WIDGET_AUDIO_SELECTOR 0x3
#define HDA_WIDGET_PIN_COMPLEX 0x4
#define HDA_WIDGET_POWER_WIDGET 0x5
#define HDA_WIDGET_VOLUME_KNOB 0x6
#define HDA_WIDGET_BEEP_GENERATOR 0x7
#define HDA_WIDGET_VENDOR_DEFINED 0xF

// Pin definitions
#define HDA_PIN_LINE_OUT 0x0
#define HDA_PIN_SPEAKER 0x1
#define HDA_PIN_HEADPHONE_OUT 0x2
#define HDA_PIN_CD 0x3
#define HDA_PIN_SPDIF_OUT 0x4
#define HDA_PIN_DIGITAL_OTHER_OUT 0x5
#define HDA_PIN_MODEM_LINE_SIDE 0x6
#define HDA_PIN_MODEM_HANDSET_SIDE 0x7
#define HDA_PIN_LINE_IN 0x8
#define HDA_PIN_AUX 0x9
#define HDA_PIN_MIC_IN 0xA
#define HDA_PIN_TELEPHONY 0xB
#define HDA_PIN_SPDIF_IN 0xC
#define HDA_PIN_DIGITAL_OTHER_IN 0xD
#define HDA_PIN_RESERVED 0xE
#define HDA_PIN_OTHER 0xF

// Node types
#define HDA_OUTPUT_NODE 0x1
#define HDA_INPUT_NODE 0x2

// Codec quirks
#define HDA_QUIRK_ALC292_DELL_M4800 0x00000001

// HDA verbs
#define VERB_GET_PARAMETER            0xF00
#define VERB_GET_CONN_LIST_ENTRY      0xF02
#define VERB_GET_CONFIG_DEFAULT       0xF1C
#define VERB_GET_SUBSYSTEM_ID         0xF20
#define VERB_GET_PIN_SENSE            0xF09
#define VERB_SET_PIN_WIDGET_CONTROL   0x707
#define VERB_SET_EAPD_BTLENABLE       0x70C
#define VERB_GET_CONN_LIST_LEN		0xF00
#define AC_PAR_CONNLIST_LEN      0x0E
#define VERB_SET_CONNECT_SEL 0x701
#define VERB_GET_CONNECT_SEL 0xF01
#define VERB_SET_COEF_INDEX   0x500
#define VERB_GET_COEF_INDEX   0xD00
#define VERB_SET_PROC_COEF    0x400
#define VERB_GET_PROC_COEF    0xC00
#define VERB_GET_PIN_WIDGET_CONTROL 0xF07
#define VERB_GET_EAPD_BTLENABLE     0xF0C
#define VERB_GET_AMP_GAIN_MUTE      0xB00
#define VERB_SET_AMP_GAIN_MUTE      0x300

// Pin Widget Control bits
#define PINCTL_OUT_EN                 0x40
#define PINCTL_IN_EN                  0x20

// GET_PARAMETER id's
#define AC_PAR_AUDIO_WIDGET_CAP       0x09
#define AC_PAR_PIN_CAP                0x0C

// Widget caps helpers
#define AC_WCAP_TYPE_SHIFT 20
#define AC_WCAP_TYPE_MASK  (0xF << AC_WCAP_TYPE_SHIFT)
#define AC_WCAP_TYPE_PIN   (0x4 << AC_WCAP_TYPE_SHIFT)



/*****************************************************************************
 * HDA_Codec
 *****************************************************************************
 * HD Audio Codec object for managing individual codec instances on the link.
 * Supports up to 16 codecs per HD Audio link as per the spec.
 */
class HDA_Codec
{
private:
    IAdapterCommon* pAdapter;           // Reference to parent adapter
    UCHAR codec_address;                // Codec address on the link (0-15)
    ULONG codec_id;                     // Codec vendor ID and device ID
	ULONG codec_subsystem_id;             // Codec subsystem ID reported by the AFG
	ULONG codec_quirks;                   // Hardware-specific initialization flags
	USHORT codec_ven;
	USHORT codec_dev;
	BOOLEAN isRealtek;					// Realtek codecs need different init order
    
    BOOLEAN useSpdif;                   // Use SPDIF flag
    BOOLEAN useAltOut;                  // Use alternate output flag
    
    ULONG selected_output_node;
    ULONG length_of_node_path;

    ULONG afg_node_sample_capabilities;
    ULONG afg_node_stream_format_capabilities;
    ULONG afg_node_input_amp_capabilities;
    ULONG afg_node_output_amp_capabilities;

	USHORT prev_data_format;

	//Codec output paths
	HDA_OUTPUT_LIST out_paths;

    ULONG pin_output_node_number;
    ULONG headphone_node_number;
	ULONG dock_lineout_node_number;
	
	LONG HpPollingEnabled;
    LONG InShutdown;

    void ForcePinOut(ULONG pinNid, BOOLEAN enable);
    void ForceEapd(ULONG pinNid, BOOLEAN enable);
    BOOLEAN IsHpPresent();
    void SwitchOutput(BOOLEAN hpPresent);
    void UnmutePinOutAmp(ULONG pinNid);
    void MutePinOutAmp(ULONG pinNid);   
    void UnmutePinInAmp(ULONG pinNid, UCHAR inIndex);
    void MutePinInAmp(ULONG pinNid, UCHAR inIndex); 
    void ForceConnSel(ULONG nid, UCHAR sel);
    //void WakeSpeakerPath();
    ULONG ReadCoef(USHORT idx);
    void WriteCoef(USHORT idx, USHORT val);
    void ApplyAlc292HeadphoneMode();
	BOOLEAN IsDockLineoutPresent();
    void SetOutAmpLR(ULONG nid, BOOLEAN mute, UCHAR gain);
    //void ApplyEeeInit();
    //void ForcePlaybackChain();
    void UnmuteInAmp(ULONG nid, UCHAR inIndex, UCHAR gain);
    void UnmuteOutAmp(ULONG nid, UCHAR gain);


public:
    explicit HDA_Codec(BOOLEAN useSpdif, BOOLEAN useAltOut, UCHAR num, IAdapterCommon* adapter);
    ~HDA_Codec();

	STDMETHODIMP_(NTSTATUS) InitializeCodec();
	STDMETHODIMP_(ULONG) hda_send_verb(
        ULONG Node,
        ULONG Verb,
        ULONG Payload
    );

	STDMETHODIMP_(ULONG) SendVerbLogged(ULONG node, ULONG verb, ULONG command, const char* tag);

	STDMETHODIMP_(NTSTATUS) hda_initialize_audio_function_group(ULONG afg_node_number); 
	STDMETHODIMP_(UCHAR) hda_get_node_type(ULONG node);
	STDMETHODIMP_(ULONG) hda_get_node_connection_entries(ULONG node, ULONG connection_entries_number);
	STDMETHODIMP_(NTSTATUS) hda_initialize_output_pin(ULONG pin_node_number, HDA_NODE_PATH& path);
	STDMETHODIMP_(void) hda_initialize_audio_output(ULONG output_node_number, HDA_NODE_PATH& path);
	STDMETHODIMP_(void) hda_initialize_audio_mixer(ULONG audio_mixer_node_number, HDA_NODE_PATH& path);
	STDMETHODIMP_(void) hda_initialize_audio_selector(ULONG audio_selector_node_number, HDA_NODE_PATH& path);
	STDMETHODIMP_(BOOLEAN) hda_is_headphone_connected(void);
	STDMETHODIMP_(void) hda_set_volume(ULONG volume, UCHAR ch, BOOLEAN mute);
	STDMETHODIMP_(void) hda_set_node_gain(ULONG node, ULONG node_type, ULONG capabilities, ULONG gain, UCHAR ch, BOOLEAN mute);

	STDMETHODIMP_(void) hda_check_headphone_connection_change(void);
	STDMETHODIMP_(UCHAR) hda_is_supported_sample_rate(ULONG sample_rate);
	STDMETHODIMP_(void) hda_enable_pin_output(ULONG pin_node);
	STDMETHODIMP_(void) hda_disable_pin_output(ULONG pin_node);
	STDMETHODIMP_(USHORT) hda_return_sound_data_format(ULONG sample_rate, ULONG channels, ULONG bits_per_sample);
	STDMETHODIMP_(NTSTATUS) ProgramSampleRate(DWORD dwSampleRate);
    
    // Accessors
    UCHAR GetCodecAddress() const { return codec_address; }
    ULONG GetCodecId() const { return codec_id; }
    BOOLEAN IsInitialized() const { return (BOOLEAN) (out_paths.count > 0); }
};

typedef HDA_Codec *PHDACODEC;

#endif // _CODEC_H_
