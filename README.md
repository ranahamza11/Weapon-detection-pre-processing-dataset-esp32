# Weapon-detection-pre-processing-dataset-esp32
Power on the esp32 cam. esp32 should send image as an bmp file of size 320 x 240 pixels, then run the code provided here this code is continually feteching images and detect motion, recoganize objects, highlight objects, separate objects, make boundary of the objects and resize them. 

This code will run on windows platform only because of the winsock network library. 
--> The esp32 camera server should send 320x240 pixel bmp.

--> To execute the code you need to make the same directories inside the project folder as shown in my directory, then connect the esp32 and your system to the same wifi network i.e. your mobile hotspot. 

--> NEED to modify ESP_SERVER_IP to your esp32 ip (you can check this from your mobile hotspot connected devices then select esp32 cam the ip will be shown there).
--> NEED to modify the path variable to your project directory.

then execute... :)
