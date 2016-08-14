@pushd %~dp0
@setlocal
@call ..\setflags.cmd
@set cpp=rt.cpp mem.cpp pe.cpp out_stream.cpp
@set extracpp=net\net.cpp net\tftp.cpp
@set hostcpp=host_stubs.cpp
@set usercpp=crtstartup.cpp
@set obj=%cpp:.cpp=.obj% net.obj tftp.obj
@set hostobj=%hostcpp:.cpp=.obj%
@set targetobj=crt.obj
@set userobj=%usercpp:.cpp=.obj% syscall.obj %targetobj%
nasm %ATTOS_ASFLAGS% crt.asm -o crt.obj || (popd & exit /b 1)
nasm %ATTOS_ASFLAGS% syscall.asm -o syscall.obj || (popd & exit /b 1)
cl %ATTOS_CXXFLAGS% /c %cpp% %extracpp% %hostcpp% %usercpp% || (popd & exit /b 1)
lib /out:attos_host.lib   /nologo /ignore:4221 %obj% %hostobj%   || (popd & exit /b 1)
lib /out:attos_kernel.lib /nologo /ignore:4221 %obj% %targetobj% || (popd & exit /b 1)
lib /out:attos_user.lib   /nologo /ignore:4221 %obj% %userobj%   || (popd & exit /b 1)
@endlocal
@popd
