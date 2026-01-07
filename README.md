# WDMHDA
HD Audio driver for Windows 98SE / ME

This project is a High Definition Audio aka Azalia codec and controller driver. It's for Intel 915 and later chipsets motherboard onboard audio that's not AC97. 
Designed for all versions of Windows with WDM support  Windows 98SE and ME (and possibly 98FE and 2000 as well, but not tested there yet.) 

Current status is a MVP Proof of Concept that functions in VMware and VirtualBox, and on Intel controllers with a Realtek ALC2xx; further development is needed to support more real hardware. Testing and feedback from anyone who can run this on bare metal with a [kernel debugger](https://bikodbg.com/blog/2021/08/win98-ddk/) will be appreciated. May experience possible horrible screeching and popping noises or static, and possible hard freezes when the driver is loaded or unloaded. If you want to use this in some kind of business critical production I would highly recommend either a Sound Blaster Live, CMI8738 or any $2 USB Audio dongle instead. (Seriously, almost all of the cheapest USB dongles work perfectly in 98se/Me. If you're using QuickInstall 0.9.6 you will need to add the wdma_usb.inf file back to C:\Windows\Inf though)

Windows 9x may need to be patched to function at all on modern hardware even when virtualized. See [JHRobotics' Patcher9x project](https://github.com/JHRobotics/patcher9x) which now includes [Sweetlow's patch for memory resource conflict issues](https://msfn.org/board/topic/186768-bug-fix-vmmvxd-on-handling-4gib-addresses-and-description-of-problems-with-resource-manager-on-newer-bioses/). 

## Installation:

Install HDA.inf with device manager on the Unknown Device which shows up as "PCI Card" with class code 0403. Select the location of the HDA.sys file when Windows asks you. The release build of the driver is in the buildfre\i386 folder, the debug build is in buildchk\i386. 

## Current Limitations:

- Only supports 8000-48000hz 16-bit sample rate (up to 192khz 24-bit and 32-bit could technically be added but what's the point for 9x?)
- Output only, recording not supported
- Single audio stream, no hardware mixing
- Volume control is only implemented for the main mix output
- Jack detection and retasking is not supported
- Only initializes the first codec detected on the link (extra codec in laptop docking stations won't work)
- Freezes, fails to start or outputs horrible noises on a lot of real hardware. No guarantees. 

Source Code from [Microsoft's driver samples](https://github.com/microsoftarchive/msdn-code-gallery-microsoft/tree/master/Official%20Windows%20Driver%20Kit%20Sample/Windows%20Driver%20Kit%20(WDK)%208.1%20Samples/%5BC%2B%2B%5D-windows-driver-kit-81-cpp/WDK%208.1%20C%2B%2B%20Samples/AC97%20Driver%20Sample/C%2B%2B)

and [BleskOS](https://github.com/VendelinSlezak/BleskOS/blob/master/source/drivers/sound/hda.c)
used under MIT license.

See also [Dogbert's open source CMI driver](https://codesite-archive.appspot.com/archive/p/cmediadrivers/).

for build instructions see Build Instructions.txt
