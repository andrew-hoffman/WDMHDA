# WDMHDA
HD Audio driver for Windows 98SE / ME

This does mostly function as a HD Audio controller and codec driver, in VMWare and Virtualbox ONLY so far. Consider it a MVP Proof of Concept; further development is needed to function with basically any real hardware. Testing and feedback from anyone who can run this on real hardware with a [kernel debugger](https://bikodbg.com/blog/2021/08/win98-ddk/) will be appreciated. May experience possible horrible screeching and popping noises or static, and possible hard freezes when the driver is loaded or unloaded.

Current Limitations:
- Only supports 8000-48000hz 16-bit sample rate (up to 192khz 24-bit and 32-bit could technically be added but what's the point for 9x?)
- Output only, recording not supported
- Single audio stream, no hardware mixing
- Volume and mixer controls are mostly unimplemented
- Jack detection and retasking is not supported
- Freezes, fails to start or outputs horrible noises on most real hardware. Intel controller / Realtek codec *MIGHT* work.

Source Code from [Microsoft's driver samples](https://github.com/microsoftarchive/msdn-code-gallery-microsoft/tree/master/Official%20Windows%20Driver%20Kit%20Sample/Windows%20Driver%20Kit%20(WDK)%208.1%20Samples/%5BC%2B%2B%5D-windows-driver-kit-81-cpp/WDK%208.1%20C%2B%2B%20Samples/AC97%20Driver%20Sample/C%2B%2B)

and [BleskOS](https://github.com/VendelinSlezak/BleskOS/blob/master/source/drivers/sound/hda.c)
used under MIT license.

See also [Dogbert's open source CMI driver](https://codesite-archive.appspot.com/archive/p/cmediadrivers/).
