set dt=%date:/=%
set newdate=%dt:~2,6%01
romh NuWriterFW_128MB.bin 0x07f00040 %newdate%
echo F|xcopy  xusb.bin xusb128MB.bin  /m /q /Y
echo F|xcopy  xusb128MB.bin ..\\NuWriter\\NuWriter\\Release\\xusb128.bin /q /Y