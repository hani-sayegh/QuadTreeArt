@echo off
rmdir /s /q debug
mkdir debug

:: Add SDL3 DLL folder to PATH for this session
copy "C:\Users\battlepants\Desktop\CE\projects\QuadTreeArt\SDL3-devel-3.2.26-VC\SDL3-3.2.26\lib\x64\SDL3.dll" "C:\Users\battlepants\Desktop\CE\projects\QuadTreeArt\debug\"

:: Add SDL3 DLL folder to PATH for this session
copy "C:\Users\battlepants\Desktop\CE\projects\QuadTreeArt\owl.jpg" "C:\Users\battlepants\Desktop\CE\projects\QuadTreeArt\debug\"

clang  -I "C:/Users/battlepants/Desktop/CE/projects/QuadTreeArt/SDL3-devel-3.2.26-VC/SDL3-3.2.26/include" -L "C:/Users/battlepants/Desktop/CE/projects/QuadTreeArt/SDL3-devel-3.2.26-VC/SDL3-3.2.26/lib/x64" -x c -std=c11 -g -Wvla -Wall -Wextra  -Wno-unused-variable -lSDL3 ./file_main.c -o debug/quad.exe
.\debug\quad.exe
popd
