# Build Instructions

This project must be built with Visual C++ 6.0 and the Windows 2000 DDK in a 32-bit Windows environment. Use a Virtual Machine if needed.

The source tree MUST be checked out with CRLF line endings or the Visual Studio workspace and project files will break; the .gitattributes file *should* enforce this.

1. Create a Windows 98se, 2000 or XP VM in VMWare or Virtualbox
	Real hardware will work too if you have an older system with these OSes
	May build on newer Windows versions or Wine but I have not tested this.

2. Install Visual C++ 6.0 Enterprise or Professional (No service packs) with all options

3. Install the Windows 2000 DDK (Driver Development Kit). 
	Include at least the Audio samples, General Driver samples and General WDM samples but you might as well install everything

4. Copy the source tree to any folder under `C:\NTDDK\src\wdm\audio\`
	I use `C:\NTDDK\src\wdm\audio\WDMHDA.git`
5. Open a "Checked Build Environment" or "Free Build Environment" command prompt 
	(Start > Programs > Development Kits > Windows 2000 DDK)
	checked builds include debug prints, asserts and extra memory bounds checking.

6. CD to the `C:\NTDDK\src\wdm\audio\` directory and run `build -cZ`
	This will build all the sample audio drivers because this project needs stdunk and the headers from it (for the CUnknown class and IUnknown interface).
	After the first build, you can build in the WDMHDA\src subdirectory with b.bat (run from the DDK build environment command prompt).

`HDA.sys` will be built in the `src\objfre\i386` or `src\objchk\i386` folders.

Install it on the "PCI Card" device in Device Manager using the "Have Disk" option with `hda.inf`

To set up a kernel debugger see https://bikodbg.com/blog/2021/08/win98-ddk/
You can also receive debug messages using Sysinternals DebugView for Windows 98
you will have to disable the driver, restart, and enable again to read the initialization messages.


## Quick Guide to source files:
 
Code adapted from the Win2k DDK AC97 and SB16 sample drivers
Also adapted from BleskOS (2025u10 branch), MPXPlay, FreeBSD, MSDN references
Sources are noted in the file headers.

| File		| Description							|
|---------------|---------------------------------------------------------------|
| adapter.cpp	| Connects the driver with the system				|
| codec.cpp 	| Object for each Codec device on HDA link			|
| codec.h 	| Codec object header						|
| common.cpp	| Common object used by all miniports, and HDA controller setup |
| common.h 	| Header file for the common object				|
| debug.h	| Debug print definition					|
| hda_vendor.h	| PCI Vendor and Device IDs					|
| makefile	| Standard Windows NT makefile					|
| mintopo.cpp	| Implementation of the topology miniport			|
| mintopo.h	| Header file for the topology miniport				|
| minwave.cpp	| Implementation of the wave cyclic miniport and stream object	|
| minwave.h	| Header file for the wave cyclic miniport and stream object	|
| mydma.h	| DMA channel wrapper for alignment and cache flushing		|
| tables.h	| Topology and property tables					|
| HDA.inf	| Setup information						|
| HDA.rc	| Resource file containing version information			|
| sources	| Dependency information for compiling (MSBUILD)		|
| b.bat		| Build command							|
| c.bat		| Copies driver into system32\Drivers				|

