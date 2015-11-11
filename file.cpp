#include "file.h"

using namespace std;

/******************************************************************************
	Navrat velikosti souboru v bajtech
*******************************************************************************/
int File::get_file_size(void)
{
	return file_size;
}

/******************************************************************************
	Ukladani dat do souboru
*******************************************************************************/
int File::store_data(int size, char *source, int type)
{
	int bytes_write;
	char temp;
	int counter;		// Index v prijate zprave
	int byte_counter;	// Pocitadlo skutecne zapsanych B

	if(type == NETASCII)
	{
		counter = 0;
		byte_counter = 0;

		while(counter < size)
		{
			if(source[counter] == '\r' && source[counter + 1] == '\n')
			{// CRFL
				fputc('\n',out);
				counter += 2;
				byte_counter++;
			}
			else if(source[counter] == '\r' && source[counter + 1] == '\0')
			{// CRNULL
				fputc('\r',out);
				counter += 2;
				byte_counter++;
			}
			else
			{// Jiny znak
				fputc(source[counter],out);
				counter++;
				byte_counter++;
			}
		}

		return byte_counter;
	}
	else if(type == OCTET)
	{
		bytes_write = fwrite(source, 1, size, out); // zapsat blok do souboru

		if(bytes_write == size) return bytes_write;
		else return READ_ERR;
	}
}

/******************************************************************************
	Cteni dat ze souboru
*******************************************************************************/
int File::get_data(int size, int block_num, char *dest, int type)
{
	int pos;
	int next_pos;
	int counter;
	char temp;

	if(type == NETASCII)
	{
		counter = 0; // Inicializovat citac znaku

		if(last_char == '\n' || last_char == '\0')
		{// Zapsat posledni znak, ktery se nevlezl do predchoziho bloku
			dest[counter++] = last_char;
			last_char = '0';
		}

		if(block_num > 1)
		{// Navrat o jeden znak zpet
			fseek(in, -1, SEEK_CUR);
		}

		while((temp = fgetc(in)) != EOF)
		{
			if(temp == '\n')
			{// Zmenit na CRLF
				dest[counter++] = '\r';

				if(counter > size)
				{// Tento znak se musi pridat na zacatek dalsiho bloku
					last_char = '\n';
					break;
				}

				dest[counter++] = '\n';

				if(counter > size) break;
			}
			else if(temp == '\r')
			{// Zmenit na CRNULL
				dest[counter++] = '\r';

				if(counter > size)
				{// Tento znak se musi pridat na zacatek dalsiho bloku
					last_char = '\0';
					break;
				}

				dest[counter++] = '\0';

				if(counter > size) break;
			}
			else
			{// Jakykoliv jiny znak
				dest[counter++] = temp;

				if(counter > size) break;
			}
		}

		if(counter == 0) return 0;
		else return counter; // Vrati pocet skutecne zapsanych znaku

	}
	else if(type == OCTET)
	{
		if(block_num == 1)
		{// Prvni blok

			if(file_size < size)
			{// soubor je mensi nez jedna velikost jednoho bloku

				fseek(in, 0, SEEK_SET); // nastavi se ukazatel na zacatek souboru			

				bytes_read = fread(dest, 1, file_size, in); // precte se cely soubor

				if(bytes_read == file_size) return bytes_read;
				else return READ_ERR;
			}
			else
			{// soubor je vetsi nebo roven jednomu bloku

				fseek(in, 0, SEEK_SET); // nastavi se ukazatel na zacatek souboru			

				bytes_read = fread(dest, 1, size, in); // precte se blok

				if(bytes_read == size) return bytes_read;
				else return READ_ERR;
			}
		}
		else
		{// n-ty blok
			next_pos = ((block_num - 1) * size); // zacatek dalsiho bloku v souboru

			if(next_pos >= file_size)
			{// cteni dalsiho bloku jiz neni mozne
				dest[0] = '\0';
				return 0;
			}

			fseek(in, next_pos, SEEK_SET); // presunout ukazatel na novou pozici od zacatku

			if((file_size - next_pos) < size)
			{// zbyvajici data jsou mensi nez jeden blok
				bytes_read = fread(dest, 1, (file_size - next_pos), in); // precte se ten zbytek

				if(bytes_read == (file_size - next_pos)) return bytes_read;
				else return READ_ERR;
			}
			else
			{// zbyva jeste dost dat, takze se precte blok od dane pozice
				bytes_read = fread(dest, 1, size, in);

				if(bytes_read == size) return bytes_read;
				else return READ_ERR;
			}
		}
	}
}

/******************************************************************************
	Otevreni souboru
*******************************************************************************/
int File::open(string name, string path, int mode, int type)
{
	string f;

	if(mode == FILE_IN)
	{// otevrit vstupni soubor

		if(in != NULL)
		{// uz je nejaky otevreny
			fclose(in);
			in = NULL;
		}

		if(name.front() != '/' && path.back() != '/')
		{// Je nutne doplnit lomitko, aby cesta byla spravna
			f = path + "/" + name;
		}
		else
		{// Cestu a nazev souboru staci beze zmen spojit
			f = path + name;
		}

		if(type == OCTET)
		{// Otevrit soubor v modu cteni bytu
			in = fopen(f.c_str(),"rb");
		}
		else if(type == NETASCII)
		{// Otevrit soubor v modu cteni znaku
			in = fopen(f.c_str(),"r");
		}

		if(in == NULL)
		{// Test zda otevreni probehlo uspesne

			if(errno == EACCES)
			{// Pristup odepren
				return PERM_DEN;
			}
			else if(errno == ENOENT)
			{// Soubor neexistuje
				return NOT_FOUND;
			}
		}

		// zjistit velikost souboru
		fseek (in , 0 , SEEK_END);
		file_size = ftell (in);
		rewind (in);
	}

	else if(mode == FILE_OUT)
	{// otevrit vystupni soubor

		string f = path + name;
		FILE *temp;

		// Otestovani jestli soubor uz neexistuje
		temp = fopen(f.c_str(),"r");

		if(temp != NULL)
		{// Soubor jiz existuje
			fclose(temp);
			return FILE_EXISTS;
		}

		if(out != NULL)
		{// uz je nejaky otevreny
			fclose(out);
			out = NULL;
		}

		if(type == OCTET)
		{// Zapis bajtu
			out = fopen(f.c_str(),"wb");
		}
		else if(type == NETASCII)
		{// Zapis znaku
			out = fopen(f.c_str(),"w");
		}

		if(out == NULL) return CANNOT_OPEN;
	}

	return OPEN_OK;
}

/******************************************************************************
	Smazani souboru
*******************************************************************************/
void File::remove_file(string name)
{
	if((remove(name.c_str())) != 0)
	{// Nepodarilo se odstranit soubor
		cerr << "Removing file error\n";
	}
}

/******************************************************************************
	Uzavreni souboru
*******************************************************************************/
void File::close_file(int type)
{
	if(type == FILE_IN && in != NULL)
	{
		fclose(in);
		in = NULL;
	}
	else if(type == FILE_OUT && out != NULL)
	{
		fclose(out);
		out = NULL;
	}
}