#include "server.h"


vector<pid_t> children; // vektor PID potomku, globalni


/******************************************************************************
	Konstruktor tridy Client
*******************************************************************************/
Client::Client(): extension(false), mode_tr(0), block_num(0), mtu_size(0)
{
	client_timeout.value = 0;
	client_timeout.used = false;

	block_size.value = 0;
	block_size.used = false;

	transport_size.value = 0;
	transport_size.used = false;
}

/******************************************************************************
	Osetreni ukonceni obsluzneho procesu
*******************************************************************************/
void client_end(int sig)
{// Obsluha skoncila
	int status;
	waitpid(-1, &status, 0); // Cekani na jakehokoli potomka, az se ukonci
}

/******************************************************************************
	Osetreni ukonceni vsech procesu pri SIGINT
*******************************************************************************/
void server_signal_reaction(int sig)
{// Reakce serveru na signal k ukonceni
	int chld = children.size(); // Zjisti se pocet potomku
	int status;

	if(chld > 0)
	{// Bezi nejaky potomek
		for(int i=0; i<chld; i++)
		{// Posle signal kazdemu potomkovi

			kill(children.at(i), SIGINT); // Zasle potomkovi SIGINT
			waitpid(-1, &status, 0); // Cekani na jakehokoli potomka, az se ukonci
		}
	}

	exit(0);
}

/******************************************************************************
	Osetreni ukonceni klienta pri SIGINT
*******************************************************************************/
void client_signal_reaction(int sig)
{// Reakce klienta na signal k ukonceni
	exit(0);
}

/******************************************************************************
	Funkce ziska IP adresu

	Inspirace (ziskani adresy ze struktury):
	http://beej.us/guide/bgnet/output/html/multipage/clientserver.html#datagram
*******************************************************************************/
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET)
		{// Navrat ukazatele na ipv4 adresu
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr); // navrat ukazatele na ipv6 adresu
}

/******************************************************************************
	Funkce nastavi cas vyprseni socketu
*******************************************************************************/
int Client::set_socket_timeout(Opt *options, int value)
{
	// Nastaveni timeout
	struct timeval tv;
	tv.tv_usec = 0;
	tv.tv_sec = value;

	if(setsockopt (client_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv)) < 0)
	{
		cerr << "Error - Service " << service_ip << ", " << client_port << " - setsockopt failed" << endl;
		return SET_FAILED;
	}
	else return SET_OK;
	
}

/******************************************************************************
	Funkce ziska velikost MTU na rozhrani, na kterem bezi server

	Inspirace (prochazeni rozhrani a zjistovani parametru):
	http://www.linuxhowtos.org/manpages/3/getifaddrs.htm
*******************************************************************************/
int Service::get_mtu_size(int sockfd, string service_ip)
{
	struct ifaddrs *ifaddr, *ifa;
	int family, s;
	char host[NI_MAXHOST];
	struct ifreq ifr;
	ifr.ifr_addr.sa_family = AF_UNSPEC;
	bool error = false;
	int mtu_size;

	string interface;
	string ip;

	if (getifaddrs(&ifaddr) == -1)
		return MTU_FAILED;


	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
	{
		if (ifa->ifa_addr == NULL)
			continue;

		family = ifa->ifa_addr->sa_family;
		interface = ifa->ifa_name;

		if (family == AF_INET || family == AF_INET6)
		{
			s = getnameinfo(ifa->ifa_addr,
					(family == AF_INET) ? sizeof(struct sockaddr_in) :
										  sizeof(struct sockaddr_in6),
					host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
			if (s != 0)
				error = true;
			
			ip = host;

			if(ip == service_ip)
			{// nalezeno rozhrani, na kterem bezi nase sluzba

				strcpy(ifr.ifr_name, interface.c_str());

				if (ioctl(sockfd, SIOCGIFMTU, (caddr_t)&ifr) < 0)
					error = true;
					
				else
					mtu_size = ifr.ifr_mtu;

				break;
			}
		}
	}

	freeifaddrs(ifaddr);

	if(error)
		return MTU_FAILED;
	else
		return mtu_size;
}

/******************************************************************************
	Ziskani aktualniho casu a ulozeni do stringu

	Inspirace (zjistovani aktualniho data a casu):
	http://stackoverflow.com/questions/997946/how-to-get-current-time-and-date-in-c
*******************************************************************************/
string current_time(void)
{
	time_t	   now = time(0);
	struct tm  tstruct;
	char	   buf[80];
	tstruct = *localtime(&now);
	string str_buf;
	string out;

	strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);

	str_buf = buf;
	out = "[" + str_buf + "] ";

	return out;
}

/******************************************************************************
	Zjisteni pritomnosti rozsireni, pripadne zpracovani parametru
*******************************************************************************/
int Client::opt_extension(char *message, int numbytes, Opt *options, File *f)
{
	int zero_count = 0;
	int pos = 0;
	int opt_num = 0;
	int opt_len = 0;
	string option;
	string value;

	for(int i=2; i<numbytes; i++)
	{// spocitat vsechny oddelovace
		if(message[i] == '\0')
		{
			zero_count++;
			if(zero_count == 2) pos = i;
		}
	}

	if(zero_count > 2)
	{// i s options
		opt_num = (zero_count-2)/2;

		for(int i=0; i<opt_num; i++)
		{
			// ulozeni nazvu option do stringu
			option = message + pos + 1;
			opt_len = option.length(); // ulozim si jeho delku
			pos = pos + opt_len + 1; // nastavim novou pozici nuly

			// ulozeni hodnoty option do stringu
			value = message + pos + 1;
			pos = pos + value.length() + 1;

			// zmenit vsechny pismena na male
			for(int i=0; i<opt_len; i++) if(isupper(option[i])) option[i] = tolower(option[i]);

			if(!(option.compare("blksize")))
			{
				// prevod ze stringu na int
				block_size.value = stoi(value);

				if(options->block_size() == 0)
				{// nebyla zadana velikost bloku pri spusteni, nastavi se max. velikost dle MTU
					if(block_size.value > (mtu_size - HEADERS)) block_size.value = mtu_size - HEADERS;
					else if(block_size.value < MIN_BLOCK_SIZE) block_size.value = MIN_BLOCK_SIZE;
				}
				else
				{// Byla zadana velikost bloku pri spusteni, musi se porovnat s ni
					if(block_size.value > options->block_size()) block_size.value = options->block_size();
					else if(block_size.value < MIN_BLOCK_SIZE) block_size.value = MIN_BLOCK_SIZE;
				}

				block_size.used = true;
				extension = true;
			}
			else if(!(option.compare("timeout")))
			{
				client_timeout.value = stoi(value);

				if(options->max_timeout() > 0)
				{// Byl zadan max. timeout pri spusteni
					if(client_timeout.value <= options->max_timeout())
					{// Pozadovana hodnota je mensi nebo rovna nastavenemu limitu
						set_socket_timeout(options, client_timeout.value);

						client_timeout.used = true;
						extension = true;
					}
				}
				else
				{// Nebyl zadan
					if(client_timeout.value <= IMPLICIT_TIMEOUT)
					{// Pozadovana hodnota je mensi nebo rovna implicitnimu limitu
						set_socket_timeout(options, client_timeout.value);

						client_timeout.used = true;
						extension = true;
					}
				}
			}
			else if(!(option.compare("tsize")))
			{
				transport_size.value = stoi(value);

				if(transport_size.value == 0)
				{// je to RRQ paket, oznami se velikost souboru
					transport_size.value = f->get_file_size();
				}
				else
				{// WRQ paket, server by mel zkontrolovat volne misto a pripadne prerusit prenos
					transport_size.value = stoi(value);
				}

				transport_size.used = true;
				extension = true;
			}
		}

		if(extension == true) return EXTENSION; // aspon jeden option byl rozpoznan
		else return STANDARD; // options byly, ale server je nezna, takze je nebude uvazovat
	}

	else return STANDARD; // bez options
}

void Client::test(void)
{
	cout << "--- Rozsirene options : ---\n";

	if(block_size.used) cout << "blksize : " << block_size.value << endl;
	if(client_timeout.used) cout << "timeout : " << client_timeout.value << endl;
	if(transport_size.used) cout << "transport size : " << transport_size.value << endl;

	cout << "------------\n";

	cout << "jmeno souboru : " << filename << endl;
}

/******************************************************************************
	Zjisteni modu prenosu, navrat prislusne hodnoty
*******************************************************************************/
int Client::transfer_mode(char *message)
{
	char *pos;
	string mode;
	int i = 2;

	while(message[i] != '\0') i++; // najit oddelovac za filename

	pos = message + i + 1;
	mode = pos;

	for (int j=0; j<mode.length(); j++)
	{// Vsechna pismena na mala
		if(isupper(mode[i])) mode[i] = tolower(mode[i]);
	}

	if(!(mode.compare("netascii")) || !(mode.compare("net ascii")))
	{
		mode_tr = NETASCII;
		return NETASCII;
	}
	else if(!(mode.compare("octet")))
	{
		mode_tr = OCTET;
		return OCTET;
	}
	else return UNKNOWN_MODE;
}

/******************************************************************************
	Ziskani jmena souboru z prijate zpravy
*******************************************************************************/
int Client::get_filename(char *message)
{
	filename = message + 2;

	if(filename.length() > 0) return 0;
	else return 1;
}

/******************************************************************************
	Zjisteni typu paketu, ktery byl prijat
*******************************************************************************/
int Client::packet_type(char *message)
{
	if(message[0] == 0)
	{
		switch(message[1])
		{
			case RRQ: return RRQ;
			case WRQ: return WRQ;
			case DATA: return DATA;
			case ACK: return ACK;
			case ERR: return ERR;

			default:
			{
				return UNKNOWN_OPCODE;
			}
		}
	}
}

/******************************************************************************
	Zaslani chybove zpravy v error packetu
*******************************************************************************/
int Client::send_error(int sockfd, struct sockaddr_storage client_addr, string message, int code)
{
	char temp[100];
	int pos = 0;
	int numbytes = 4;

	temp[pos++] = '\0';
	temp[pos++] = '\5';
	temp[pos++] = '\0';
	temp[pos++] = code;

	strcpy(temp + pos, message.c_str());
	numbytes += message.length() + 1;

	if ((numbytes = sendto(sockfd,temp,numbytes,0,(struct sockaddr *)&client_addr,sizeof(client_addr))) == -1)
	{
		perror("client error: sendto");
		return 1;
	}

}

/******************************************************************************
	Zaslani potvrzeni rozsirujicich parametru
*******************************************************************************/
int Client::send_oack(int sockfd, struct sockaddr_storage client_addr)
{
	char temp[100];
	int pos = 0;
	int numbytes = 2;
	string temp_str;

	temp[pos++] = '\0';
	temp[pos++] = OACK;

	if(client_timeout.used)
	{
		strcpy(temp + pos, "timeout");
		pos += 8;
		numbytes += 8;
		temp_str = to_string(client_timeout.value);
		strcpy(temp + pos, temp_str.c_str());
		pos += temp_str.length() + 1;
		numbytes += temp_str.length() + 1;
	}

	if(block_size.used)
	{
		strcpy(temp + pos, "blksize");
		pos += 8;
		numbytes += 8;
		temp_str = to_string(block_size.value);
		strcpy(temp + pos, temp_str.c_str());
		pos += temp_str.length() + 1;
		numbytes += temp_str.length() + 1;
	}

	if(transport_size.used)
	{
		strcpy(temp + pos, "tsize");
		pos += 6;
		numbytes += 6;
		temp_str = to_string(transport_size.value);
		strcpy(temp + pos, temp_str.c_str());
		pos += temp_str.length() + 1;
		numbytes += temp_str.length() + 1;
	}

	if ((numbytes = sendto(sockfd,temp,numbytes,0,(struct sockaddr *)&client_addr,sizeof(client_addr))) == -1) {
		perror("client oack: sendto");
		return 1;
	}

}

/******************************************************************************
	Zaslani paketu, ktery potvrzuje rozsirene parametry
*******************************************************************************/
int Client::send_ack(int sockfd, struct sockaddr_storage client_addr, int block_num)
{
	char temp[20];
	int pos = 0;
	int numbytes = 4;

	temp[pos++] = 0;
	temp[pos++] = ACK;

	if(block_num > 0)
	{// ack nejakeho bloku
		temp[pos++] = block_num / 256;
		temp[pos++] = block_num % 256;
	}
	else
	{// ack nulteho bloku - zahajeni prijmu dat
		temp[pos++] = 0;
		temp[pos++] = 0;
	}

	if ((numbytes = sendto(sockfd,temp,numbytes,0,(struct sockaddr *)&client_addr,sizeof(client_addr))) == -1) {
		perror("client ack: sendto");
		return 1;
	}
}

/******************************************************************************
	Odeslani paketu s daty
*******************************************************************************/
int Client::send_data(int sockfd, struct sockaddr_storage client_addr, int block_num, int size, File *f, char *file_buf)
{
	int pos = 0;
	int numbytes = 4;
	int data_len;

	// Priprava formatu zpravy

	// Opcode
	file_buf[pos++] = '\0';
	file_buf[pos++] = DATA;

	// Cislo bloku
	file_buf[pos++] = block_num / 256;
	file_buf[pos++] = block_num % 256;

	// Data
	data_len = f->get_data(size, block_num, file_buf+pos, mode_tr);

	if(data_len == READ_ERR)
	{// nastala chyba pri cteni
		return SEND_FAIL;
	}

	numbytes += data_len;

	if ((numbytes = sendto(sockfd,file_buf,numbytes,0,(struct sockaddr *)&client_addr,sizeof(client_addr))) == -1) {
		perror("client send_data: sendto");
		return SEND_FAIL;
	}

	return data_len;
}

/******************************************************************************
	Prijeti paketu, ulozeni zpravy do bufferu
*******************************************************************************/
int Client::recv_packet(int sockfd, struct sockaddr_storage client_addr, char *buf, int buf_size)
{
	socklen_t addr_len = sizeof client_addr;
	int numbytes;

	if ((numbytes = recvfrom(sockfd, buf, buf_size , 0,
		(struct sockaddr *)&client_addr, &addr_len)) == -1)
	{
		return TIMEOUT_ELAPSED;
	}
	else return numbytes;
}

/******************************************************************************
	Ukladani prijatych dat do souboru
*******************************************************************************/
int Client::recv_data(char *source, int numbytes, int size, File *f)
{
	int bytes_write;

	numbytes -= 4; // opcode a cislo bloku neuvazovat

	if(numbytes < size)
	{// dorazilo mene dat nez je velikost bloku
		bytes_write = f->store_data(numbytes, source + 4, mode_tr);

		if(bytes_write != READ_ERR) return bytes_write;
		else return READ_ERR;
	}
	else
	{// dorazilo stejne, nebo vice, nez velikost bloku
		bytes_write = f->store_data(size, source + 4, mode_tr);

		if(bytes_write != READ_ERR) return bytes_write;
		else return READ_ERR;
	}
}

/******************************************************************************
	Zjisteni cisla potvrzujiciho bloku
*******************************************************************************/
int Client::ack_number(char *message)
{
	int number;
	unsigned char digit1 = message[2];
	unsigned char digit2 = message[3];

	number = (256 * digit1) + digit2;

	return number;
}

/******************************************************************************
	Zjisteni chyboveho kodu z error packetu
*******************************************************************************/
int Client::recv_error(char *source)
{
	int temp;

	temp = source[3];

	return temp;
}

/******************************************************************************
	Uvolneni vsech zdroju
*******************************************************************************/
void clean_sources(File *f, int sockfd)
{
	f->close_file(FILE_IN); // zavre se vstupni soubor
	f->close_file(FILE_OUT); // zavre se vystupni soubor
	close(sockfd); // uzavre se socket
	delete f; // uvolni se pamet file
}

/******************************************************************************
	Start obsluhy klienta

	Inspirace (vytvoreni socketu, bind):
	http://beej.us/guide/bgnet/output/html/multipage/clientserver.html#datagram
*******************************************************************************/
int Client::start(Opt *options, struct sockaddr_storage client_addr, int index, char *buf, int numbytes, int mtu)
{
	// Generovani nahodneho cisla pro port, na kterem bude klient obsluhovan
	srand (getpid());
	int port = rand() % MAX_PORT + MIN_PORT;

	client_port = to_string(port);
	service_ip = options->address_at(index);

	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	socklen_t addr_len;
	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

	signal(SIGINT, client_signal_reaction); // Registrace funkce pro odchyt signalu

	if ((rv = getaddrinfo(service_ip.c_str(), client_port.c_str(), &hints, &servinfo)) != 0)
	{
		cerr << "Error - Service " << service_ip << ", " << client_port << gai_strerror(rv) << endl;
		return EXIT_FAILURE;
	}

	for(p = servinfo; p != NULL; p = p->ai_next)
	{
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
			p->ai_protocol)) == -1)
		{
			continue;
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
		{
			close(sockfd);
			continue;
		}

		break;
	}

	if (p == NULL)
	{
		cerr << "Error - Service " << service_ip << ", " << client_port << " - failed to bind socket" << endl;
		return EXIT_FAILURE;
	}

	freeaddrinfo(servinfo);

	client_socket = sockfd; // Ulozim si deskr. socketu

	// Nastavi se vychozi timeout socketu
	if((set_socket_timeout(options, IMPLICIT_TIMEOUT)) == SET_FAILED) return EXIT_FAILURE;

	// ulozim si klientovu IP pro potreby vypisu informacnich hlasek
	string client_ip = inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), s, sizeof s);

	mtu_size = mtu; // Ulozim si MTU

	// Cinnost obsluhy serveru
	string err_msg;
	unsigned int data_counter = 0;		// Pocitadlo dat
	unsigned int block_counter = 0;		// Pocitadlo bloku
	int data_len;						// Pocet odeslanych B
	int mode;							// Mod prenosu
	int tryouts = 0;					// Pocet pokusu o znovu navazani komunikace
	bool reading_data = false;			// Flag cteni dat
	bool writing_data = false;			// Flag zapisu dat
	int ack;							// Cislo potvrzeneho bloku
	int type;							// Typ paketu
	int file_status;					// Info o uspesnosti operace se souborem
	int bytes_write;					// Pocet zapsanych bajtu
	int block_num;						// Cislo bloku
	int recv_status;					// Pocet prijatych B pri volani funkce recvfrom()
	char *file_buf;						// Buffer pro nacitani dat ze souboru
	int file_buf_size;// Velikost bufferu

	File *f = new File(); // Nova instance objektu pro praci se soubory

	while(1)
	{// Smycka obsluhy klienta

	   if(writing_data == true) type = packet_type(file_buf); // buffer na heapu
	   else type = packet_type(buf); // buffer na zasobniku

	   switch(type)
	   {// Podle typu paketu se provadi dana operace

			case RRQ:
			{// Zadost o cteni
				get_filename(buf); // ukladani jmena souboru

				// Zjisti se mod prenosu

				mode = transfer_mode(buf);

				if(mode == UNKNOWN_MODE)
				{// neznamy mod
				   err_msg = "Unknown mode";
				   send_error(sockfd, client_addr, err_msg, ERROR_IL);
				   cout << current_time() << client_ip << " Unknown mode, operation aborted" << endl;
					
					clean_sources(f, sockfd);
					return 1;
				}
				else if(mode == NETASCII)
				{
					cout << current_time() + client_ip + " Requested READ of " << filename << " [netascii]"<< endl;
				}
				else if(mode == OCTET)
				{
					cout << current_time() + client_ip + " Requested READ of " << filename << " [octet]"<< endl;
				}

				// Otevre se soubor pro cteni
				file_status = f->open(filename, options->working_path(), FILE_IN, mode_tr);

				if(file_status == NOT_FOUND)
				{// Soubor nebyl nalezen
					err_msg = "File not found";
					send_error(sockfd, client_addr, err_msg, ERROR_NF);
					cout << current_time() + client_ip + " File " << filename << " not found, operation aborted" << endl;

					clean_sources(f, sockfd);
					return 1;
				}
				else if(file_status == PERM_DEN)
				{// Nedostatecne opravneni
					err_msg = "Permission denided";
					send_error(sockfd, client_addr, err_msg, ERROR_AV);
					cout << current_time() << client_ip << " Permission denided, operation aborted" << endl;

					clean_sources(f, sockfd);
					return EXIT_FAILURE;
				}

				// Zjisti se pritomnost rozsireni

				if(opt_extension(buf, numbytes, options, f) == EXTENSION)
				{// request obsahuje options
					send_oack(sockfd, client_addr);
					
					reading_data = true;

					// Alokace bufferu pro nacitani souboru
					if(block_size.used == true)
					{// byla specifikovana velikost bloku
						file_buf = new char[block_size.value + 1];
						file_buf_size = block_size.value + 1;

						if(f->get_file_size() > MAX_BLOCK_COUNT * block_size.value)
						{// Soubor je prilis velky na prenos pres tftp
							err_msg = "File is too large to send";
							send_error(sockfd, client_addr, err_msg, ERROR_DF);
							cout << current_time() + client_ip + " File is too large to send. Operation aborted\n";
							
							clean_sources(f, sockfd);
							return EXIT_FAILURE;
						}
					}
					else
					{// mezi parametry nebyla velikost bloku, uvazuje se standardni
						file_buf = new char[STANDARD_BLOCK + 1];
						file_buf_size = STANDARD_BLOCK + 1;
					}
				}
				else
				{// bez parametru, posilani prvnich dat

					if(f->get_file_size() > MAX_BLOCK_COUNT * STANDARD_BLOCK)
					{// Soubor je prilis velky na prenos pres tftp
						err_msg = "File is too large to send";
						send_error(sockfd, client_addr, err_msg, ERROR_IL);
						cout << current_time() + client_ip + " File is too large to send. Operation aborted\n";
						
						clean_sources(f, sockfd);
						return 1;
					}

					// Alokace bufferu pro nacitani souboru - standardni velikost
					file_buf = new char[STANDARD_BLOCK + 1];

					reading_data = true;

					cout << current_time() + client_ip + " Sending DATA\n";

					// poslou se prvni data
					data_len = send_data(sockfd, client_addr, 1, STANDARD_BLOCK, f, file_buf);
					data_counter += data_len;

					if(data_len < STANDARD_BLOCK)
					{// Prvni blok je zaroven i posledni, tim konci cinnost
						reading_data = false;
					}
				}

				break;
			}

			case WRQ:
			{// Zadost o zapis

				get_filename(buf); // ulozim jmeno souboru

				// Zjistim mod prenosu

				mode = transfer_mode(buf);

				if(mode == UNKNOWN_MODE)
				{// neznamy mod
					err_msg = "Unknown transfer mode";
					send_error(sockfd, client_addr, err_msg, ERROR_IL);
					break;
				}
				else if(mode == NETASCII)
				{
					cout << current_time() << client_ip << " Requested WRITE of " << filename << " [netascii]" << endl;
				}
				else if(mode == OCTET)
				{
					cout << current_time() << client_ip << " Requested WRITE of " << filename << " [octet]" << endl;
				}

				// Otevre se soubor pro zapis

				if(mode == OCTET)
				{
					file_status = f->open(filename, options->working_path(), FILE_OUT, OCTET);
				}
				else if(mode == NETASCII)
				{
					file_status = f->open(filename, options->working_path(), FILE_OUT, NETASCII);
				}

				if(file_status == FILE_EXISTS)
				{// Soubor jiz existuje
					err_msg = "File already exists";
					send_error(sockfd, client_addr, err_msg, ERROR_AE);
					cout << current_time() << client_ip << " File " << filename << " already exists, operation aborted" << endl;
					clean_sources(f, sockfd);
					return 1;
				}
				else if(file_status == CANNOT_OPEN)
				{// Nelze otevrit
					err_msg = "Cannot open file for writing";
					send_error(sockfd, client_addr, err_msg, ERROR_IL);
					cout << current_time() << client_ip << " Cannot opet file " << filename << " for writing, operation aborted" << endl;
					clean_sources(f, sockfd);
					return 1;
				}

				// Zjisti se rozsireni

				if(opt_extension(buf, numbytes, options, f) == EXTENSION)
				{// request obsahuje options
					send_oack(sockfd, client_addr);

					// Alokace bufferu pro nacitani souboru
					if(block_size.used == true)
					{// byla specifikovana velikost bloku
						file_buf = new char[block_size.value + 10];
						file_buf_size = block_size.value + 10;
					}
					else
					{// mezi parametry nebyla velikost bloku, uvazuje se standardni
						file_buf = new char[STANDARD_BLOCK + 10];
						file_buf_size = STANDARD_BLOCK + 10;
					}	  

					writing_data = true;
					cout << current_time() + client_ip + " Receiving DATA\n";
				}
				else
				{// Bez rozsireni, zasle se ack 0
					send_ack(sockfd, client_addr, 0);

					// Alokace bufferu pro zapis souboru - standardni velikost
					file_buf = new char[STANDARD_BLOCK + 10];
					file_buf_size = STANDARD_BLOCK + 10;

					writing_data = true;
					cout << current_time() + client_ip + " Receiving DATA\n";
				}

				break;
			}

			case DATA:
			{// Datovy paket od klienta

				if(writing_data == true)
				{// Probiha prenos
					block_num = ack_number(file_buf); // zjisti se cislo bloku

					if((block_counter + 1) != block_num)
					{// Prisel blok, ktery nenavazuje na predchozi

						err_msg = "Error while file transfer";
						send_error(sockfd, client_addr, err_msg, ERROR_IL);

						break;
					}

					int actual_block;

					if(block_size.used == true)
					{// Pouziva se nestandardni velikost bloku
						actual_block = block_size.value;
						bytes_write = recv_data(file_buf, numbytes, actual_block, f); // zapisou se data
					}
					else
					{// Standardni velikost bloku
						actual_block = STANDARD_BLOCK;
						bytes_write = recv_data(file_buf, numbytes, actual_block, f); // zapisou se data
					}

					if(bytes_write >= 0)
					{// Zapis byl uspesny, potvrdi se klientovi
						send_ack(sockfd, client_addr, block_num);

						block_counter++; // zvetsi se pocitadlo ulozenych bloku
						data_counter += bytes_write; // pricte se pocet ulozenych dat k pocitadlu

						if((numbytes - 4) < actual_block)
						{// dat bylo min nez je velikost bloku
							writing_data = false;

							f->close_file(FILE_OUT); // uzavre se soubor pro zapis

							cout << current_time() << client_ip << " File " << filename << " has been stored [" << data_counter << " B, " << block_counter << " blocks]" << endl;

							clean_sources(f, sockfd);

							return EXIT_FAILURE;
						}
					}
					else if(bytes_write == READ_ERR)
					{// Zapis nebyl uspesny
						err_msg = "Error while writing to file";
						send_error(sockfd, client_addr, err_msg, ERROR_IL);
						clean_sources(f, sockfd);
						return EXIT_FAILURE;
					}
				}

				break;
			}

			case ACK:
			{// Potvrzeni od klienta, ze obdrzel konkretni datovy paket

				ack = ack_number(buf);

				if(ack == 0) cout << current_time() + client_ip + " Sending DATA\n";

				if(reading_data == true)
				{// prenos jeste nebyl dokoncen

					if(block_size.used == true)
					{// Pouziva se nestandardni velikost bloku
						data_len = send_data(sockfd, client_addr, ack + 1, block_size.value, f, file_buf);

						if(data_len == SEND_FAIL)
						{// Chyba pri odesilani
							cerr << current_time() << client_ip << " Error in sendto, operation aborted" << endl;
							clean_sources(f, sockfd);
							return EXIT_FAILURE;
						}
						else if(data_len < block_size.value) reading_data = false;
					}
					else
					{// Standardni velikost bloku
						data_len = send_data(sockfd, client_addr, ack + 1, STANDARD_BLOCK, f, file_buf);

						if(data_len == SEND_FAIL)
						{// Chyba pri odesilani
							cerr << current_time() << client_ip << " Error in sendto, operation aborted" << endl;
							clean_sources(f, sockfd);
							return EXIT_FAILURE;
						}
						else if(data_len < STANDARD_BLOCK) reading_data = false;
					}

					data_counter += data_len;
				}
				else
				{// Prenos byl dokoncen
					cout << current_time() << client_ip << " File " << filename << " has been sent [" << data_counter << " B, " << ack << " blocks]\n";

					clean_sources(f, sockfd);

					return EXIT_SUCCESS;
				}

				break;
			}

			case ERR:
			{// Error paket

				int err;

				if(reading_data == true)
				{// Klient poslal error pri cteni dat
					err = recv_error(buf);

					switch(err)
					{
						case ERROR_UN:
						{// Unknown transfer ID

							f->close_file(FILE_IN);
							cout << current_time() << client_ip << " Client aborted file read (transport error)" << endl;
							return EXIT_FAILURE;
						}

						case ERROR_DF:
						{// Disk full
							f->close_file(FILE_IN);
							cout << current_time() << client_ip << " Client aborted file read (too large)" << endl;
							return EXIT_FAILURE;
						}

						case ERROR_ND:
						{// Nedefinovana chyba
							f->close_file(FILE_IN);
							cout << current_time() << client_ip << " Client aborted file read (undefined error)" << endl;
							return EXIT_FAILURE;
						}
					}
				}
				else if(writing_data == true)
				{// Klient poslal error pri zapisu dat
					err = recv_error(buf);

					switch(err)
					{
						case ERROR_UN:
						{// Unknown transfer ID
							f->close_file(FILE_OUT);
							cout << current_time() << client_ip << " Client aborted file write" << endl;
							return EXIT_FAILURE;
						}

						case ERROR_ND:
						{// Nedefinovana chyba
							f->close_file(FILE_OUT);
							cout << current_time() << client_ip << " Client aborted file read (undefined error)" << endl;
							return EXIT_FAILURE;
						}
					}
				}

				break;
			}

			case UNKNOWN_OPCODE:
			{// Neznamy opcode
				err_msg = "Unknown request";
				send_error(sockfd, client_addr, err_msg, ERROR_IL);
				clean_sources(f, sockfd);
				return EXIT_FAILURE;
			}

		}// konec switch(type)

		if(writing_data == true)
		{// Pokud probiha zapis, pouziva se vetsi buffer alokovany na halde
			recv_status = recv_packet(sockfd, client_addr, file_buf, file_buf_size);
		}
		else
		{// Pri cteni staci mensi velikost na zasobniku
			recv_status = recv_packet(sockfd, client_addr, buf, BUFSIZE);
		}

		if(recv_status == TIMEOUT_ELAPSED)
		{// Vyprsel cas cekani, probehne 3x pokus o znovu navazani komunikace
			tryouts++;

			if(tryouts == 3)
			{// Probehly tri pokusy o znovu navazani komunikace
				cout << current_time() << client_ip << " Timeout elapsed, operation aborted" << endl;
				clean_sources(f, sockfd);
				return 1;
			}
			else
			{// Probehne pokus
				cout << current_time() << client_ip << " Timeout elapsed, retrying..." << endl;
			}
		}
		else
		{// Byl normalne prijat paket
			numbytes = recv_status;
			tryouts = 0; // Vynuluje se pocitadlo neuspesnych pokusu
		}
	}
}

/******************************************************************************
	Start sluzby (TFTP serveru)

	Inspirace (vytvoreni socketu, bind):
	http://beej.us/guide/bgnet/output/html/multipage/clientserver.html#datagram
*******************************************************************************/
int Service::start(Opt *options, int index)
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int numbytes;
	int mtu;
	struct sockaddr_storage client_addr;
	char buf[BUFSIZE];
	socklen_t addr_len;
	pid_t process;

	signal(SIGINT, server_signal_reaction); // Registrace funkce pro odchyt signalu SIGINT
	signal(SIGCHLD, client_end);			// Registrace funkce pro odchyt signalu SIGCHLD

	// Adresa a port sluzby
	string port_str = to_string(options->port_at(index));
	string service_ip = options->address_at(index);

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

	if ((rv = getaddrinfo(service_ip.c_str(), port_str.c_str(), &hints, &servinfo)) != 0)
	{
		cerr << "Error - Service " << service_ip << "," << port_str << " - " << gai_strerror(rv) << endl;
		exit(1);
	}

	for(p = servinfo; p != NULL; p = p->ai_next)
	{
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
		{
			continue;
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
		{
			close(sockfd);
			continue;
		}

		break;
	}

	if (p == NULL)
	{// Nepodarilo se nabindovat socket
		cerr << "Error - Service " << service_ip <<	 "," << port_str << " - failed to bind socket" << endl;
		exit(1);
	}

	freeaddrinfo(servinfo);

	// Zjistim velikost MTU rozhrani, na kterem bezi sluzba
	if((mtu = get_mtu_size(sockfd, options->address_at(index))) == MTU_FAILED)
	{
		cerr << "Error - Service " << service_ip << "," << port_str << " - failed to get MTU size" << endl;
		return EXIT_FAILURE;
	}	

	// Vypis uvodnich informaci

	cout << current_time() << "Server started, ip: " << service_ip << "," << port_str;

	if(options->block_size() == 0)
	{// Nezadana velikost
		cout << " block: " << mtu - 32 << "B";
	}
	else
	{// Byla zadana
		cout << " block: " << options->block_size() << "B";
	}

	if(options->max_timeout() == 0)
	{// Nezadan timeout
		cout << " timeout: " << IMPLICIT_TIMEOUT << "s";
	}
	else
	{// Byl zadan
		cout << " timeout: " << options->max_timeout() << "s";
	}

	cout << endl;

	while(1)
	{// Smycka sluzby

		addr_len = sizeof client_addr;

		if ((numbytes = recvfrom(sockfd, buf, BUFSIZE , 0, (struct sockaddr *)&client_addr, &addr_len)) == -1)
		{// Chyba pri prijeti zpravy
			cerr << "Error - Service " << service_ip <<	 "," << port_str << " - recvfrom failed" << endl;
			exit(1);
		}

		process = fork();

		if(process == 0)
		{// Potomek, zacina obsluha klienta

			close(sockfd); // uzavre se puvodni socket, v potomkovi jiz neni potreba

			Client *a = new Client(); // nova instance obsluhy klienta

			a->start(options, client_addr, index, buf, numbytes, mtu);

			delete a;

			exit(0);
		}
		else
		{// Ulozi se PID potomka do vektoru
			children.push_back(process);
		}
	}

	close(sockfd);
}