# WDMHDA
HD Audio driver for Windows 98SE / ME

This project is a High Definition Audio aka Azalia codec and controller driver. It's for Intel 915 and later chipsets motherboard onboard audio that's not AC97.

It is designed for all versions of Windows with Windows Driver Model (WDM) support, but only Windows 98 SE and ME are officially supported currently.

Windows 98 First Edition may work but there are known issues with sample rate and but depth conversion not working, and because Microsoft doesn't recommend WDM audio drivers for this version it will be unsupported.

As of Version Alpha-019 the driver functions on Windows 2000/XP, though as these OSes already have the oficial KB888111 HDA Bus driver update, they are not the primary target.

Current status of this driver is an Alpha. It is known to function in VMware and VirtualBox, and on many Intel, AMD & VIA chipset HD Audio controllers with Realtek codecs; further development and testing is needed to support more real hardware. Nvidia chipsets, and IDT, Analog Devices, Cirrus Logic and VIA codecs, are not well supported yet.

This driver is dependent on the BIOS Pin Configuration defaults for selecting a sensible combination of outputs and there are no overrides yet for buggy BIOSes. You may experience garbled or glitchy audio, possible horrible screeching and popping noises or static, or complete silence, as well as possible hard freezes when the driver is loaded or unloaded. 

If you want to use this in some kind of business critical production application, I would highly recommend using a Sound Blaster Live, CMI8738 or any $2 USB Audio dongle instead. (Seriously, almost all of the cheapest USB Audio 1.0 class adapters work perfectly in 98se/Me.)

Windows 9x may need to be patched to function at all on modern hardware and > 512mb of RAM even when virtualized. For Intel 12th gen and newer this is Mandatory. See [JHRobotics' Patcher9x project](https://github.com/JHRobotics/patcher9x) which now includes [Sweetlow's patch for memory resource conflict issues](https://msfn.org/board/topic/186768-bug-fix-vmmvxd-on-handling-4gib-addresses-and-description-of-problems-with-resource-manager-on-newer-bioses/). Or for a prepatched solution, try [Windows 98 QuickInstall from Oerg866](https://github.com/oerg866/win98-quickinstall/releases).

## Installation:

Install HDA.inf with Device Manager on the HD Audio Controller device. This will be listed on Windows 9x as a "PCI Card" with class code 0403 (you can run hwinfo /gui to see the vendor /device info on unknown devices).
On Windows 2000/XP the HDA controller will be listed as "Microsoft UAA Bus Device" or similar if you have the official KB888111 update installed.
Do not install on the HD Audio Codec device (this will have a device ID string that starts with "HDAUDIO\") as this will not function.

You may get a dialog asking "A file being copied is older than the file currently in use" for ksuser.dll and stream.sys. **Always keep the newer file.**

For best results, go to the Multimedia control panel, Click the Advanced Properties button for the Playback device, go to the Performance tab and set Audio Acceleration to Standard (one notch to the left of Full) and Sample Rate Conversion Quality to Best (all the way to the right).

It is recommended but not strictly necessary to install DirectX 8.1 or newer after installing this driver.

## Current Limitations:

- Only supports 16-bit stereo audio at 22-48khz sample rates (up to 96khz 32-bit could technically be added but 9x doesn't make the best choices about resampling)
- Playback only, recording is not supported
- Single audio stream, no hardware mixing
- Audio latency is ~40 ms at best. This is a kernel limit
- Volume control is only implemented for the main mix output
- Jack detection and retasking is not supported
- Analog CD audio is not mixed into the audio output. Digital Audio Extraction is supported.
- DOS Sound Blaster emulation is provided by Microsoft's WDM emulator `SBEMUL.SYS` which only supports 8-bit stereo digital sound and General MIDI but does not emulate OPL. DOSBox can be used to run games with OPL emulation and there is a patched version of SBEMUL available from SweetLow which will support 16-bit digital sound (high DMA the same as low DMA). 
- Freezes, crashes, fails to start or outputs horrible noises on some real hardware. No guarantees. 

Source Code from [Microsoft's driver samples](https://github.com/microsoftarchive/msdn-code-gallery-microsoft/tree/master/Official%20Windows%20Driver%20Kit%20Sample/Windows%20Driver%20Kit%20(WDK)%208.1%20Samples/%5BC%2B%2B%5D-windows-driver-kit-81-cpp/WDK%208.1%20C%2B%2B%20Samples/AC97%20Driver%20Sample/C%2B%2B)

and [BleskOS](https://github.com/VendelinSlezak/BleskOS/blob/master/source/drivers/sound/hda.c)
used under MIT license.

See also [Dogbert's open source CMI driver](https://codesite-archive.appspot.com/archive/p/cmediadrivers/).

For build instructions, see the file Build Instructions.txt

Testing and feedback from anyone who can run this on bare metal with a [kernel debugger](https://bikodbg.com/blog/2021/08/win98-ddk/) will be appreciated. 

The release build of the driver is in the buildfre\i386 folder, the debug build is in buildchk\i386.

## If the driver will not start for you or has other issues here's how you can get a debug log:
 
- Find and install Sysinternals Debug View. I think a version that works on Win98 is here https://www.digiater.nl/openvms/decus/vmslt00a/nt/dbgview.htm
- Install the debug version of my driver by using the HDA.sys which is in the objchk\i386 folder. you can just copy this into C:\Windows\System32\Drivers if the driver was already installed 
- Disable the HD audio controller device in Device Manager
- Restart
- Open DebugView
- Enable the HD audio controller device
- Watch the debug messages come in, they should explain why the driver is failing to start 
- Save the log file from Debug View and post it to an issue or discussion thread here. 

## AI Disclaimer:

Generative AI (Large Language Models) have been used for research and debugging help, and writing small amounts of the code (C++ class and interface definitions, some memory safety fixes). I do not intend to make this a completely "Vibe Coded" project; all bugs are the fault and responsibility of the humans that submit the code. Pull requests automatically generated by a LLM tool will not be accepted. The same applies if the author cannot answer questions about their pull request. You must understand what you are adding to the project. Driver code is not forgiving. 

This software is supplied with NO WARRANTY, express or implied. See the MIT LICENSE file for more information. For support, please file an issue on Github or refer to the complaints department of the Sirius Cybernetics Corporation. 
