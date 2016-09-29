CC = ~/w/iip/bikestation/vendor/rpi-tools/arm-bcm2708/gcc-linaro-arm-linux-gnueabihf-raspbian-x64/bin/arm-linux-gnueabihf-gcc
CFLAGS = -std=c99

rc522_mifare: rc522_mifare.c *.h
	$(CC) $(CFLAGS) -o rc522_mifare rc522_mifare.c 

run: rc522_mifare
	ssh root@box050 'killall rc522_mifare || true'
	scp rc522_mifare root@box050:
	ssh root@box050 'strace -e read,write -xx ./rc522_mifare 2>&1 | head -n40'
