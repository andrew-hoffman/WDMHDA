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
#define HDA_PIN_INVALID 0x10

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

	//Codec output paths
	HDA_OUTPUT_LIST out_paths;

    ULONG pin_output_node_number;
    ULONG headphone_node_number;



public:
    explicit HDA_Codec(BOOLEAN useSpdif, BOOLEAN useAltOut, UCHAR num, IAdapterCommon* adapter);
    ~HDA_Codec();

	STDMETHODIMP_(NTSTATUS) InitializeCodec();
	STDMETHODIMP_(ULONG) hda_send_verb(
        ULONG Node,
        ULONG Verb,
        ULONG Payload
    );

	STDMETHODIMP_(NTSTATUS) hda_initialize_audio_function_group(ULONG afg_node_number); 
	STDMETHODIMP_(UCHAR) hda_get_node_type(ULONG node);
	STDMETHODIMP_(ULONG) hda_get_node_connection_entries(ULONG node, ULONG connection_entries_number);
	STDMETHODIMP_(NTSTATUS) hda_initialize_output_pin(ULONG pin_node_number, HDA_NODE_PATH& path);
	STDMETHODIMP_(void) hda_initialize_audio_output(ULONG output_node_number, HDA_NODE_PATH& path);
	STDMETHODIMP_(void) hda_initialize_audio_mixer(ULONG audio_mixer_node_number, HDA_NODE_PATH& path);
	STDMETHODIMP_(void) hda_initialize_audio_selector(ULONG audio_selector_node_number, HDA_NODE_PATH& path);
	STDMETHODIMP_(BOOLEAN) hda_is_headphone_connected(void);
	STDMETHODIMP_(void) hda_set_volume(ULONG volume, UCHAR ch);
	STDMETHODIMP_(void) hda_set_node_gain(ULONG node, ULONG node_type, ULONG capabilities, ULONG gain, UCHAR ch);

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
