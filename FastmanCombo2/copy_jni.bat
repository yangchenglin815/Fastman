cd /d %~dp0
COPY /Y libs\armeabi\zmaster2 gencode\
COPY /Y libs\armeabi-v7a\zmaster2_pie gencode\
COPY /Y libs\mips\zmaster2_mips gencode\
COPY /Y libs\x86\zmaster2_x86 gencode\
pause
