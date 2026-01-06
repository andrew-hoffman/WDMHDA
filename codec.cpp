/*****************************************************************************
 * codec.cpp - Codec object.
 *****************************************************************************
 * Copyright (c) 2026 Drew Hoffman
 * Released under MIT License
 * Code from BleskOS and Microsoft's driver samples used under MIT license. 
 *
 */

#include "common.h"

class HDA_Codec
{
private:
    ULONG is_initialized_useful_output;
    ULONG selected_output_node;
    ULONG length_of_node_path;

    ULONG afg_node_sample_capabilities;
    ULONG afg_node_stream_format_capabilities;
    ULONG afg_node_input_amp_capabilities;
    ULONG afg_node_output_amp_capabilities;

	//Codec output paths
	HDA_OUTPUT_LIST out_paths;

    ULONG audio_output_node_number;
    ULONG audio_output_node_sample_capabilities;
    ULONG audio_output_node_stream_format_capabilities;
    ULONG output_amp_node_number;
    ULONG output_amp_node_capabilities;

    ULONG second_audio_output_node_number;
    ULONG second_audio_output_node_sample_capabilities;
    ULONG second_audio_output_node_stream_format_capabilities;
    ULONG second_output_amp_node_number;
    ULONG second_output_amp_node_capabilities;

    ULONG pin_output_node_number;
    ULONG pin_headphone_node_number;

public:

	STDMETHODIMP_(NTSTATUS) InitializeCodec (UCHAR num);

	STDMETHODIMP_(ULONG)	hda_send_verb(ULONG codec, ULONG node, ULONG verb, ULONG command);
	STDMETHODIMP_(PULONG)	get_bdl_mem(void);
	STDMETHODIMP_(NTSTATUS)	hda_initialize_audio_function_group(ULONG codec_number, ULONG afg_node_number); 
	STDMETHODIMP_(ULONG)	hda_get_actual_stream_position(void);
	STDMETHODIMP_(UCHAR)	hda_get_node_type(ULONG codec, ULONG node);
	STDMETHODIMP_(ULONG)	hda_get_node_connection_entries(ULONG codec, ULONG node, ULONG connection_entries_number);
	STDMETHODIMP_(void)		hda_initialize_output_pin(ULONG pin_node_number);
	STDMETHODIMP_(void)		hda_initialize_audio_output(ULONG output_node_number);
	STDMETHODIMP_(void)		hda_initialize_audio_mixer(ULONG audio_mixer_node_number);
	STDMETHODIMP_(void)		hda_initialize_audio_selector(ULONG audio_selector_node_number);
	STDMETHODIMP_(BOOLEAN)	hda_is_headphone_connected (void);
	STDMETHODIMP_(void)		hda_set_node_gain(ULONG codec, ULONG node, ULONG node_type, ULONG capabilities, ULONG gain, UCHAR ch);
	STDMETHODIMP_(void)		hda_set_volume(ULONG volume, UCHAR ch);
	STDMETHODIMP_(void)		hda_start_sound (void);
	STDMETHODIMP_(void)		hda_stop_sound (void);

	STDMETHODIMP_(void)		hda_check_headphone_connection_change(void);
	STDMETHODIMP_(UCHAR)	hda_is_supported_channel_size(UCHAR size);
	STDMETHODIMP_(UCHAR)	hda_is_supported_sample_rate(ULONG sample_rate);
	STDMETHODIMP_(void)		hda_enable_pin_output(ULONG codec, ULONG pin_node);
	STDMETHODIMP_(void)		hda_disable_pin_output(ULONG codec, ULONG pin_node);
	STDMETHODIMP_(NTSTATUS)	hda_showtime(PDMACHANNEL DmaChannel);
	STDMETHODIMP_(USHORT)	hda_return_sound_data_format(ULONG sample_rate, ULONG channels, ULONG bits_per_sample);
	
	/*****************************************************************************
 * CAdapterCommon::InitializeCodec
 *****************************************************************************
 */

STDMETHODIMP_(NTSTATUS) CAdapterCommon::InitializeCodec (UCHAR codec_number)
{
    PAGED_CODE ();

	//test if this codec exists
	ULONG codec_id = hda_send_verb(codec_number, 0, 0xF00, 0);
	if(codec_id == 0x00000000) {
		return STATUS_NOT_FOUND;
	}
	codecNumber = codec_number;

	//log basic codec info
	DOUT (DBG_SYSINFO, ("Codec %d ", codec_number ) );
	DOUT (DBG_SYSINFO, ("VID: %04x", (codec_id>>16) ));
	DOUT (DBG_SYSINFO, ("PID: %04x", (codec_id & 0xFFFF) ));

	//find Audio Function Groups
	ULONG response = hda_send_verb(codec_number, 0, 0xF00, 0x04);

	DOUT (DBG_SYSINFO, ( "First Group node: %d Number of Groups: %d", 
	 (response >> 16) & 0xFF, response & 0xFF 
	 ));
	ULONG node = (response >> 16) & 0xFF;
	for(ULONG last_node = (node + (response & 0xFF)); node < last_node; node++) {
		if((hda_send_verb(codec_number, node, 0xF00, 0x05) & 0x7F)==0x01) { //this is Audio Function Group
			//initialize (first) Audio Function Group
			return hda_initialize_audio_function_group(codec_number, node); 
			//todo: handle multiple AFGs?
		}
	}
	DOUT (DBG_ERROR, ("HDA ERROR: No AFG found"));
	return STATUS_NOT_FOUND;	
}

/*****************************************************************************
 * CAdapterCommon::initialize_audio_function_group
 *****************************************************************************
 */

STDMETHODIMP_(NTSTATUS) CAdapterCommon::hda_initialize_audio_function_group(ULONG codec_number, ULONG afg_node_number) {
	PAGED_CODE ();
	HDA_NODE_PATH path;

	//reset AFG
	hda_send_verb(codec_number, afg_node_number, 0x7FF, 0x00);

	//disable unsolicited responses
	hda_send_verb(codec_number, afg_node_number, 0x708, 0x00);

	//enable power for AFG
	hda_send_verb(codec_number, afg_node_number, 0x705, 0x00);

	//read available info
	afg_node_sample_capabilities = hda_send_verb(codec_number, afg_node_number, 0xF00, 0x0A);
	afg_node_stream_format_capabilities = hda_send_verb(codec_number, afg_node_number, 0xF00, 0x0B);
	afg_node_input_amp_capabilities = hda_send_verb(codec_number, afg_node_number, 0xF00, 0x0D);
	afg_node_output_amp_capabilities = hda_send_verb(codec_number, afg_node_number, 0xF00, 0x12);

	//log AFG info
	DOUT (DBG_SYSINFO, ("\nAudio Function Group node %d", afg_node_number));
	DOUT (DBG_SYSINFO, ("\nAFG sample capabilities: 0x%x", afg_node_sample_capabilities));
	DOUT (DBG_SYSINFO, ("\nAFG stream format capabilities: 0x%x", afg_node_stream_format_capabilities));
	DOUT (DBG_SYSINFO, ("\nAFG input amp capabilities: 0x%x", afg_node_input_amp_capabilities));
	DOUT (DBG_SYSINFO, ("\nAFG output amp capabilities: 0x%x", afg_node_output_amp_capabilities));

	//log all AFG nodes and find useful PINs
	DbgPrint( ("\n\nLIST OF ALL NODES IN AFG:"));
	ULONG subordinate_node_count_reponse = hda_send_verb(codec_number, afg_node_number, 0xF00, 0x04);
	ULONG pin_alternative_output_node_number = 0, pin_speaker_default_node_number = 0; 
	ULONG pin_speaker_node_number = 0, pin_headphone_node_number = 0, pin_spdif_node_number = 0;

	pin_output_node_number = 0;
	pin_headphone_node_number = 0;

	for (ULONG node = ((subordinate_node_count_reponse>>16) & 0xFF), last_node = (node+(subordinate_node_count_reponse & 0xFF)), 
		type_of_node = 0; node < last_node; node++) {

		//log number of node
		DbgPrint("\n%d ", node);
  
		//get type of node
		type_of_node = hda_get_node_type(codec_number, node);

		//process node
		//TODO: can i express this more compactly as a switch statement?

		if(type_of_node == HDA_WIDGET_AUDIO_OUTPUT) {
			DbgPrint( ("Audio Output"));

			//disable every audio output by connecting it to stream 0
			hda_send_verb(codec_number, node, 0x706, 0x00);
		}
		else if(type_of_node == HDA_WIDGET_AUDIO_INPUT) {
			DbgPrint( ("Audio Input"));
		}
		else if(type_of_node == HDA_WIDGET_AUDIO_MIXER) {
			DbgPrint( ("Audio Mixer"));
		}
		else if(type_of_node == HDA_WIDGET_AUDIO_SELECTOR) {
			DbgPrint( ("Audio Selector"));
		}
		else if(type_of_node == HDA_WIDGET_PIN_COMPLEX) {
			DbgPrint( ("Pin Complex "));

			//read type of PIN
			type_of_node = ((hda_send_verb(codec_number, node, 0xF1C, 0x00) >> 20) & 0xF);

			if(type_of_node == HDA_PIN_LINE_OUT) {
				DbgPrint( ("Line Out"));
	
				//save this node, this variable contain number of last alternative output
				pin_alternative_output_node_number = node;
			} else if(type_of_node == HDA_PIN_SPEAKER) {
				DbgPrint( ("Speaker "));

				//first speaker node is default speaker
				if(pin_speaker_default_node_number==0) {
					pin_speaker_default_node_number = node;
				}

				//find if there is device connected to this PIN
				if((hda_send_verb(codec_number, node, 0xF00, 0x09) & 0x4) == 0x4) {
					//find if it is jack or fixed device
					if((hda_send_verb(codec_number, node, 0xF1C, 0x00) >> 30) != 0x1) {
						//find if is device output capable
						if((hda_send_verb(codec_number, node, 0xF00, 0x0C) & 0x10) == 0x10) {
							//there is connected device
							DbgPrint( ("connected output device"));
							//we will use first pin with connected device, so save node number only for first PIN
							if(pin_speaker_node_number==0) {
								pin_speaker_node_number = node;
							}
						} else {
							DbgPrint( ("not output capable"));
						}
					} else {
						DbgPrint( ("not jack or fixed device"));
					}
				} else {
					DbgPrint( ("no output device"));
				}
			} else if(type_of_node == HDA_PIN_HEADPHONE_OUT) {
				DbgPrint( ("Headphone Out"));

				//save node number
				//TODO: handle if there are multiple HP nodes
				pin_headphone_node_number = node;
			} else if(type_of_node == HDA_PIN_CD) {
				DbgPrint( ("CD"));
	
				//save this node, this variable contain number of last alternative output
				if (useAltOut){
					pin_alternative_output_node_number = node;
					DbgPrint( (" used"));
				} else {
					DbgPrint( (" not considered"));
				}
			} else if(type_of_node == HDA_PIN_SPDIF_OUT) {
				DbgPrint( ("SPDIF Out"));
	
				//save this node, this variable contain number of last alternative output
				if (useSPDIF){
					pin_spdif_node_number = node;
					DbgPrint( (" used"));
				} else {
					DbgPrint( (" not considered"));
				}
			} else if(type_of_node == HDA_PIN_DIGITAL_OTHER_OUT) {
				DbgPrint( ("Digital Other Out"));
				//save this node, this variable contain number of last alternative output
				if (useAltOut){
					pin_alternative_output_node_number = node;
					DbgPrint( (" used"));
				} else {
					DbgPrint( (" not considered"));
				}
			} else if(type_of_node == HDA_PIN_MODEM_LINE_SIDE) {
				DbgPrint( ("Modem Line Side"));

				//save this node, this variable contain number of last alternative output
				if (useAltOut){
					pin_alternative_output_node_number = node;
					DbgPrint( (" used"));
				} else {
					DbgPrint( (" not considered"));
				}
			} else if(type_of_node == HDA_PIN_MODEM_HANDSET_SIDE) {
				DbgPrint( ("Modem Handset Side"));
	
				//save this node, this variable contain number of last alternative output
				if (useAltOut){
					pin_alternative_output_node_number = node;
					DbgPrint( (" used"));
				} else {
					DbgPrint( (" not considered"));
				}
			} else if(type_of_node == HDA_PIN_LINE_IN) {
				DbgPrint( ("Line In"));
			} else if(type_of_node == HDA_PIN_AUX) {
				DbgPrint( ("AUX"));
			} else if(type_of_node == HDA_PIN_MIC_IN) {
				DbgPrint( ("Mic In"));
			} else if(type_of_node == HDA_PIN_TELEPHONY) {
				DbgPrint( ("Telephony"));
			} else if(type_of_node == HDA_PIN_SPDIF_IN) {
				DbgPrint( ("SPDIF In"));
			} else if(type_of_node == HDA_PIN_DIGITAL_OTHER_IN) {
				DbgPrint( ("Digital Other In"));
			} else if(type_of_node == HDA_PIN_RESERVED) {
				DbgPrint( ("Reserved"));
			} else if(type_of_node == HDA_PIN_OTHER) {
				DbgPrint( ("Other"));
			}
		}
		//if it's not a PIN then what other type of node is it?
		else if(type_of_node == HDA_WIDGET_POWER_WIDGET) {
			DbgPrint( ("Power Widget"));
		} else if(type_of_node == HDA_WIDGET_VOLUME_KNOB) {
			DbgPrint( ("Volume Knob"));
		} else if(type_of_node == HDA_WIDGET_BEEP_GENERATOR) {
			DbgPrint( ("Beep Generator"));
		} else if(type_of_node == HDA_WIDGET_VENDOR_DEFINED) {
			DbgPrint( ("Vendor defined"));
		} else {
			DbgPrint( ("Reserved type"));
		}

		//log all connected nodes
		DbgPrint( (" "));
		UCHAR connection_entries_number = 0;
		ULONG connection_entries_node = hda_get_node_connection_entries(codec_number, node, 0);
		while (connection_entries_node != 0x0000) {
			DbgPrint( "%d ", connection_entries_node);
			connection_entries_number++;
			connection_entries_node = hda_get_node_connection_entries(codec_number, node, connection_entries_number);
		}
	}
	
	//reset variables of second path
	second_audio_output_node_number = 0;
	second_audio_output_node_sample_capabilities = 0;
	second_audio_output_node_stream_format_capabilities = 0;
	second_output_amp_node_number = 0;
	second_output_amp_node_capabilities = 0;

	//initialize output PINs
	// TODO: turn else-ifs to ifs and save more than 2 paths
	DbgPrint( ("\n"));
	if (pin_speaker_default_node_number != 0) {
		//initialize speaker
		is_initialized_useful_output = TRUE;
		if (pin_speaker_node_number != 0) {
			DbgPrint("\nSpeaker output ");
			hda_initialize_output_pin(pin_speaker_node_number); //initialize speaker with connected output device
			pin_output_node_number = pin_speaker_node_number; //save speaker node number
		}	
		else {
			DbgPrint("\nDefault speaker output ");
			hda_initialize_output_pin(pin_speaker_default_node_number); //initialize default speaker
			pin_output_node_number = pin_speaker_default_node_number; //save speaker node number
		}

		//save speaker path
		second_audio_output_node_number = audio_output_node_number;
		second_audio_output_node_sample_capabilities = audio_output_node_sample_capabilities;
		second_audio_output_node_stream_format_capabilities = audio_output_node_stream_format_capabilities;
		second_output_amp_node_number = output_amp_node_number;
		second_output_amp_node_capabilities = output_amp_node_capabilities;

		//add path to paths list
		path.audio_output_node_number = audio_output_node_number;
		path.audio_output_node_sample_capabilities = audio_output_node_sample_capabilities;
		path.audio_output_node_stream_format_capabilities = audio_output_node_stream_format_capabilities;
		path.output_amp_node_number = output_amp_node_number;
		path.output_amp_node_capabilities = output_amp_node_capabilities;
		if (out_paths.count < MAX_OUTPUT_PATHS) {
			out_paths.paths[out_paths.count++] = path;
		}

		//if codec has also headphone output, initialize it
		if (pin_headphone_node_number != 0) {
			DbgPrint("\n\nHeadphone output ");
			hda_initialize_output_pin(pin_headphone_node_number); //initialize headphone output
			pin_headphone_node_number = pin_headphone_node_number; //save headphone node number

			//if first path and second path share nodes, left only info for first path
			if(audio_output_node_number == second_audio_output_node_number) {
				second_audio_output_node_number = 0;
			}
			if(output_amp_node_number == second_output_amp_node_number) {
				second_output_amp_node_number = 0;
			}

			//find headphone connection status
			if(hda_is_headphone_connected() == TRUE) {
				hda_disable_pin_output(codec_number, pin_output_node_number);
				selected_output_node = pin_headphone_node_number;
			} else {
				selected_output_node = pin_output_node_number;
			}

			//check once for now
			hda_check_headphone_connection_change();

			//TODO: add task for checking headphone connection
			// this will have to be a DPC

			//create_task(hda_check_headphone_connection_change, TASK_TYPE_USER_INPUT, 50);

			//add path to paths list
			path.audio_output_node_number = audio_output_node_number;
			path.audio_output_node_sample_capabilities = audio_output_node_sample_capabilities;
			path.audio_output_node_stream_format_capabilities = audio_output_node_stream_format_capabilities;
			path.output_amp_node_number = output_amp_node_number;
			path.output_amp_node_capabilities = output_amp_node_capabilities;
			if (out_paths.count < MAX_OUTPUT_PATHS) {
				out_paths.paths[out_paths.count++] = path;
			}
		}
	}
	else if(pin_headphone_node_number != 0) { //codec do not have speaker, but only headphone output
		DbgPrint("\nHeadphone output selected ");
		is_initialized_useful_output = TRUE;
		hda_initialize_output_pin(pin_headphone_node_number); //initialize headphone output
		pin_output_node_number = pin_headphone_node_number; //save headphone node number
	}

	if (pin_alternative_output_node_number != 0) { //codec has alternative output
		DbgPrint("\nAlternative output selected ");
		//is_initialized_useful_output = FALSE;
		hda_initialize_output_pin(pin_alternative_output_node_number); //initialize alternative output
		pin_output_node_number = pin_alternative_output_node_number; //save alternative output node number
		//add path to paths list
		path.audio_output_node_number = audio_output_node_number;
		path.audio_output_node_sample_capabilities = audio_output_node_sample_capabilities;
		path.audio_output_node_stream_format_capabilities = audio_output_node_stream_format_capabilities;
		path.output_amp_node_number = output_amp_node_number;
		path.output_amp_node_capabilities = output_amp_node_capabilities;
		if (out_paths.count < MAX_OUTPUT_PATHS) {
			out_paths.paths[out_paths.count++] = path;
		}
	}

	if (pin_spdif_node_number != 0) { //codec has SPDIF output (display audio?)
		DbgPrint("\nSPDIF output selected ");
		//is_initialized_useful_output = FALSE;
		hda_initialize_output_pin(pin_spdif_node_number); //initialize SPDIF output
		pin_output_node_number = pin_spdif_node_number; //save SPDIF output node number
		//add path to paths list
		path.audio_output_node_number = audio_output_node_number;
		path.audio_output_node_sample_capabilities = audio_output_node_sample_capabilities;
		path.audio_output_node_stream_format_capabilities = audio_output_node_stream_format_capabilities;
		path.output_amp_node_number = output_amp_node_number;
		path.output_amp_node_capabilities = output_amp_node_capabilities;
		if (out_paths.count < MAX_OUTPUT_PATHS) {
			out_paths.paths[out_paths.count++] = path;
		}
	}
	DbgPrint("\n%d Output paths found", out_paths.count);
	if(out_paths.count == 0) {
		//no usable output paths have been found
		DbgPrint("\nCodec does not have any usable output PINs");
		return STATUS_UNSUCCESSFUL;
	}
	return STATUS_SUCCESS;
}

STDMETHODIMP_(UCHAR) CAdapterCommon::hda_get_node_type (ULONG codec, ULONG node){
	PAGED_CODE ();
	return (UCHAR) ((hda_send_verb(codec, node, 0xF00, 0x09) >> 20) & 0xF);
}

STDMETHODIMP_(ULONG) CAdapterCommon::hda_get_node_connection_entries (ULONG codec, ULONG node, ULONG connection_entries_number) {
	PAGED_CODE ();
	//read connection capabilities
	ULONG connection_list_capabilities = hda_send_verb(codec, node, 0xF00, 0x0E);
	
	//test if this connection even exists
	if(connection_entries_number >= (connection_list_capabilities & 0x7F)) {
		return 0x0000;
	}

	//return number of connected node
	if((connection_list_capabilities & 0x80) == 0x00) { //short form
		return ((hda_send_verb(codec, node, 0xF02, ((connection_entries_number/4)*4)) >> ((connection_entries_number%4)*8)) & 0xFF);
	}
	else { //long form
		return ((hda_send_verb(codec, node, 0xF02, ((connection_entries_number/2)*2)) >> ((connection_entries_number%2)*16)) & 0xFFFF);
	}
}

STDMETHODIMP_(void) CAdapterCommon::hda_enable_pin_output(ULONG codec, ULONG pin_node) {
	hda_send_verb(codec, pin_node, 0x707, (hda_send_verb(codec, pin_node, 0xF07, 0x00) | 0x40));
}

STDMETHODIMP_(void) CAdapterCommon::hda_disable_pin_output(ULONG codec, ULONG pin_node) {
	hda_send_verb(codec, pin_node, 0x707, (hda_send_verb(codec, pin_node, 0xF07, 0x00) & ~0x40));
}

STDMETHODIMP_(BOOLEAN) CAdapterCommon::hda_is_headphone_connected ( void ) {
	if (pin_headphone_node_number != 0
    && (hda_send_verb(codecNumber, pin_headphone_node_number, 0xF09, 0x00) & 0x80000000) == 0x80000000) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}

STDMETHODIMP_(void) CAdapterCommon::hda_initialize_output_pin ( ULONG pin_node_number) {
    PAGED_CODE ();

    NTSTATUS ntStatus = STATUS_SUCCESS;

    DOUT (DBG_PRINT, ("[CAdapterCommon::hda_initialize_output_pin] %d", pin_node_number));
	//reset variables of first path
	audio_output_node_number = 0;
	audio_output_node_sample_capabilities = 0;
	audio_output_node_stream_format_capabilities = 0;
	output_amp_node_number = 0;
	output_amp_node_capabilities = 0;

	//disable unsolicited responses
	hda_send_verb(codecNumber, pin_node_number, 0x708, 0x00);

	//disable any processing
	hda_send_verb(codecNumber, pin_node_number, 0x703, 0x00);

	//set 16-bit stereo format before turning on power so it sticks
	hda_send_verb(codecNumber, pin_node_number, 0x200, 0x11);

	//turn on power for PIN
	hda_send_verb(codecNumber, pin_node_number, 0x705, 0x00);

	//enable PIN
	hda_send_verb(codecNumber, pin_node_number, 0x707, (hda_send_verb(codecNumber, pin_node_number, 0xF07, 0x00) | 0x80 | 0x40));

	//enable EAPD. do not enable L-R swap, why were we doing that, the channel order is correct
	hda_send_verb(codecNumber, pin_node_number, 0x70C, 0x0002);

	//set maximal volume for PIN
	ULONG pin_output_amp_capabilities = hda_send_verb(codecNumber, pin_node_number, 0xF00, 0x12);
	hda_set_node_gain(codecNumber, pin_node_number, HDA_OUTPUT_NODE, pin_output_amp_capabilities, 250, 3);
	if(pin_output_amp_capabilities != 0) {
		//we will control volume by PIN node
		output_amp_node_number = pin_node_number;
		output_amp_node_capabilities = pin_output_amp_capabilities;
	}

	//start enabling path of nodes
	length_of_node_path = 0;
	hda_send_verb(codecNumber, pin_node_number, 0x701, 0x00); //select first node
	ULONG first_connected_node_number = hda_get_node_connection_entries(codecNumber, pin_node_number, 0); //get first node number
	ULONG type_of_first_connected_node = hda_get_node_type(codecNumber, first_connected_node_number); //get type of first node
	if(type_of_first_connected_node==HDA_WIDGET_AUDIO_OUTPUT) {
		hda_initialize_audio_output(first_connected_node_number);
	}
	else if(type_of_first_connected_node == HDA_WIDGET_AUDIO_MIXER) {
		hda_initialize_audio_mixer(first_connected_node_number);
	}
	else if(type_of_first_connected_node == HDA_WIDGET_AUDIO_SELECTOR) {
		hda_initialize_audio_selector(first_connected_node_number);
	}
	else {
		DOUT (DBG_PRINT, ("\nHDA ERROR: PIN have connection %d", first_connected_node_number));
	}
}

STDMETHODIMP_(void) CAdapterCommon::hda_initialize_audio_output(ULONG output_node_number) {
	PAGED_CODE ();
	DOUT (DBG_PRINT, ("Initializing Audio Output %d", output_node_number));
	audio_output_node_number = output_node_number;

	//disable unsolicited responses
	hda_send_verb(codecNumber, output_node_number, 0x708, 0x00);

	//disable any processing
	hda_send_verb(codecNumber, output_node_number, 0x703, 0x00);

	//set 16-bit stereo format before turning on power so it sticks
	hda_send_verb(codecNumber, output_node_number, 0x200, 0x11);

	//turn on power for Audio Output
	hda_send_verb(codecNumber, output_node_number, 0x705, 0x00);

	//connect Audio Output to stream 1 channel 0
	hda_send_verb(codecNumber, output_node_number, 0x706, 0x10);

	//set maximum volume for Audio Output
	ULONG audio_output_amp_capabilities = hda_send_verb(codecNumber, output_node_number, 0xF00, 0x12);
	hda_set_node_gain(codecNumber, output_node_number, HDA_OUTPUT_NODE, audio_output_amp_capabilities, 250, 3);
	if(audio_output_amp_capabilities != 0) {
		//we will control volume by Audio Output node
		output_amp_node_number = output_node_number;
		output_amp_node_capabilities = audio_output_amp_capabilities;
	}

	//read info, if something is not present, take it from AFG node
	ULONG audio_output_sample_capabilities = hda_send_verb(codecNumber, output_node_number, 0xF00, 0x0A);
	if(audio_output_sample_capabilities==0) {
		audio_output_node_sample_capabilities = afg_node_sample_capabilities;
	}
	else {
		audio_output_node_sample_capabilities = audio_output_sample_capabilities;
	}
	ULONG audio_output_stream_format_capabilities = hda_send_verb(codecNumber, output_node_number, 0xF00, 0x0B);
	if(audio_output_stream_format_capabilities==0) {
		audio_output_node_stream_format_capabilities = afg_node_stream_format_capabilities;
	}
	else {
		audio_output_node_stream_format_capabilities = audio_output_stream_format_capabilities;
	}
	if(output_amp_node_number==0) { //if nodes in path do not have output amp capabilities, volume will be controlled by Audio Output node with capabilities taken from AFG node
		output_amp_node_number = output_node_number;
		output_amp_node_capabilities = afg_node_output_amp_capabilities;
	}

	//because we are at end of node path, log all gathered info
	DOUT (DBG_PRINT, ("Sample Capabilites: 0x%x", audio_output_node_sample_capabilities));
	DOUT (DBG_PRINT, ("Stream Format Capabilites: 0x%x", audio_output_node_stream_format_capabilities));
	DOUT (DBG_PRINT, ("Volume node: %d", output_amp_node_number));
	DOUT (DBG_PRINT, ("Volume capabilities: 0x%x", output_amp_node_capabilities));
}

STDMETHODIMP_(void) CAdapterCommon::hda_initialize_audio_mixer(ULONG audio_mixer_node_number) {
	PAGED_CODE ();
	if(length_of_node_path>=10) {
		DOUT (DBG_PRINT,("HDA ERROR: too long path"));
		return;
	}
	DOUT (DBG_PRINT,("Initializing Audio Mixer %d", audio_mixer_node_number));

	//set 16-bit stereo format before turning on power so it sticks
	hda_send_verb(codecNumber, audio_mixer_node_number, 0x200, 0x11);

	//turn on power for Audio Mixer
	hda_send_verb(codecNumber, audio_mixer_node_number, 0x705, 0x00);

	//disable unsolicited responses
	hda_send_verb(codecNumber, audio_mixer_node_number, 0x708, 0x00);

	//set maximal volume for Audio Mixer
	ULONG audio_mixer_amp_capabilities = hda_send_verb(codecNumber, audio_mixer_node_number, 0xF00, 0x12);
	hda_set_node_gain(codecNumber, audio_mixer_node_number, HDA_OUTPUT_NODE, audio_mixer_amp_capabilities, 250, 3);
	if(audio_mixer_amp_capabilities != 0) {
		//we will control volume by Audio Mixer node
		output_amp_node_number = audio_mixer_node_number;
		output_amp_node_capabilities = audio_mixer_amp_capabilities;
	}

	//continue in path
	length_of_node_path++;
	ULONG first_connected_node_number = hda_get_node_connection_entries(codecNumber, audio_mixer_node_number, 0); //get first node number
	ULONG type_of_first_connected_node = hda_get_node_type(codecNumber, first_connected_node_number); //get type of first node
	if(type_of_first_connected_node == HDA_WIDGET_AUDIO_OUTPUT) {
		hda_initialize_audio_output(first_connected_node_number);
	}
	else if(type_of_first_connected_node == HDA_WIDGET_AUDIO_MIXER) {
		hda_initialize_audio_mixer(first_connected_node_number);
	}
	else if(type_of_first_connected_node == HDA_WIDGET_AUDIO_SELECTOR) {
		hda_initialize_audio_selector(first_connected_node_number);
	}
	else {
		DOUT (DBG_PRINT,("HDA ERROR: Mixer have connection %d", first_connected_node_number));
	}
}

STDMETHODIMP_(void) CAdapterCommon::hda_initialize_audio_selector(ULONG audio_selector_node_number) {
	PAGED_CODE ();
	if(length_of_node_path>=10) {
		DOUT (DBG_PRINT,("HDA ERROR: too long path"));
	return;
	}
	DOUT (DBG_PRINT,("Initializing Audio Selector %d", audio_selector_node_number));

	//set 16-bit stereo format before turning on power so it sticks
	hda_send_verb(codecNumber, audio_selector_node_number, 0x200, 0x11);

	//turn on power for Audio Selector
	hda_send_verb(codecNumber, audio_selector_node_number, 0x705, 0x00);

	//disable unsolicited responses
	hda_send_verb(codecNumber, audio_selector_node_number, 0x708, 0x00);

	//disable any processing
	hda_send_verb(codecNumber, audio_selector_node_number, 0x703, 0x00);

	//set maximal volume for Audio Selector
	ULONG audio_selector_amp_capabilities = hda_send_verb(codecNumber, audio_selector_node_number, 0xF00, 0x12);
	hda_set_node_gain(codecNumber, audio_selector_node_number, HDA_OUTPUT_NODE, audio_selector_amp_capabilities, 250, 3);
	if(audio_selector_amp_capabilities != 0) {
		//we will control volume by Audio Selector node
		output_amp_node_number = audio_selector_node_number;
		output_amp_node_capabilities = audio_selector_amp_capabilities;
	}
	
	//continue in path
	length_of_node_path++;
	hda_send_verb(codecNumber, audio_selector_node_number, 0x701, 0x00); //select first node
	ULONG first_connected_node_number = hda_get_node_connection_entries(codecNumber, audio_selector_node_number, 0); //get first node number
	ULONG type_of_first_connected_node = hda_get_node_type(codecNumber, first_connected_node_number); //get type of first node
	if(type_of_first_connected_node == HDA_WIDGET_AUDIO_OUTPUT) {
		hda_initialize_audio_output(first_connected_node_number);
	}
	else if(type_of_first_connected_node == HDA_WIDGET_AUDIO_MIXER) {
		hda_initialize_audio_mixer(first_connected_node_number);
	}
	else if(type_of_first_connected_node == HDA_WIDGET_AUDIO_SELECTOR) {
		hda_initialize_audio_selector(first_connected_node_number);
	}
	else {
		DOUT (DBG_PRINT,("HDA ERROR: Selector have connection %d", first_connected_node_number));
	}
}

//***End of pageable code!***
//everything above this must have PAGED_CODE ();
#pragma code_seg() 

STDMETHODIMP_(void) CAdapterCommon::hda_check_headphone_connection_change(void) {
	//TODO: schedule as a periodic task 
	//and make sure to clean up correctly on driver unload!
	if(selected_output_node == pin_output_node_number && hda_is_headphone_connected() == TRUE) { //headphone was connected
		hda_disable_pin_output(codecNumber, pin_output_node_number);
		selected_output_node = pin_headphone_node_number;
	}
	else if(selected_output_node == pin_headphone_node_number && hda_is_headphone_connected()==FALSE) { //headphone was disconnected
		hda_enable_pin_output(codecNumber, pin_output_node_number);
		selected_output_node = pin_output_node_number;
	}
}

STDMETHODIMP_(ULONG) CAdapterCommon::hda_send_verb(ULONG codec, ULONG node, ULONG verb, ULONG command) {
	//DOUT (DBG_PRINT, ("[CAdapterCommon::hda_send_verb]"));

	//TODO: check sizes of components passed in
	//note that verbs and parameters are variable length so verbs are left aligned in the fields here
	//verbs which take a 16 bit payload will be shifted left like verb 2h will be verb 0x200 in this code. 

	//TODO: check for unsolicited responses and maybe schedule a DPC to deal with them
	//TODO: check for responses with high bit set (those are the errors)
	
	KIRQL oldirql;
	ULONG value = ((codec<<28) | (node<<20) | (verb<<8) | (command));
	//DOUT (DBG_SYSINFO, ("Write codec verb 0x%X position %d", value, CorbPointer));
	if (communication_type == HDA_CORB_RIRB) {

		ULONG response = STATUS_UNSUCCESSFUL;
		
		//CORB/RIRB interface

		//acquire spin lock: this isnt great to do as well as blocking but what can i do
		KeAcquireSpinLock(&QLock, &oldirql);

		//get current rirb pointer temp
		USHORT RirbTmp = readUSHORT (0x58);
		BOOLEAN valid = FALSE;
 
		//write verb
		WRITE_REGISTER_ULONG(CorbMemVirt + (CorbPointer), value);
  
		//move write pointer
		writeUSHORT(0x48, CorbPointer);
  
		//wait for RIRB pointer to increment/wrap

		for(ULONG ticks = 0; ticks < 600; ++ticks) {
			KeStallExecutionProcessor(10);
			if(readUSHORT (0x58) != RirbTmp) {
				valid = TRUE;
				break;
			} else if (ticks == 599) {
				//6 ms and no movement
				DOUT (DBG_ERROR, ("No Response to Codec Verb"));
				//communication_type = HDA_UNINITIALIZED;
			}
		}
		if (valid){

			//read response. each response is 8 bytes long but we only care about the lower 32 bits
			response = READ_REGISTER_ULONG (RirbMemVirt + (RirbPointer * 2));

			//move RIRB pointer **only if we got a response**

			//TODO: if we have usolicited responses on there may be more than 1 difference
			//between last read and last written
			RirbPointer++;
			if(RirbPointer == RirbNumberOfEntries) {
				RirbPointer = 0;
			}
		} 
		
		//move corb pointer
		CorbPointer++;
		if(CorbPointer == CorbNumberOfEntries) {
			CorbPointer = 0;
		}
		//unlock! must get here!
		KeReleaseSpinLock(&QLock, oldirql);

		//return response whatever it is. single point of exit
		return response;

	} else if (communication_type == HDA_PIO){
		
		//Immediate command interface
		//acquire spin lock: this isnt great to do as well as blocking but what can ya do
		KeAcquireSpinLock(&QLock, &oldirql);

		//DOUT (DBG_SYSINFO, ("Write codec verb Immediate:0x%X", value));
		//clear Immediate Result Valid bit
		writeUSHORT(0x68, 0x2);

		//write verb
		writeULONG(0x60, value);

		//start verb transfer
		writeUSHORT(0x68, 0x1);

		//poll for response
		ULONG ticks = 0;
		while (++ticks < 600) {
			KeStallExecutionProcessor(10);

			//wait for Immediate Result Valid bit = set and Immediate Command Busy bit = clear
			if ((readUSHORT(0x68) & 0x3) == 0x2) {
				//clear Immediate Result Valid bit
				writeUSHORT(0x68, 0x2);
				
				ULONG response = readULONG(0x64);
				//unlock!
				KeReleaseSpinLock(&QLock, oldirql);
				//return response
				return response;
			}
		}
  
		//there was no response after 6 ms
		//unlock
		KeReleaseSpinLock(&QLock, oldirql);
		DOUT (DBG_ERROR, ("\nHDA PIO ERROR: no response"));

		communication_type = HDA_UNINITIALIZED;
		return STATUS_UNSUCCESSFUL;
	} else {
		return STATUS_UNSUCCESSFUL;
	}
}