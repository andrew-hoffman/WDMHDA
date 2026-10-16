# Microsoft Developer Studio Project File - Name="HDA" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=HDA - Win32 Checked
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "HDA.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "HDA.mak" CFG="HDA - Win32 Checked"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "HDA - Win32 Free" (based on "Win32 (x86) Application")
!MESSAGE "HDA - Win32 Checked" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "HDA - Win32 Free"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "objfre\i386"
# PROP BASE Intermediate_Dir "objfre\i386"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "objfre\i386"
# PROP Intermediate_Dir "objfre\i386"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /Gz /W3 /Z7 /O2 /I "..\..\inc" /I "$(ddkpath)\inc" /I "$(ddkpath)\inc\ddk" /I "$(ddkpath)\inc\ddk\wdm" /FI"$(ddkpath)\inc\warning.h" /D CONDITION_HANDLING=1 /D DBG=0 /D DEVL=1 /D "DRIVER" /D FPO=1 /D "NDEBUG" /D NT_INST=0 /D NT_UP=1 /D "STD_CALL" /D "UNDER_NT" /D "UNICODE" /D "WIN32" /D WIN32=100 /D WIN32_LEAN_AND_MEAN=1 /D WINNT=1 /D WINVER=0x0500 /D _DLL=1 /D "_IDWBUILD" /D "_MBCS" /D _NT1X_=100 /D "_UNICODE" /D "_WIN32" /D _WIN32_IE=0x0501 /D _WIN32_WINNT=0x0500 /D "_WINDOWS" /D _X86_=1 /D i386=1 /FR /YX /FD -GF -Oxs -QI0f -QIf -QIfdiv- -Zel -cbstring /c
# ADD CPP /nologo /Gz /W3 /Z7 /O2 /I "..\..\inc" /I "$(ddkpath)\inc" /I "$(ddkpath)\inc\ddk" /I "$(ddkpath)\inc\ddk\wdm" /FI"$(ddkpath)\inc\warning.h" /D CONDITION_HANDLING=1 /D DBG=0 /D DEVL=1 /D "DRIVER" /D FPO=1 /D "NDEBUG" /D NT_INST=0 /D NT_UP=1 /D "STD_CALL" /D "UNDER_NT" /D "UNICODE" /D "WIN32" /D WIN32=100 /D WIN32_LEAN_AND_MEAN=1 /D WINNT=1 /D WINVER=0x0500 /D _DLL=1 /D "_IDWBUILD" /D "_MBCS" /D _NT1X_=100 /D "_UNICODE" /D "_WIN32" /D _WIN32_IE=0x0501 /D _WIN32_WINNT=0x0500 /D "_WINDOWS" /D _X86_=1 /D i386=1 /FR /YX /FD -GF -Oxs -QI0f -QIf -QIfdiv- -Zel -cbstring /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /i "..\..\inc" /i "$(ddkpath)\inc" /i "$(ddkpath)\inc\ddk" /i "$(ddkpath)\inc\ddk\wdm" /d "NDEBUG"
# ADD RSC /l 0x409 /i "..\..\inc" /i "$(ddkpath)\inc" /i "$(ddkpath)\inc\ddk" /i "$(ddkpath)\inc\ddk\wdm" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 wdm.lib wmilib.lib ..\..\lib\*\stdunk.lib /nologo /base:"0x10000" /version:5.0 /stack:0x40000,0x1000 /entry:"DriverEntry@8" /machine:IX86 /nodefaultlib /out:"objfre\i386\hda.sys" /libpath:"$(ddkpath)\libfre\i386" -align:0x20 -debug:full -driver:wdm -force:multiple -fullbuild -ignore:4001,4037,4039,4049,4044,4065,4070,4078,4087,4089,4198 -merge:_PAGE=PAGE -merge:_TEXT=.text -nocomment -opt:icf -opt:ref -optidata -osversion:4.00 -release -section:init,d -subsystem:native,4.00
# ADD LINK32 wdm.lib wmilib.lib portcls.lib ks.lib ..\..\lib\i386\stdunk.lib /nologo /base:"0x10000" /version:5.0 /stack:0x40000,0x1000 /entry:"DriverEntry@8" /map /machine:IX86 /nodefaultlib /out:"objfre\i386\hda.sys" /libpath:"$(ddkpath)\libfre\i386" -align:0x20 -debug:full -driver:wdm -force:multiple -fullbuild -ignore:4001,4037,4039,4049,4044,4065,4070,4078,4087,4089,4198 -merge:_PAGE=PAGE -merge:_TEXT=.text -nocomment -opt:icf -opt:ref -optidata -osversion:4.00 -release -section:init,d -subsystem:native,4.00
# Begin Custom Build - Finishing up...
IntDir=.\objfre\i386
TargetPath=.\objfre\i386\hda.sys
TargetName=hda
InputPath=.\objfre\i386\hda.sys
SOURCE="$(InputPath)"

"$(IntDir)\$(TargetName).nms" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	$(SIWPATH)\nmsym -translate:source,package,always $(TargetPath) 
	rebase -b 10000 -x junk $(TargetPath) 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "HDA - Win32 Checked"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "HDA___Win32_Debug"
# PROP BASE Intermediate_Dir "HDA___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "objchk\i386"
# PROP Intermediate_Dir "objchk\i386"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /Gz /W3 /Z7 /Oi /Gy /I "..\..\inc" /I "$(ddkpath)\inc" /I "$(ddkpath)\inc\ddk" /I "$(ddkpath)\inc\ddk\wdm" /FI"$(ddkpath)\inc\warning.h" /D CONDITION_HANDLING=1 /D DBG=1 /D DEVL=1 /D "DRIVER" /D FPO=0 /D "NDEBUG" /D NT_INST=0 /D NT_UP=1 /D "RDRDBG" /D "SRVDBG" /D "STD_CALL" /D "UNDER_NT" /D "UNICODE" /D "WIN32" /D WIN32=100 /D WIN32_LEAN_AND_MEAN=1 /D WINNT=1 /D WINVER=0x0500 /D "_DEBUG" /D _DLL=1 /D "_IDWBUILD" /D "_MBCS" /D _NT1X_=100 /D "_UNICODE" /D "_WIN32" /D _WIN32_IE=0x0501 /D _WIN32_WINNT=0x0500 /D "_WINDOWS" /D _X86_=1 /D i386=1 /FR"debug\\" /YX /FD -GF -QI0f -QIf -QIfdiv- -Zel -cbstring /c
# ADD CPP /nologo /Gz /W3 /Z7 /Oi /Gy /I "..\..\inc" /I "$(ddkpath)\inc" /I "$(ddkpath)\inc\ddk" /I "$(ddkpath)\inc\ddk\wdm" /FI"$(ddkpath)\inc\warning.h" /D CONDITION_HANDLING=1 /D DBG=1 /D DEVL=1 /D "DRIVER" /D FPO=0 /D "NDEBUG" /D NT_INST=0 /D NT_UP=1 /D "RDRDBG" /D "SRVDBG" /D "STD_CALL" /D "UNDER_NT" /D "UNICODE" /D "WIN32" /D WIN32=100 /D WIN32_LEAN_AND_MEAN=1 /D WINNT=1 /D WINVER=0x0500 /D "_DEBUG" /D _DLL=1 /D "_IDWBUILD" /D "_MBCS" /D _NT1X_=100 /D "_UNICODE" /D "_WIN32" /D _WIN32_IE=0x0501 /D _WIN32_WINNT=0x0500 /D "_WINDOWS" /D _X86_=1 /D i386=1 /FR"debug\\" /YX /FD -GF -QI0f -QIf -QIfdiv- -Zel -cbstring /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /i "..\..\inc" /i "$(ddkpath)\inc" /i "$(ddkpath)\inc\ddk" /i "$(ddkpath)\inc\ddk\wdm" /d "_DEBUG"
# ADD RSC /l 0x409 /i "..\..\inc" /i "$(ddkpath)\inc" /i "$(ddkpath)\inc\ddk" /i "$(ddkpath)\inc\ddk\wdm" /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 wdm.lib wmilib.lib ..\..\lib\*\stdunk.lib /nologo /base:"0x10000" /version:5.0 /stack:0x40000,0x1000 /entry:"DriverEntry@8" /incremental:no /machine:IX86 /nodefaultlib /out:"objchk\i386\hda.sys" /libpath:"$(ddkpath)\libchk\i386" -align:0x20 -debug:full -driver:wdm -force:multiple -fullbuild -ignore:4001,4037,4039,4049,4044,4065,4070,4078,4087,4089,4198 -merge:_PAGE=PAGE -merge:_TEXT=.text -nocomment -opt:icf -opt:ref -optidata -osversion:4.00 -release -section:init,d -subsystem:native,4.00
# ADD LINK32 portcls.lib ks.lib wdm.lib wmilib.lib ..\..\lib\i386\stdunk.lib /nologo /base:"0x10000" /version:5.0 /stack:0x40000,0x1000 /entry:"DriverEntry@8" /incremental:no /map /machine:IX86 /nodefaultlib /out:"objchk\i386\hda.sys" /libpath:"$(ddkpath)\libchk\i386" -align:0x20 -debug:full -driver:wdm -force:multiple -fullbuild -ignore:4001,4037,4039,4049,4044,4065,4070,4078,4087,4089,4198 -merge:_PAGE=PAGE -merge:_TEXT=.text -nocomment -opt:icf -opt:ref -optidata -osversion:4.00 -release -section:init,d -subsystem:native,4.00
# Begin Custom Build - Finishing up...
IntDir=.\objchk\i386
TargetPath=.\objchk\i386\hda.sys
TargetName=hda
InputPath=.\objchk\i386\hda.sys
SOURCE="$(InputPath)"

"$(IntDir)\$(TargetName).nms" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	$(SIWPATH)\nmsym -translate:source,package,always $(TargetPath) 
	copy $(TargetPath) $(WINDIR)\system32\drivers 
	
# End Custom Build

!ENDIF 

# Begin Target

# Name "HDA - Win32 Free"
# Name "HDA - Win32 Checked"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\adapter.cpp
# End Source File
# Begin Source File

SOURCE=.\codec.cpp
# End Source File
# Begin Source File

SOURCE=.\common.cpp
# End Source File
# Begin Source File

SOURCE=.\HDA.rc
# End Source File
# Begin Source File

SOURCE=.\mintopo.cpp
# End Source File
# Begin Source File

SOURCE=.\minwave.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
