set dt=%date:/=%
set newdate=%dt:~2,6%13
romh NuWriterFW_32MB.bin 0x01f00040 0x00010307
echo F|xcopy xusb.bin xusb32MB.bin  /m /q /Y
echo F|xcopy xusb32MB.bin ..\\NuWriter\\NuWriter\\Release\\xusb.bin /q /Y