/*
 * memoria.h
 *
 *  Created on: 31 may. 2020
 *      Author: utnso
 */

#ifndef MEMORIA_H_
#define MEMORIA_H_
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdbool.h>
#include <commons/collections/queue.h>
#include <commons/config.h>
#include <commons/log.h>
#include <messages_lib/messages_lib.h>
#include "broker.h"
#include <time.h>

typedef struct {
	uint32_t idMensaje;
	uint32_t cola;
	listaMutex* subsYaEnviado;
	listaMutex* subsACK;
	uint32_t sizeStream;
	void* stream;
	uint32_t modulo;
}msgMemoriaBroker;

typedef enum{
	LIBRE,
	OCUPADO,
	PARTICIONADO
}nodeStatus;

typedef struct{
	nodeStatus status;
	uint32_t size;
	struct tm tiempoDeCarga;
	struct tm ultimoAcceso;
}nodeData;

struct nodoMemoria {
	nodeData header;
	msgMemoriaBroker* mensaje;
	uint32_t offset;
	struct nodoMemoria* hijoIzq;
	struct nodoMemoria* hijoDer;
	struct nodoMemoria* padre;
};

typedef enum{
	PARTICIONES_DINAMICAS,
	BUDDY_SYSTEM
}algoritmoMem;

typedef enum{
	FIRST_FIT,
	BEST_FIT
}algoritmoParticiones;

typedef enum{
	FIFO,
	LRU
}algoritmoReemp;

typedef enum {
	CONFIRMADO,
	SUBSYAENVIADOS
}ListasMemoria;

typedef struct{
	t_config* config;
	char* configAtributo;
	char* OPCION1;
	char* OPCION2;
	uint32_t OP1;
	uint32_t OP2;
	char* error;
} algoritmoParameter;

struct nodoMemoria* nodoRaizMemoria;
listaMutex* particionesOcupadas;
listaMutex* particionesLibres;
listaMutex* nodosOcupados;

uint32_t auxTamanioStreamGlobal;

void definirComienzoDeMemoria();

struct nodoMemoria* crearRaizArbol(void);
struct nodoMemoria* inicializarNodo();
void liberarNodo(struct nodoMemoria* nodo);

void registrarMensajeEnMemoria(uint32_t idMensaje, paquete* paq, algoritmoMem metodo);
void registrarEnMemoriaBUDDYSYSTEM(msgMemoriaBroker* mensajeNuevo, struct nodoMemoria* partActual);

void particionarMemoriaBUDDY(struct nodoMemoria*);
void evaluarTamanioParticion(struct nodoMemoria* partActual, msgMemoriaBroker* msg);
uint32_t evaluarTamanioParticionYasignar(struct nodoMemoria* partActual, msgMemoriaBroker* msg);

void guardarEnListaMemoria(uint32_t idmensaje, uint32_t socket, uint32_t lista);
msgMemoriaBroker* buscarMensajeEnMemoria(uint32_t idMensajeBuscado);
msgMemoriaBroker* buscarMensajeEnMemoriaBuddy(uint32_t id);
msgMemoriaBroker* buscarMensajeEnParticionesOcupadas(uint32_t idMensajeBuscado);
msgMemoriaBroker* buscarPorRama(uint32_t id, struct nodoMemoria* partActual );
bool buscarPorRamaGet(mensajeGet* msgGet, struct nodoMemoria* nodoActual );
bool buscarPorRamaCatch(mensajeCatch*  msgCatch, struct nodoMemoria* nodoActual );

uint32_t intentarRamaIzquierda(msgMemoriaBroker* mensajeNuevo, struct nodoMemoria* partActual);

uint32_t tamanioParticion(struct nodoMemoria* part);
uint32_t tamanioMinimo(struct nodoMemoria* partActual);
bool noEsParticionMinima(struct nodoMemoria* particion);
bool estaLibre(struct nodoMemoria* particion);
bool estaEnLista(uint32_t socket, ListasMemoria lista, msgMemoriaBroker* mensaje);
bool entraEnLaMitad(struct nodoMemoria* partActual, msgMemoriaBroker* mensajeNuevo);
bool esParticionMinima(struct nodoMemoria* particion);

bool estaParticionado(struct nodoMemoria* partActual);
bool estaOcupado(struct nodoMemoria* partActual);
bool ambosHijosOcupados(struct nodoMemoria* padre);

void enviarMensajesPreviosEnMemoria(uint32_t* socket, uint32_t cola);

void crearDumpDeCache();
void asignarPuntero(uint32_t offset, void* stream, uint32_t sizeStream);

bool yaEstaEnMemoria(paquete* paq);
bool yaSeGuardoEnMemoria(mensajeCatch* msgCatch, mensajeGet* msgGet);
bool existeMensajeEnMemoriaBuddy(mensajeGet* msgGet, mensajeCatch*  msgCatch);
bool compararCatch(mensajeCatch*  elemLista, mensajeCatch*  msgCatch);
bool compararGet(mensajeGet* elemLista, mensajeGet* msgGet);

void elegirVictimaDeReemplazoYeliminarBD();
struct nodoMemoria* buscarVictimaPor(bool(*condition)(struct nodoMemoria*,struct nodoMemoria*));
void modificarNodoAlibre(struct nodoMemoria* victima);
struct tm tiempoCarga(struct nodoMemoria* nodo);
void removerDeListaOcupados(struct nodoMemoria* nodo);
bool tiempoDeCargaMenor(struct nodoMemoria* nodo, struct nodoMemoria* otroNodo);
bool tiempoDeUsoMenor(struct nodoMemoria* nodo, struct nodoMemoria* otroNodo);
void evaluarConsolidacion(struct nodoMemoria* nodo);

#endif /* MEMORIA_H_ */
