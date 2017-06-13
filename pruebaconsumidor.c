#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <pthread.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <string.h>

#define TAM_MEMORIA 4

#define MAIN_PRODUCER 0
#define MAIN_SPOOLER 1
#define MEM_PRODUCER1 2
#define MEM_SPOOLER1 3
#define MEM_PRODUCER2 4
#define MEM_SPOOLER2 5
#define MEM_PRODUCER3 6
#define MEM_SPOOLER3 7
#define MEM_PRODUCER4 8
#define MEM_SPOOLER4 9



int semid_llamadas;
int shmid_llamadas;

struct{
    unsigned short int sem_num;
    short int sem_op;
    short int sem_flg;
}sembuf;

typedef struct{
	int flag;
	char operacion[161];
	char compania[11];
	char usuario[21];
}Zona;

Zona* mem_llamadas;

union semun{
    int val;                /* value for SETVAL */
    struct semid_ds *buf;   /* buffer for IPC_STAT & IPC_SET */
    unsigned short int *array; /* array for GETALL & SETALL */
    struct seminfo *__buf;  /* buffer for IPC_INFO */
};

void init_semaphores(unsigned short int* sem_array,int semid){
    union semun semopts;
    semopts.array = sem_array;

	if(semctl(semid,0,SETALL,semopts) == -1){
		perror("semctl");
		exit(1);
	}
}

void open(int sem_num){
	struct sembuf sem_open;
	sem_open.sem_num = sem_num;
    sem_open.sem_op = 1;
    sem_open.sem_flg = SEM_UNDO; //Free resource of the semaphore
    if (semop(semid_llamadas,&sem_open,1) == -1) {
        perror("semop");
        exit(1);
    }
}

void lock(int sem_num){
	struct sembuf sem_lock;
	sem_lock.sem_num = sem_num;
    sem_lock.sem_op = -1;  //Block the calling process until the value of the semaphore is greater than or equal to the absolute value of sem_op.
    sem_lock.sem_flg = SEM_UNDO;

	if (semop(semid_llamadas,&sem_lock,1) == -1) {
        perror("semop");
        exit(1);
    }
}

void remove_semaphores(){
	union semun semopts;
	if (semctl(semid_llamadas,0,IPC_RMID,semopts) == -1) {
        perror("semctl");
        exit(1);
    }
    printf("Destruyo semaforos\n");
}

void linkmemory(Zona* memoria,int shmid){
  //Se enlaza el arreglo de zonas a la memoria creada
	  if((mem_llamadas = (Zona*)shmat(shmid,(Zona*) 0,SHM_EXEC)) == (void *)-1) { 
	      perror("shmat");
	      exit(1);
	  }
}

void destroymemory(){
  //Terminada de usar la memoria compartida se libera
  if(shmid_llamadas != 0 || shmid_llamadas != -1){ //si la memoria ya fue destruida solo se des asocia de ella
    shmdt (mem_llamadas);//des asocia la memoria compartida de la zona de datos de nuestro programa
    shmctl (shmid_llamadas, IPC_RMID, (struct shmid_ds *)NULL);//destruye la zona de memoria compartida
    //printf("\nLa memoria compartida ha sido destruida OUT\n");
    printf("Destruyo la memoria\n");
  }
  else{
    shmdt(mem_llamadas);//desasocia la memoria compartida de la zona de datos de nuestro programa
    printf("Se Desligo de la memoria\n");
  }
}

void consumememory(int indexmem){
	unsigned char bufferdata[191];
	strcpy(bufferdata,mem_llamadas[indexmem].operacion);
	strcat(bufferdata,mem_llamadas[indexmem].compania);
	strcat(bufferdata,mem_llamadas[indexmem].usuario);
	printf("%s\n",bufferdata);
}

void consumecalls(){
	int sem_memvalue;
	int loop;
	for(int i=0;i<15;i++){
		//Zonas Criticas
			lock(MAIN_SPOOLER);
				loop = 1;
				do{
					sem_memvalue = semctl(semid_llamadas,MEM_SPOOLER1,GETVAL,NULL);
					if(sem_memvalue!=0){
						open(MAIN_SPOOLER);
						//Zona de memoria 1
						lock(MEM_SPOOLER1);
						consumememory(0);
						open(MEM_PRODUCER1);
						loop = 0;
					}
					else{
						sem_memvalue = semctl(semid_llamadas,MEM_SPOOLER2,GETVAL,NULL);
						if(sem_memvalue!=0){
							open(MAIN_SPOOLER);
							//Zona de memoria 2
							lock(MEM_SPOOLER2);
							consumememory(1);
							open(MEM_PRODUCER2);
							loop = 0;
						}
						else{
							sem_memvalue = semctl(semid_llamadas,MEM_SPOOLER3,GETVAL,NULL);
							if(sem_memvalue!=0){
								open(MAIN_SPOOLER);
								//Zona de memoria 3
								lock(MEM_SPOOLER3);
								consumememory(2);
								open(MEM_PRODUCER3);
								loop = 0;
							}
							else{
								sem_memvalue = semctl(semid_llamadas,MEM_SPOOLER4,GETVAL,NULL);
								if(sem_memvalue!=0){
									open(MAIN_SPOOLER);
									//Zona de memoria 4
									lock(MEM_SPOOLER4);
									consumememory(3);
									open(MEM_PRODUCER4);
									loop = 0;
								}
							}
						}
					}	
				}while(loop);
	}
}


void *consumedata(void *arg){
	consumecalls();
}


int main(int argc, char const *argv[]){
	//Creacion de los semaforos  
	key_t key_llamadas;
	key_t key_mensajes;

	unsigned short int  sem_arrayllamadas[10] = {1,1,1,0,1,0,1,0,1,0};

	//Creacion de una llave unica ligada al archvo especificado
	if((key_llamadas = ftok("/bin/ls",'l')) == -1){
		perror("Error al establecer la llave");
		exit(1);
	}

	//Creamos un SET de 10 semaforos para las llamadas
	if((semid_llamadas = semget(key_llamadas,10,0666 | IPC_CREAT | IPC_EXCL)) == -1){
		if((semid_llamadas = semget(key_llamadas,10,0666))==-1){
			perror("semget");	
			exit(1);	
		}
		else{
			printf("Consumidor: Me ligue exitosamente a los semaforos de las llamadas\n");

		}
	}
	else{
		//Inicializamos los semaforos
		printf("Consumidor: Cree exitosamente los semaforos de las llamadas\n");
		init_semaphores(sem_arrayllamadas,semid_llamadas);
	}


	//Creacion de la memoria compartida para llamadas
	  if((shmid_llamadas = shmget(key_llamadas,sizeof(Zona)*TAM_MEMORIA,IPC_CREAT|IPC_EXCL|0666)) == -1){
	      /* El segmento ya existe - abrimos como consumidor*/
	      if((shmid_llamadas = shmget(key_llamadas,sizeof(Zona)*TAM_MEMORIA,0)) == -1){
		       perror("shmget");
		       exit(1);
	      }
	      else{
	        printf("Consumidor: Me ligue exitosamente a la memoria de las llamadas\n");
	      }
	  }
	  else{
	    printf("Consumidor: Cree exitosamente la memoria de las llamadas\n");
	  }

	  linkmemory(mem_llamadas,shmid_llamadas);

	//Creacion de los hilos productores (Pit y Hugue)
	pthread_t consumidores[2];
	int param[2];

	for(int i=0;i<2;i++){
		param[i] = i;
		if((pthread_create(&consumidores[i],NULL,consumedata,NULL) != 0)){
			perror("pthread_create");
			exit(1);
		}
	}

	for(int j=0;j<2;j++){
		if((pthread_join(consumidores[j],NULL)) == -1){
			perror("pthread_join");
			exit(1);
		}
	}

	remove_semaphores();
	destroymemory();

	return 0;
}