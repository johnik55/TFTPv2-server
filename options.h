#ifndef OPTIONS_H
#define OPTIONS_H

#include "libraries.h"

// Makra pro navratove hodnoty metod (lepsi prehlednost kod)
#define OPTION_OK	0
#define OPTION_FAIL 1
#define HELP		2

// Defaultni adresa a port sluzby
#define DEFAULT_ADDRESS "127.0.0.1"
#define DEFAULT_PORT	69


using namespace std;

typedef struct addr
{// Adresa a port
	string address;
	int port;

}addr;

class Opt
{
public:
	// Atributy

	string path;				// cesta k pracovnimu adresari
	int size;					// velikost bloku
	int timeout;				// timeout
	vector<addr> addresses;		// n-tice adres a portu
	
	// Metody
	
	// Konstruktor tridy
	Opt();

	// Parsovani ip adresy a portu
	int parse_address(char *str);

	// Nacteni parametru
	int load(int argc, char *argv[]);

	// Kontrola poctu a spravnosti parametru
	int check(void);

	// Navrat nazvu pracovniho adresare
	string working_path(void);

	// Navrat velikosti bloku
	int block_size(void);

	// Navrat hodnoty timeoutu
	int max_timeout(void);

	// Navrat poctu zadanych adres
	int address_count(void);

	// Navrat ip adresy na urcitem indexu
	string address_at(int index);

	// Navrat portu na urcitem indexu
	int port_at(int index);

	// Vypis napovedy
	void print_help();
};

#endif /* OPTIONS_H */