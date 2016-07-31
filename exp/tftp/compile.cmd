@setlocal
@set ad=..\..\stage3
cl /W4 /WX /DEBUG /Zi /D_HAS_EXCEPTIONS=0 /I%ad% tftps.cpp %ad%\attos\net\net.cpp %ad%\attos\net\tftp.cpp %ad%\attos\out_stream.cpp ws2_32.lib || exit /b 1
