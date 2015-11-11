#include "libraries.h"
#include "options.h"
#include "server.h"

vector<pid_t> main_children; // vektor PID potomku, globalni

/******************************************************************************
	Reakce na signál SIGINT
*******************************************************************************/
void main_signal_reaction(int sig)
{
	int count = main_children.size();
	int status;
	pid_t pid;

	if(count == 0)
	{// Pouze jedna instance
		cout << "\nService has been terminated\n";
		exit(0);
	}

	for(int i=0; i<count; i++)
	{
		kill(main_children.at(i), SIGINT); // Zasle potomkovi SIGINT
		waitpid(-1, &status, 0); // Cekani na jakehokoli potomka, az se ukonci
	}

	cout << "\nAll services have been terminated\n";
	exit(0);
}


int main(int argc, char *argv[])
{
	Opt *options = new Opt();
	int count,i,status,opt_status;
	pid_t process;

	signal(SIGINT, main_signal_reaction); // Registrace funkce pro odchyt signalu

	opt_status = options->load(argc,argv);

	if(opt_status == HELP) return EXIT_SUCCESS; // Vypis napovedy a konec
	else if(opt_status == OPTION_FAIL) return EXIT_FAILURE; // Chyba v parametrech, konec

	if((options->check()) == OPTION_FAIL) return EXIT_FAILURE;

	vector<Service*> services;

	count = options->addresses.size();
	if(count == 1)
	{// jedna sluzba

		Service *a = new Service();
		a->start(options, 0);

		delete a;
		delete options;
	}
	else
	{// vice sluzeb

		for(i=0; i<count; i++)
		{
			process = fork();

			if(process == 0)
			{// Nova sluzba
				Service *a = new Service();
				a->start(options, i);

				delete a;
				delete options;

				exit(0); // sluzba konci
			}
			else
			{// Rodic ulozi pid potomka do vektoru
				main_children.push_back(process);
			}
		}

		for(i=0; i<count; i++)
		{// Cekani na ukonceni sluzeb (v normalnim pripade)

			waitpid(-1, &status, 0); // Cekani na jakehokoli potomka, az se ukonci
		}		
	}

	return EXIT_SUCCESS;
}