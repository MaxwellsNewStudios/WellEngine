@echo off
cd %0\..\
texconv.exe --block-compress x --format BC1_UNORM -vflip -nologo --overwrite -r -o out in/Color/*
texconv.exe --block-compress x --format BC4_UNORM -vflip -nologo --overwrite -r -o out in/Grayscale/*
texconv.exe --block-compress x --format BC3_UNORM -vflip -nologo --overwrite -r -o out in/Transparent/*
texconv.exe --block-compress x --format BC7_UNORM -vflip -nologo --overwrite -r -o out in/Normal/*
pause

:: texconv.exe -nologo --block-compress x --format BC3_UNORM -o test/1_compressed test/transperancy.png
:: texconv.exe -nologo --format R8G8B8A8_UNORM -o test/2_uncompressed test/1_compressed/transperancy.dds
:: texconv.exe -nologo --block-compress x --format BC3_UNORM -o test/3_recompressed test/2_uncompressed/transperancy.dds