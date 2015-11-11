#ifndef FILE_H
#define FILE_H

// Vstupni nebo vystupni soubor
#define FILE_IN			0
#define FILE_OUT		1

// Makra pro navratove hodnoty metod (lepsi prehlednost kod)
#define OPEN_OK			0
#define CANNOT_OPEN 	1
#define FILE_EXISTS		2
#define NOT_FOUND  		3
#define PERM_DEN		4
#define READ_ERR		-1
#define READ_OK			0

// Mod prenosu
#define NETASCII 		1
#define OCTET 			2


#include "libraries.h"


using namespace std;

class File
{
public:
	// Atributy

	FILE *in;
	FILE *out;
	unsigned int bytes_read;
	unsigned int file_size;
	char last_char;

	// Metody

	// Konstruktor tridy
	File(): in(NULL), out(NULL), bytes_read(0), file_size(0), last_char(42) {};

	// Otevreni souboru
	int open(string name, string path, int mode, int type);

	// Navrat hodnoty velikosti souboru
	int get_file_size(void);

	// Nacitani dat
	int get_data(int size, int block_num, char *dest, int type);

	// Ukladani dat
	int store_data(int size, char *source, int type);

	// Mazani souboru
	void remove_file(string name);

	// Uzavirani deskriptoru souboru
	void close_file(int type);
};

#endif /* FILE_H */