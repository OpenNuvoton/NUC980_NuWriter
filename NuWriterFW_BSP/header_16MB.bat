set dt=%date:/=%
set newdate=%dt:~2,6%13
romh NuWriterFW_16MB.bin 0x0f00040 0x00010406
echo F|xcopy xusb.bin xusb16MB.bin /m /q /Y 
echo F|xcopy xusb16MB.bin ..\\NuWriter\\NuWriter\\Release\\xusb16.bin /q /Y