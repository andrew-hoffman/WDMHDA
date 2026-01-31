/*****************************************************************************
 * codec.h - Codec object header.
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
class CAdapterCommon;

/*****************************************************************************
 * HDA_Codec
 *****************************************************************************
 * HD Audio Codec object for managing individual codec instances on the link.
 * Supports up to 16 codecs per HD Audio link as per the spec.
 */
class HDA_Codec
{
private:
    CAdapterCommon* pAdapter;           // Reference to parent adapter
    UCHAR codec_address;                // Codec address on the link (0-15)
    ULONG codec_id;                     // Codec vendor ID and device ID
    
    ULONG is_initialized_useful_output;
    ULONG selected_output_node;
    ULONG length_of_node_path;

    ULONG afg_node_sample_capabilities;
    ULONG afg_node_stream_format_capabilities;
    ULONG afg_node_input_amp_capabilities;
    ULONG afg_node_output_amp_capabilities;

	//Codec output paths
	HDA_OUTPUT_LIST out_paths;

    ULONG pin_output_node_number;
    ULONG pin_headphone_node_number;

public:
    HDA_Codec(CAdapterCommon* adapter, UCHAR num);
    ~HDA_Codec();

	NTSTATUS InitializeCodec();

	ULONG hda_send_verb(ULONG node, ULONG verb, ULONG command);
	PULONG get_bdl_mem(void);
	NTSTATUS hda_initialize_audio_function_group(ULONG afg_node_number); 
	ULONG hda_get_actual_stream_position(void);
	UCHAR hda_get_node_type(ULONG node);
	ULONG hda_get_node_connection_entries(ULONG node, ULONG connection_entries_number);
	void hda_initialize_output_pin(ULONG pin_node_number, HDA_NODE_PATH& path);
	void hda_initialize_audio_output(ULONG output_node_number, HDA_NODE_PATH& path);
	void hda_initialize_audio_mixer(ULONG audio_mixer_node_number, HDA_NODE_PATH& path);
	void hda_initialize_audio_selector(ULONG audio_selector_node_number, HDA_NODE_PATH& path);
	BOOLEAN hda_is_headphone_connected(void);
	void hda_set_node_gain(ULONG node, ULONG node_type, ULONG capabilities, ULONG gain, UCHAR ch);
	void hda_set_volume(ULONG volume, UCHAR ch);
	void hda_start_sound(void);
	void hda_stop_sound(void);

	void hda_check_headphone_connection_change(void);
	UCHAR hda_is_supported_channel_size(UCHAR size);
	UCHAR hda_is_supported_sample_rate(ULONG sample_rate);
	void hda_enable_pin_output(ULONG pin_node);
	void hda_disable_pin_output(ULONG pin_node);
	USHORT hda_return_sound_data_format(ULONG sample_rate, ULONG channels, ULONG bits_per_sample);
	NTSTATUS hda_stop_stream(void);
    
    // Accessors
    UCHAR GetCodecAddress() const { return codec_address; }
    ULONG GetCodecId() const { return codec_id; }
    BOOLEAN IsInitialized() const { return is_initialized_useful_output; }
};

typedef HDA_Codec *PHDACODEC;

#endif // _CODEC_H_
