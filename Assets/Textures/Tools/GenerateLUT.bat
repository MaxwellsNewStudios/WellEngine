@echo off
cd %0\..\

magick in/LUT_Test.png -crop 64x64 +repage t/%d.png

texassemble volume -o out/LUT_Test.dds -y -f R8G8B8A8_UNORM t/0.png t/1.png t/2.png t/3.png t/4.png t/5.png t/6.png t/7.png t/8.png t/9.png t/10.png t/11.png t/12.png t/13.png t/14.png t/15.png t/16.png t/17.png t/18.png t/19.png t/20.png t/21.png t/22.png t/23.png t/24.png t/25.png t/26.png t/27.png t/28.png t/29.png t/30.png t/31.png t/32.png t/33.png t/34.png t/35.png t/36.png t/37.png t/38.png t/39.png t/40.png t/41.png t/42.png t/43.png t/44.png t/45.png t/46.png t/47.png t/48.png t/49.png t/50.png t/51.png t/52.png t/53.png t/54.png t/55.png t/56.png t/57.png t/58.png t/59.png t/60.png t/61.png t/62.png t/63.png 

pause
