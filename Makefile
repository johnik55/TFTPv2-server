all:
	g++ -std=c++0x -o mytftpserver main.cpp options.cpp server.cpp file.cpp

clean:
	rm -f mytftpserver