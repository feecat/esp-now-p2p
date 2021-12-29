# esp-now-p2p

Our team have a project that need wireless communication with some button, Like a garage wireless key but in industrial.  
433mhz or lora is slow and not good for programming, I had seen that most industrial handwheel is 2.4G communication.  
So i choose esp32-s2 with arduino library. It Success finish my job. And i want share if anyone need it in others project.  
Also welcome put you issues in this repo.

Because the code need modify to suitable you project, so i just write a simple explain.  
esp8266/esp32/esp32-s2/esp32-c3 have different pin, you need modify it, but all chip works fine.  

## Project target
We need a master to send buttons status, and slave will receive package and send it to other device, it need fast.  
But we also need slave send data to master, to show leds, it not need fast.  
We dont care current consume, in our project a 18650 battery will supply about 30 hours for sender.  
For now this project is not perfect, but enouth to use.  

## Master/Slave  
We had use same project to any esp32 chip, so we need a physical pin to detect master/slave, for example it defined on pin3.  
when startup, put it on internel pullup mode and read pin state, if high, then work on master mode(Sender).  

## EEPROM
The device only works on p2p mode, its exactly point to point otherwize the machine will crazy.  
So we add a pin for pair, for example it defined on pin4.  
When startup, pull it on internel pullup mode and read pin state, if low, then works on pair mode.  

In pair mode, it broadcase self mac address, and receive all esp-now package.  
If any package that package data equal sender mac address, then record mac to eeprom (Only record once for reduce eeprom damage).  
Also it continues send self mac, so anyone device can pair this device.  

## Normal mode
In normal mode, it read mac address from eeprom, if success, Only accept esp-now package that sender mac equal eeprom mac.  
Master and slave will exchange data every 1 sec. if some data change, it will interrupt wait and send data immediately.  
  
Here have a little flaw, sometime data jitter in send time, maybe will send a lot of package. but it good for actual use.  

## Product deploy
Arduino will compile to 4 file, so in product line, install arduino ide and enable verbose output show.  
After compile once successful, copy file to a folder, and write a bat like this:  
```
C:\Users\Administrator\AppData\Local\Arduino15\packages\esp32\tools\esptool_py\3.1.0/esptool.exe --chip esp32s2 --port COM8 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB 0xe000 ./boot_app0.bin 0x1000 ./espnow_05.ino.bootloader.bin 0x10000 ./espnow_05.ino.bin 0x8000 ./espnow_05.ino.partitions.bin 
```
The path, com port, chip and file name is different in your project, you need see it on your compile verbose.  
