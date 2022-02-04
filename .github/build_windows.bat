call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
mkdir build
cd build
cmake -G "NMake Makefiles" -DBUILD_TEST=TRUE ..
nmake
