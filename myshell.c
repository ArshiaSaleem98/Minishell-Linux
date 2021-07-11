#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "parser.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
extern int errno;
int back_process[1024];
int num_back = 0;

void nozombie(int sig){

	int kidpid = waitpid(-1, NULL, WNOHANG);
}

//Función para ejecutar el mandato cd
void mycd (tline * line)
{
	//Variables
	char * dir;
	char buf[512];

	//Comando cd sin argumentos
	if (line->commands[0].argv[1] == NULL) {
		//Nos lleva a HOME
		dir = getenv("HOME");
	}
	//Si el comando cd va acompañado de argumentos
	else {
		dir = line->commands[0].argv[1];
	}

	//Comprobamos si es un directorio
        if (chdir(dir) != 0) {
             	fprintf(stderr,"Error al cambiar de directorio: %s\n", strerror(errno));
        }

	printf( "El directorio actual es: %s\n", getcwd(buf,-1));

}

//Función para ejecutar otros comandos
void myothercommands(tline * line, char * redirect_input, char * redirect_output, char * redirect_error)
{
	//Si es solo un comando
	if (line->ncommands == 1) {
		//Comprobamos si el comando es válido
        	if ((line->commands[0].filename == NULL)) {
                	fprintf(stderr, "%s. Error en el mandato\n", line->commands[0].argv[0]);
           	}

		//Variables
		pid_t pid;
		int status;

		pid = fork();

		if (pid < 0) {
			fprintf(stderr, "Falló el fork(). %s\n", strerror(errno));
        		exit(-1);
        	}
		//Proceso Hijo
		else if (pid == 0) {
                	execvp(line->commands[0].filename, line->commands[0].argv);
			//Error en el execvp
                	fprintf(stderr, "%s. Error en el execvp\n", strerror(errno));
		        exit(1);
		}
		//Proceso Padre
		else {
                	wait(&status);
		}
	}
	//Fin de un solo  comando

	//Para dos comandos
	else if (line->ncommands == 2) {
		//Variables
		int p[2];
		int pid;

		pipe(p);
		pid = fork();

                //Primer hijo
                if (pid < 0) {
			fprintf(stderr, "Falló el fork(). %s\n", strerror(errno));
                        exit(-1);
                }

		if (pid == 0) {
			close(p[0]);
			dup2(p[1],1);
			//Comprobar si el mandato es válido
                        if ((line->commands[0].filename == NULL)) {
	              	        fprintf(stderr, "%s. Error en el mandato\n", line->commands[0].argv[0]);
                        }
                  	execvp(line->commands[0].filename, line->commands[0].argv);
                        //Error en el execvp
			fprintf(stderr, "%s. Error en el execvp\n", strerror(errno));
			exit(1);
		}
        	else {
			pid = fork();

			//Segundo hijo
			if (pid == 0) {
				close(p[1]);
				dup2(p[0], 0);
				//Comprobar si el mandato es válido
                                if ((line->commands[1].filename == NULL)) {
                                	fprintf(stderr, "%s. Error en el mandato\n", line->commands[1].argv[0]);
                                }
                            	execvp(line->commands[1].filename, line->commands[1].argv);
                                //Error en el execvp
				fprintf(stderr, "%s. Error en el execvp\n", strerror(errno));
				exit(1);
			}
			//Padre
			else {
				close(p[0]);
				close(p[1]);
				wait(NULL);
				wait(NULL);
			}
		}

	}
	//Fin de dos comandos

	//Para más de dos comandos
	else if (line->ncommands > 2) {
		//Variables
		int p[line->ncommands][2];
                int pid;
                int status;


		for(int i=0;i<=line->ncommands;i++) {
			pipe(p[i]);
		}

		for(int i=0;i<=line->ncommands;i++) {

        		pid = fork();

	                if (pid < 0) {
                                fprintf(stderr, "Falló el fork(). %s\n", strerror(errno));
                                exit(-1);
        	        }

			if(pid == 0) {


		        	for(int j=0;j<=line->ncommands;j++) {
					if(j != i)
		                                close(p[j][0]);
					if(j != i + 1)
        		                        close(p[j][1]);
				}

				if(i == 0) {
					//Primer hijo
					close(p[i][0]);
					dup2(p[i+1][1], 1);
				} else if(i == line->ncommands - 1) {
					//Último hijo
					dup2(p[i][0], 0);
				} else {
					//Cualquier  otro hijo
					dup2(p[i][0], 0); //redirigir stdin
					dup2(p[i+1][1], 1); //redirigir stout
				}
				//Comprobar si el mandato es válido
                	        if ((line->commands[i].filename == NULL)) {
                        	        fprintf(stderr, "%s. Error en el mandato\n", line->commands[i].argv[0]);
	                        }
				execvp(line->commands[i].filename, line->commands[i].argv);
				//Error en el execvp
	        	        fprintf(stderr, "%s. Error en el execvp\n", strerror(errno));
                        	exit(1);
			}

        		//Padre
	                else {
				close(p[i][0]);
				close(p[i][1]);
                 	            wait(NULL);



	       		}
               }//Fin for
               if (line->background==1){//si es background va a imprimir el pid  
                            printf("[%d]\n ",pid); 
                            back_process[num_back] = pid;
			    num_back++;
	       }else{ //si no es background
			    wait(&status);
	       }

        }
	//Fin de mas de dos comandos
}

int main(void)
{
	//Variables
	char buf[1024];
	tline * line;
        int fd;

	//Guardar entrada estándar para las redirecciones
	int guardar_redirect_input = dup(fileno(stdin));
	//Guardar salida estándar para las redirecciones
	int guardar_redirect_output = dup(fileno(stdout));
	//Guardar el error estándar para las redirecciones
	int guardar_redirect_error = dup(fileno(stderr));

        signal(SIGCHLD, nozombie);

	printf("msh> ");

	while (fgets(buf, 1024, stdin)) {

		line = tokenize(buf);

		if (line == NULL) {
			continue;
		}

		/* Redirección entrada: envía el contenido del fichero
			especificado para utilizarlo como entrada
					estándar. */
		if (line->redirect_input != NULL) {
		        //Se abre el fichero para lectura
		        fd = open (line->redirect_input, O_RDONLY);

		        if (fd == -1) {
                		printf("%s: Error. Fallo al abrir el fichero.\n", line->redirect_input);
		        }
		        else {
                		//Redirigimos la entrada estándar desde archivo
		                dup2(fd, 0);
                		close(fd);
		        }
		}

		/* Redirección salida: crea un nuevo fichero que contiene
			la salida estándar. Si el fichero especificado
				existe,	se sobrescribe */
		if (line->redirect_output != NULL) {
			//Se crea el fichero
		        fd = creat (line->redirect_output, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

		        if (fd == -1) {
                		printf("%s: Error. Fallo al crear el fichero.\n", line->redirect_output);
		        }
		        else {
                		//Redirigimos la salida estándar al archivo
		                dup2(fd, 1);
                		close(fd);
		        }
		}

		/* Redirección error: crea un nuevo fichero que contiene
			tanto la salida estándar como el error estándar.
			Si el fichero especificado existe se sobreescribe */
		if (line->redirect_error != NULL) {
			//Se crea el fichero
		        fd = creat(line->redirect_error, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

		        if (fd == -1) {
                		printf("%s: Error. Fallo al crear el fichero.\n", line->redirect_error);
		        }
		        else {
                		//Redirigimos el error estándar al archivo
		                dup2(fd, 2);
                		close(fd);
		        }
  		}

                if(line->background){//si es  ejecuta en background se va a ignorar 
                       signal(SIGINT, SIG_IGN);
                       signal(SIGQUIT, SIG_IGN);

                }else{ //si no , accion por defecto
                       signal(SIGINT, SIG_DFL);
                       signal(SIGQUIT, SIG_DFL);

               }

		//El comando introducido es cd
                if (strcmp(line->commands[0].argv[0], "cd") == 0 ) {
			mycd(line);
                }

		//Para uno o más de un comando distinto de cd
                else if((line->ncommands >= 1) & !(strcmp(line->commands[0].argv[0], "jobs") == 0 )){
                        myothercommands(line, line->redirect_input, line->redirect_output, line->redirect_error);
                }

                else if(strcmp(line->commands[0].argv[0], "jobs") == 0 ){
                        for(int i=0; i<num_back; i++) 
                            printf("[%i]+ Running PID: %i\n", i+1, back_process[i]);
                }



		//Restablecer redirección entrada
		if(line->redirect_input != NULL){
			dup2(guardar_redirect_input, fileno(stdin));
		}
		//Restablecer redirección salida
		if(line->redirect_output != NULL){
			dup2(guardar_redirect_output, fileno(stdout));
		}
		//Restablecer redirección error
		if(line->redirect_error != NULL){
			dup2(guardar_redirect_error, fileno(stderr));
		}

		printf("msh> ");
	}

	return 0;
}
