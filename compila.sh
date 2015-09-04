g++ -std=c++0x  -Wall -pedantic config.c rfid.c rc522.c timers.cpp -o timers `sdl2-config --cflags --libs` -lSDL2_image -lSDL2_ttf -lwiringPi -lpthread -lbcm2835 -L/usr/include/mysql -lmysqlclient -I/usr/include/mysql
#  config.c rfid.c rc522.c
# g++ -std=c++0x -Wall -pedantic `sdl2-config --cflags --libs` -lSDL2_image -lSDL2_ttf 
