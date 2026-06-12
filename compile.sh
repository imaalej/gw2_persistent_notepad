#!/bin/bash

# Define the output addon name
OUTPUT_DLL="Notepad.dll"

echo "Starting cross-compilation for ${OUTPUT_DLL}..."

# Compile command
x86_64-w64-mingw32-g++ -shared -o "$OUTPUT_DLL" \
    src/entry.cpp \
    src/imgui/*.cpp \
    src/notes.cpp \
    -I./src \
    -I./src/nexus \
    -I./src/imgui \
    -I./src/mumble \
    -static \
    -static-libgcc \
    -static-libstdc++ \
    -DUNICODE \
    -D_UNICODE \
    -DWIN32 \
    -D_WIN32 \
    -luser32 \
    -mwindows \
    -Wl,--subsystem,windows \
    -lgdi32 \
    -lwinmm \
    -lole32

if [ $? -eq 0 ]; then
    echo "=========================================="
    echo "Success! Created ${OUTPUT_DLL}."
    echo "Now move it to your GW2/addons/ directory."
    echo "=========================================="
else
    echo "Compilation failed! Check the errors above."
fi
