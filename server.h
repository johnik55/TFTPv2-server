#ifndef SERVER_H
#define SERVER_H

#include "libraries.h"
#include "options.h"
#include "file.h"

// Vychozi parametry serveru
#define MAX_PORT			64400	// Max. hodnota portu
#define MIN_PORT			1025	// Minimalni hodnota portu (nizsi porty jsou rezervovane)
#define MIN_BLOCK_SIZE		8		// Nejmensi velikost bloku (dle RFC)
#define MAX_BLOCK_SIZE		65464	// Maximalni velikost bloku (MTU lo)
#define STANDARD_BLOCK		512		// Standardni velikost (dle RFC)
#define MAX_BLOCK_COUNT		65535	// Maximalni pocet bloku, ktere lze odeslat (2^16 - 1)
#define IMPLICIT_TIMEOUT 	3		// Implicitni hodnota timeoutu (pokud neni urceno jinak)
#define HEADERS				32  	// Pocet B ktere zabiraji ruzne hlavicky paketu

#define BUFSIZE				512 	// Velikost standardniho bufferu

// Error kody
#define ERROR_ND 0 		// Not defined (nedefinovano)
#define ERROR_NF 1 		// File not found (soubor nenalezen)
#define ERROR_AV 2 		// Access violations (chyba pristupu)
#define ERROR_DF 3 		// Disk full or allocation exceeded (malo mista ci problem s alokaci)
#define ERROR_IL 4 		// Illegal TFTP operation (neplatna operace)
#define ERROR_UN 5 		// Unknown transfer ID ()
#define ERROR_AE 6 		// File already exists
#define ERROR_NU 7 		// No such user


// Opcode paketu
#define RRQ				1 		// Read request
#define WRQ 			2		// Write request
#define DATA 			3 		// Data packet
#define ACK 			4		// Acknowledge
#define ERR 			5 		// Error packet
#define OACK			6 		// Option acknowledge
#define UNKNOWN_OPCODE -1 		// Unknown opcode

// mod prenosu
#define NETASCII 		1
#define OCTET 			2
#define MAIL			3
#define UNKNOWN_MODE 	4

// rozsireni
#define STANDARD 		0
#define EXTENSION 		1

// odeslani paketu
#define SEND_OK 		0
#define SEND_FAIL		-1

// prijeti paketu
#define RECV_OK			0
#define TIMEOUT_ELAPSED	-1

// nastaveni socketu
#define SET_FAILED		-1
#define SET_OK 			0

#define MTU_FAILED		1



using namespace std;

typedef struct ext_option
{// Rozsirene parametry - dvojice hodnota a indikace pouziti
	unsigned int value;
	bool used;
}ext_option;


class Service
{
public:

	// Metody

	// Start sluzby
	int start(Opt *options, int index);

	// Ziskani velikosti MTU pro konkretni rozhrani
	int get_mtu_size(int sockfd, string service_ip);

};



class Client
{
public:

	// Atributy

	bool extension;
	ext_option client_timeout;
	ext_option block_size;
	ext_option transport_size;
	int mode_tr;
	string filename;
	int block_num;
	int client_socket;
	int mtu_size;
	string service_ip;
	string client_port;

	// Metody

	// Konstruktor tridy
	Client();

	// Obsluha klienta
	int start(Opt *options, struct sockaddr_storage client_addr, int index, char *buf, int numbytes, int mtu);

	// Nastavi cas vyprseni socketu
	int set_socket_timeout(Opt *options, int value);

	// Rozpoznani typu paketu ze zpravy
	int packet_type(char *message);

	// Rozpoznani typu prenosu ze zpravy
	int transfer_mode(char *message);

	// Zjisteni pritomnosti rozsireni ve zprave
	int opt_extension(char *message, int numbytes, Opt *options, File *f);

	// Ziskani jmena souboru ze zpravy
	int get_filename(char *message);

	// Zaslani OACK (potvrzeni rozsirujicich parametru)
	int send_oack(int sockfd, struct sockaddr_storage client_addr);

	// Zaslani ACK (potvrzeni prijeti datoveho paketu)
	int send_ack(int sockfd, struct sockaddr_storage client_addr, int block_num);

	// Zaslani chybove zpravy
	int send_error(int sockfd, struct sockaddr_storage client_addr, string message, int code);

	// Prijeti chyboveho kodu ze zpravy
	int recv_error(char *source);

	// Prijeti paketu
	int recv_packet(int sockfd, struct sockaddr_storage client_addr, char *buf, int buf_size);

	// Ziskani cisla potvrzovaneho datoveho paketu
	int ack_number(char *message);

	// Zaslani dat
	int send_data(int sockfd, struct sockaddr_storage client_addr, int block_num, int size, File *f, char *file_buf);

	// Ulozeni dat
	int recv_data(char *source, int numbytes, int size, File *f);

	void test(void);
};

#endif /* SERVER_H */