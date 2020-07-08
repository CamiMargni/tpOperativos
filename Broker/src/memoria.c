/*
 * memoria.c
 *
 *  Created on: 29 may. 2020
 *      Author: utnso
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "broker.h"
#include "memoria.h"

void registrarMensajeEnMemoria(uint32_t idMensaje, paquete* paq, algoritmoMem metodo){
	log_info(brokerLogger2, "Numero 2");
	if(paq->tipoMensaje == CATCH_POKEMON || paq->tipoMensaje == GET_POKEMON){
		if(yaEstaEnMemoria(paq))
			return;
	}
	log_info(brokerLogger2, "Numero 2.1");
	msgMemoriaBroker* msgNuevo = malloc(sizeof(msgMemoriaBroker));
	msgNuevo->cola          = paq->tipoMensaje;
	msgNuevo->idMensaje     = idMensaje;
	msgNuevo->subsACK       = inicializarListaMutex();
	msgNuevo->subsYaEnviado = inicializarListaMutex();
	msgNuevo->sizeStream 	= paq->sizeStream;
	msgNuevo->stream 		= paq->stream;

	uint32_t sizePrevio;

	log_info(brokerLogger2, "Comienzo a registrar el mensaje en memoria.");
	switch(metodo){
	case PARTICIONES_DINAMICAS:
		pthread_mutex_lock(mutexMemoria);
		registrarEnMemoriaPARTICIONES(msgNuevo);
		pthread_mutex_unlock(mutexMemoria);
		break;
	case BUDDY_SYSTEM:
		pthread_mutex_lock(mutexMemoria);
		log_info(brokerLogger2, "Selecciono administración de memoria con buddy system.");
		log_info(brokerLogger2, "Numero 3");
		sizePrevio = sizeListaMutex(nodosOcupados);
		registrarEnMemoriaBUDDYSYSTEM(msgNuevo, nodoRaizMemoria);
		while (sizeListaMutex(nodosOcupados) == sizePrevio){
			elegirVictimaDeReemplazoYeliminarBD();
			sizePrevio = sizeListaMutex(nodosOcupados);
			registrarEnMemoriaBUDDYSYSTEM(msgNuevo, nodoRaizMemoria);
		}
		pthread_mutex_unlock(mutexMemoria);
		break;
	default:
		printf("error al guardar mensaje");
		free(msgNuevo);
		return;
	}
	log_info(brokerLogger2, "Registre el mensaje en memoria.");
	free(msgNuevo);
}

void guardarEnListaMemoria(uint32_t idMensaje, uint32_t socket, ListasMemoria lista){
	log_info(brokerLogger2, "Numero 4");
	msgMemoriaBroker* mensaje = buscarMensajeEnMemoria(idMensaje);

	if(mensaje == NULL){
		printf("No se encontró el mensaje en memoria, ERROR");
		return;
	}

	//log_info(loggerBroker,"Se recibió ACK de cola: %s, suscriptor: %d, id de mensaje: %d -.-",nombreDeCola(mensaje->cola),socket,mensaje->idMensaje);
	//log_info(brokerLogger2,"Se recibió ACK de cola: %s, suscriptor: %d, id de mensaje: %d -.-",nombreDeCola(mensaje->cola),socket,mensaje->idMensaje);
	//log_info(loggerBroker, armarStringACK(mensaje->cola, mensaje->idMensaje, socket));
	//log_info(brokerLogger2, armarStringACK(mensaje->cola, mensaje->idMensaje, socket));

	if( lista == CONFIRMADO){
		if(estaEnLista(socket,lista, mensaje ))
		{	printf("Ya está en la lista");
			return;}
		pthread_mutex_lock(mutexMemoria);
		addListaMutex(mensaje->subsYaEnviado, (void*) socket);
		pthread_mutex_unlock(mutexMemoria);
	}else if(lista == SUBSYAENVIADOS){
		if(estaEnLista(socket,lista, mensaje ))
		{
			printf("Ya está en la lista");
			return;}
		pthread_mutex_lock(mutexMemoria);
		addListaMutex(mensaje->subsACK, (void*) socket);
		pthread_mutex_unlock(mutexMemoria);
	}
}

msgMemoriaBroker* buscarMensajeEnMemoria(uint32_t idMensajeBuscado){

	/*if(algoritmoMemoria == BUDDY_SYSTEM){*/
		return buscarMensajeEnMemoriaBuddy(idMensajeBuscado);
		/*}else{
		return buscarMensajeEnMemoriaParticiones(idMensajeBuscado);
	}*/

}

bool estaEnLista(uint32_t socket, ListasMemoria lista, msgMemoriaBroker* mensaje){
	log_info(brokerLogger2, "Numero 5");
	listaMutex* list = inicializarListaMutex();
	if(lista == CONFIRMADO){
		list =  mensaje->subsACK;
	}else{
		list =  mensaje->subsYaEnviado;
	}

	for(uint32_t i=0; i< sizeListaMutex(list); i++ ){
		if(socket == (uint32_t)getListaMutex(list, i)){
			free(list);
			return true;
		}
	}

	free(list);
    return false;
}


void asignarPuntero(uint32_t offset, void* stream, uint32_t sizeStream){
	log_info(brokerLogger2,"Se almacena mensaje en posición: %p (de memoria) -.-", memoria - memoria+ offset);
	log_info(loggerBroker, "Se almacena mensaje en memoria en posición: %p -.-", memoria - memoria + offset);
	memcpy(memoria + offset, stream, sizeStream);
}

void crearDumpDeCache(){
	log_info(loggerBroker,"Se solicitó dump de cache.");
	log_info(brokerLogger2,"Se solicitó dump de cache.");

	//archivoMutex* archivo = iniciarArchivoMutex();
	iniciarArchivoMutex();
	if(algoritmoMemoria == BUDDY_SYSTEM){
		recorrerArbolYgrabarArchivo();
	}else{
		registrarParticionesLibresYocupadas();
	}
}

bool yaEstaEnMemoria(paquete* paq){
	log_info(brokerLogger2, "Numero 2.2");
	switch(paq->tipoMensaje){
	case CATCH_POKEMON:
		log_info(brokerLogger2, "Numero CATCH");
		return yaSeGuardoEnMemoria(deserializarCatch(paq->stream), NULL);
	break;
	case GET_POKEMON:
		return yaSeGuardoEnMemoria(NULL, deserializarGet(paq->stream));
	break;
	default:
		printf("ERROR");
		return false;
	}

}

bool yaSeGuardoEnMemoria(mensajeCatch* msgCatch, mensajeGet* msgGet){
	particionOcupada* partOcupada;
	if(algoritmoMemoria == BUDDY_SYSTEM){
		if(msgCatch != NULL){
			return existeMensajeEnMemoriaBuddy(NULL, msgCatch);
		}else{
			return existeMensajeEnMemoriaBuddy(msgGet,NULL);
		}
	}else{
		for(int i=0; i<sizeListaMutex(memoriaPARTICIONES); i++){
		  partOcupada = getListaMutex(memoriaPARTICIONES, i);
		  if(partOcupada->mensaje->cola == CATCH_POKEMON && msgCatch != NULL){
			  if(compararCatch(deserializarCatch(partOcupada->mensaje->stream), msgCatch))
				  return true;
		  }else if(partOcupada->mensaje->cola == GET_POKEMON && msgGet != NULL){
			  if( compararGet(deserializarGet(partOcupada->mensaje->stream), msgGet))
				  return true;
		  }
		}
		return false;
	  }
}

bool compararCatch(mensajeCatch*  elemLista, mensajeCatch*  msgCatch){
	log_info(brokerLogger2, "Numero 2.2.3");
	if(strcmp(elemLista->pokemon, msgCatch->pokemon) == 0){
		if(elemLista->posX == msgCatch->posX && elemLista->posY == msgCatch->posY)
			log_info(brokerLogger2, "Numero 2.2.4");
			return true;
	}
	log_info(brokerLogger2, "Numero 2.2.5");
		return false;
}

bool compararGet(mensajeGet* elemLista, mensajeGet* msgGet){
	if(strcmp(elemLista->pokemon, msgGet->pokemon) == 0){
		return true;}
	return false;
}
