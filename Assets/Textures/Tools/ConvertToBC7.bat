@echo off
cd %0\..\
texconv.exe --block-compress x --format BC1_UNORM -vflip -nologo --overwrite -r -o out/Color/ in/Color/*
texconv.exe --block-compress x --format BC4_UNORM -vflip -nologo --overwrite -r -o out/Grayscale/ in/Grayscale/*
texconv.exe --block-compress x --format BC3_UNORM -vflip -nologo --overwrite -r -o out/Transparent/ in/Transparent/*
texconv.exe --block-compress x --format BC7_UNORM -vflip -nologo --overwrite -r -o out/Normal/ in/Normal/*
pause