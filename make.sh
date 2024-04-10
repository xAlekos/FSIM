gcc -g -Wall -fsanitize=address fsim.c `pkg-config fuse3 --cflags --libs` -o fsim
