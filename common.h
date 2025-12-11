#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <time.h>
#include <signal.h>

#define N 12
#define MAX_DRONOW (N*2)
#define POJEMNOSC_BAZY 4
#define BAT_CRITICAL 20
#define MAX_CYKLI 5

#define FTOK_PATH "common.h"
#define FTOK_ID 'R'

//---STANY DRONA---
#define STAN_WOLNY 0      //Dron nie istnieje
#define STAN_LADOWANIE 1  //Dron jest w bazie i ładuje baterię
#define STAN_LOT 2        //Dron lata poza bazą
#define STAN_POWROT 3     //Dron czeka przed wejśceim do bazy
#define STAN_ATAK 4       //Przeprowadza atak samobójczy
#define STAN_ZNISZCZONY 5 //Bateria padła lub atak samobójczy

struct Dron {
    pid_t pid;
    int bateria;
    int liczba_cykli;
    int stan;
    int id_wewnetrzne;
};

struct StanRoju {
    struct Dron drony[MAX_DRONOW];
    int pojemnosc_bazy;
    int aktualny_limit_dronow;
};

#define TYP_DODAJ_PLATFORMY 1
#define TYP_USUN_PLATFORMY 2

struct Komunikat {
    long mtype;
};

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

#define SEM_BAZA 0
#define SEM_WEJSCIE_1 1
#define SEM_WEJSCIE_2 2
#define SEM_PAMIEC 3
#define ILOSC_SEMAFOROW 4

#endif