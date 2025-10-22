# WDMHDA
HD Audio driver for Windows 98SE / ME

This does mostly function as an audio driver, on Virtualbox ONLY, if you don't mind occasional horrible screeching and popping noises. Consider it a MVP Proof of Concept; further development is needed to function with basically any real hardware. Testing and feedback from anyone who can run this on real hardware with a kernel debugger will be appreciated.

Current Limitations:
- Only supports 44100hz 16-bit sample rate
- Output only, recording not supported
- Volume control and mixer are mostly broken
- Jack detection and retasking is not supported
- Freezes or fails to start on most real hardware.

Source Code from [Microsoft's driver samples](https://github.com/microsoftarchive/msdn-code-gallery-microsoft/tree/master/Official%20Windows%20Driver%20Kit%20Sample/Windows%20Driver%20Kit%20(WDK)%208.1%20Samples/%5BC%2B%2B%5D-windows-driver-kit-81-cpp/WDK%208.1%20C%2B%2B%20Samples/AC97%20Driver%20Sample/C%2B%2B)

and [BleskOS](https://github.com/VendelinSlezak/BleskOS/blob/master/source/drivers/sound/hda.c)
used under MIT license.

See also [Dogbert's open source CMI driver](https://codesite-archive.appspot.com/archive/p/cmediadrivers/).
