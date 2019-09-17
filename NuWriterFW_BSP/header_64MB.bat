set dt=%date:/=%
set newdate=%dt:~2,6%13
romh NuWriterFW_64MB.bin 0x03f00040 0x00010305
echo F|xcopy  xusb.bin xusb64MB.bin  /m /q /Y
echo F|xcopy  xusb64MB.bin ..\\NuWriter\\NuWriter\\Release\\xusb64.bin /q /Y