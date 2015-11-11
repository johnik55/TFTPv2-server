#include "options.h"

using namespace std;

/******************************************************************************
	Konstruktor tridy Opt
*******************************************************************************/
Opt::Opt(): size(0), timeout(0) {}

/******************************************************************************
	Vypis napovedy
*******************************************************************************/
void Opt::print_help()
{
	string text;

	text += "================================================================================\n";
	text += "TFTPv2 server, project ISA 2014, FIT VUT Brno\n";
	text += "================================================================================\n\n";
	text += "Author: Jan Kafka\n";
	text += "login: xkafka00\n\n";
	text += "Options : \n\n";
	text += "-h\t\t\t\t\t\t\thelp\n";
	text += "-d <string>\t\t\t\t\t\tworking directory, full or relative\n";
	text += "-s <integer value>\t\t\t\t\tmax. accepted data block size from client\n";
	text += "-t <integer value>\t\t\t\t\tmax. accepted timeout from client\n";
	text += "-a <string>,<integer value>\t\t\t\tone service address and port\n";
	text += "-a <string>,<integer value>#<string>,<integer value>\tmore services addresses and ports\n\n";
	text += "Examples:\n\n";
	text += "./mytftpserver -d /home/jan/Dokumenty/\n\n";
	text += "Run one server on default address 127.0.0.1 and default port 69.\n\n";
	text += "./mytftpserver -a 127.0.0.1,1300 -d /home/jan/Dokumenty/ -s 1024 -t 10\n\n";
	text += "Run one server on address 127.0.0.1, port 1300, directory /home/jan/Dokumenty/\n";
	text += "max. block size 1024B, max. timeout 10s.\n\n";
	text += "./mytftpserver -a 127.0.0.1,1300#::1,1400 -d /home/jan/Dokumenty/ -s 1024 -t 10\n\n";
	text += "Run two servers, first on address 127.0.0.1, port 1300, second on address ::1,\n";
	text += "port 1400, both servers with max. block size 1024B and max. timeout 10s.\n\n";
	text += "================================================================================\n";

	cout << text;
}

/******************************************************************************
	Parsovani IP adresy a portu
*******************************************************************************/
int Opt::parse_address(char *str)
{
	string temp = str;
	string temp_port;
	string split_addr = ",";
	string split_block = "#";
	int block_count = count(temp.begin(),temp.end(),'#');

	if(block_count == 0)
	{// pouze jedna adresa a port

		size_t pos = temp.find(split_addr); // najit pozici carky
		addr new_addr; // nova struktura

		if(pos == string::npos)
		{// carka neni, jen adresa bez portu

			// naplneni struktury daty
			new_addr.address = temp;
			new_addr.port = DEFAULT_PORT;

			addresses.push_back(new_addr); // pridani do vektoru
		}
		else
		{// adresa i port
			new_addr.address = temp.substr(0,pos);
			temp_port = temp.substr(pos + 1,temp.length());
			new_addr.port = stoi(temp_port);

			addresses.push_back(new_addr); // pridani do vektoru
		}
	}
	else
	{// vice adres
		size_t begin = 0;
		size_t end = 0;
		size_t pos;
		addr new_addr;
		string block;

		for(int i=0; i<block_count+1; i++)
		{// opakuj pro kazdy blok

			end = temp.find(split_block,begin); // najit pozici #
			if(end == string::npos) end = temp.length(); // posledni blok

			block = temp.substr(begin,end - begin); // udelat podretezec

			pos = block.find(split_addr); // najit pozici carky

			if(pos == string::npos)
			{// jen adresa bez portu
				new_addr.address = block;
				new_addr.port = DEFAULT_PORT;

				addresses.push_back(new_addr); // pridani do vektoru
			}
			else
			{// adresa i port
				new_addr.address = block.substr(0,pos);
				temp_port = block.substr(pos + 1,block.length());
				new_addr.port = stoi(temp_port);

				addresses.push_back(new_addr); // pridani do vektoru
			}

			begin = end + 1; // posunuti zacatku pro dalsi iteraci
		}
	}
}

/******************************************************************************
	Nacitani parametru
*******************************************************************************/
int Opt::load(int argc, char *argv[])
{
	int option;
	bool error = false;

	while ((option = getopt(argc,argv,"d:t:s:a:h")) != -1)
	{
		switch(option)
		{
			case 'd':
			{// cesta k adresari
				path = optarg;

				break;
			}
			case 's':
			{// velikost bloku
				try
				{
					size = stoi(optarg);
				}
				catch(...)
				{
					cerr << "Error - block size is not valid number\n";
					error = true;
				}

				break;
			}
			case 't':
			{// timeout
				try
				{
					timeout = stoi(optarg);
				}
				catch(...)
				{
					cerr << "Error - timeout is not valid number\n";
					error = true;
				}

				break;
			}
			case 'a':
			{// adresa a port sluzby (sluzeb)
				parse_address(optarg);

				break;
			}
			case 'h':
			{// napoveda
				print_help();
				return HELP;
			}
			default:
				break;
		}
	}

	if(error == true)
		return OPTION_FAIL;
	else
		return OPTION_OK;
}

/******************************************************************************
	Kontrola nactenych parametru
*******************************************************************************/
int Opt::check(void)
{
	bool error = false;
	DIR *directory;

	if(path.length() == 0)
	{
		cerr << "ERROR: no path (option -d)\n";
		error = true;
	}

	if(addresses.size() == 0)
	{// Nebyla zadana adresa, ale chyba to neni, jen se doplni defaultni
		addr new_addr;

		new_addr.address = DEFAULT_ADDRESS;
		new_addr.port = DEFAULT_PORT;

		addresses.push_back(new_addr); // pridani do vektoru
	}

	directory = opendir(path.c_str());
	if (directory == NULL)
	{// Adresar neexistuje
		cerr << "ERROR: directory " << path << " doesn't exists\n";
		error = true;
	}

	free(directory);

	if(error) return OPTION_FAIL;
	else return OPTION_OK;
}

string Opt::working_path(void)
{// Navrat retezce s adresarem
	return path;
}

int Opt::block_size(void)
{// Navrat zadane velikosti bloku
	return size;
}

int Opt::max_timeout(void)
{// Navrat zadaneho casoveho vyprseni
	return timeout;
}

int Opt::address_count(void)
{// Navrat poctu zadanych adres
	return addresses.size();
}

string Opt::address_at(int index)
{// Navrat retezce s adresou na konkretnim indexu
	addr item;

	item = addresses.at(index);
	return item.address;
}

int Opt::port_at(int index)
{// Navrat portu na konkretnim indexu
	addr item;

	item = addresses.at(index);
	return item.port;
}
