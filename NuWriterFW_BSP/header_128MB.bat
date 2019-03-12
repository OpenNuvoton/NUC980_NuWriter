set dt=%date:/=%
set newdate=%dt:~2,6%13
romh NuWriterFW_128MB.bin 0x07f00040 0x00010301
echo F|xcopy  xusb.bin xusb128MB.bin  /m /q /Y
echo F|xcopy  xusb128MB.bin ..\\NuWriter\\NuWriter\\Release\\xusb128.bin /q /Y