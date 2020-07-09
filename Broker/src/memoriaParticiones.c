
#include "broker.h"
#include "memoria.h"

bool menorAMayorSegunTiempoCarga (void* part1, void* part2){
	particion* parti1 = (particion*) part1;
	particion* parti2 = (particion*) part2;
	return (parti1->tiempoDeCargaPart.tm_hour < parti2->tiempoDeCargaPart.tm_hour || (parti1->tiempoDeCargaPart.tm_hour == parti2->tiempoDeCargaPart.tm_hour && parti1->tiempoDeCargaPart.tm_min < parti2->tiempoDeCargaPart.tm_min));
}

bool menorAMayorSegunLru (void* part1, void* part2){
	particion* parti1 = (particion*) part1;
	particion* parti2 = (particion*) part2;
	return (parti1->lru.tm_hour < parti2->lru.tm_hour || (parti1->lru.tm_hour==parti2->lru.tm_hour && parti1->lru.tm_min<parti2->lru.tm_min));
}

void eliminarParticion (particion* part){
	particion* partiNueva = malloc(sizeof(particionLibre));
	partiNueva->offset = part->offset;
	partiNueva->sizeParticion = part->mensaje->sizeStream;
	addListaMutex(particionesLibres, (void*)partiNueva);
	removeAndDestroyElementListaMutex(particionesOcupadas, 0, destroyParticionOcupada);
}

void elegirParticionVictimaYEliminarla(){
	particion* particionVictima;
	switch(algoritmoReemplazo){
	case FIFO:
		list_sort_Mutex(particionesOcupadas, menorAMayorSegunTiempoCarga);
		 particionVictima = (particion*)getListaMutex(particionesOcupadas, 0);
		break;
	case LRU:
		list_sort_Mutex(particionesLibres, menorAMayorSegunLru);
		particionVictima = (particion*)getListaMutex(particionesOcupadas, 0);
		break;
	}
	eliminarParticion(particionVictima);
}

//void sustituirParticion(particionOcupada* particionASustituir, msgMemoriaBroker* mensajeAGuardar){
//	if(particionASustituir->mensaje->sizeStream > mensajeAGuardar->sizeStream){
//		particionLibre* particionLibreNueva = malloc(sizeof(particionLibre));
//		particionLibreNueva->offset = particionASustituir->offset + mensajeAGuardar->sizeStream;
//		particionLibreNueva->sizeParticion = particionASustituir->mensaje->sizeStream - mensajeAGuardar->sizeStream;
//	}
//	particionASustituir->mensaje=mensajeAGuardar;
//	//actualizar LRU y tiempo de carga.
//	memcpy(memoria + particionASustituir->offset, mensajeAGuardar->stream, mensajeAGuardar->sizeStream);
//}

bool sePuedeCompactar(){
	if(frecuenciaCompactacion==-1){
		return (sizeListaMutex(particionesOcupadas)==0);
	}else if(frecuenciaCompactacion>=0){
		return cantidadBusquedasFallidas >= frecuenciaCompactacion;
	}
	return NULL;
}

void registrarEnParticiones(msgMemoriaBroker* mensajeNuevo){
	particion* particionLibre;
	particionLibre = obtenerParticionLibrePARTICIONES(mensajeNuevo->sizeStream);

	if(particionLibre == NULL){
		cantidadBusquedasFallidas++;
		if(sePuedeCompactar()){
			compactar();
			particionLibre = obtenerParticionLibrePARTICIONES(mensajeNuevo->sizeStream);
			if(particionLibre == NULL){
				cantidadBusquedasFallidas++;
				elegirParticionVictimaYEliminarla();
				registrarEnParticiones (mensajeNuevo);
		}else{
			elegirParticionVictimaYEliminarla();
			registrarEnParticiones(mensajeNuevo);
		}
	}
	asignarMensajeAParticion(particionLibre, mensajeNuevo);
//	asignarPuntero(particionLibre->offset, mensajeNuevo->stream, mensajeNuevo->sizeStream);
	}
}

void asignarMensajeAParticion(particion* partiLibre, msgMemoriaBroker* mensaje){
	particion* partiOcupada = malloc(sizeof(particion));
	partiOcupada->offset=partiLibre->offset;
	partiOcupada->mensaje=mensaje;
	//asignar LRU y tiempo de carga
	memcpy(memoria + partiOcupada->offset, mensaje->stream, mensaje->sizeStream);
	mensaje->stream = memoria + partiOcupada->offset;
	addListaMutex(particionesOcupadas, (void*)partiOcupada);

	if(partiLibre->sizeParticion > mensaje->sizeStream){
		partiLibre->offset += mensaje->sizeStream;
		partiLibre->sizeParticion-= mensaje->sizeStream;
	}else{ //osea que el tamaño es igual
		destroyParticionLibre (partiLibre);
	}
}

particionLibre* obtenerParticionLibrePARTICIONES(uint32_t tamStream){
	auxTamanioStreamGlobal = tamStream;

	if(sizeListaMutex(particionesLibres) <= 0)
		return NULL;

	if (algoritmoParticionLibre == FIRST_FIT){
		list_sort_Mutex(particionesLibres, menorAmayorSegunOffset);
		return list_remove_by_condition_Mutex(particionesLibres, esSuficientementeGrandeParaElMSG );

	}else if(algoritmoParticionLibre == BEST_FIT){
		list_sort_Mutex(particionesLibres, menorAmayorSegunSize);
		return list_remove_by_condition_Mutex(particionesLibres, esSuficientementeGrandeParaElMSG );
	}
	return NULL;
}

void compactar(){
	particion* elemento;
	uint32_t base = 0;
	list_sort_Mutex(particionesOcupadas, menorAmayorSegunOffset);
	for(int i=0; i<sizeListaMutex(particionesOcupadas); i++){
		elemento = getListaMutex(particionesOcupadas, i);
		memcpy(memoria + base, memoria + elemento->offset, elemento->mensaje->sizeStream);
		elemento->offset = base;
		base  += elemento->mensaje->sizeStream;
	}
	generarParticionLibre(base);
	cantidadBusquedasFallidas = 0;
}

void generarParticionLibre(uint32_t base){
	particion* nuevaParticion = malloc(sizeof(particion));
	nuevaParticion->offset = base;
	nuevaParticion->sizeParticion = tamMemoria - base;

	for(int j=0; j<sizeListaMutex(particionesLibres); j++){
		removeAndDestroyElementListaMutex(particionesLibres,j,destroyParticionLibre); //o free normal
	}

	addListaMutex(particionesLibres,(void*) nuevaParticion);
}

void destroyParticionLibre(void* parti){
	particion* part = (particion*)parti;
	free(part);
}

void destroyParticionOcupada (void* part){
	particion* parti = (particion*) part;
	free(parti->mensaje); //ver si destruir las listas o no
	free(parti);
}

bool menorAmayorSegunOffset(void* primero, void* segundo){
	return ((particion*)primero)->offset < ((particion*)segundo)->offset;
}

bool menorAmayorSegunSize(void* primero, void* segundo){
	return ((particion*)primero)->sizeParticion < ((particion*)segundo)->sizeParticion;
}

bool esSuficientementeGrandeParaElMSG(void* elemento){
	particion* partLibre = (particion*)elemento;
	return partLibre->sizeParticion >= auxTamanioStreamGlobal;
}

t_list* buscarMensajesDeColaEnParticiones (uint32_t cola){
	particion* particion1;
	t_list* msjsDeCola = list_create();
	for(int i = 0; i<sizeListaMutex(particionesOcupadas); i++){
		particion1 = (particion*) getListaMutex(particionesOcupadas, i);
		if(particion1->mensaje->cola==cola){
			list_add(msjsDeCola, (void*)particion1->mensaje);
		}
	}
	return msjsDeCola;
}

void enviarMsjsASuscriptorNuevoParticiones (uint32_t cola, uint32_t* socket){
	t_list* listMsjs = buscarMensajesDeColaEnParticiones(cola);
	msgMemoriaBroker* msg;
	paquete* paqueteASerializar;
	void* paqueteSerializado;
	for(int i = 0; i<list_size(listMsjs); i++){
		msg = (msgMemoriaBroker*) list_get(listMsjs, i);
		paqueteASerializar = llenarPaquete(msg->modulo, msg->cola, msg->sizeStream, msg->stream);
		paqueteSerializado = serializarPaquete(paqueteASerializar);
		send(*socket, paqueteSerializado, sizePaquete(paqueteASerializar),0);
		addListaMutex(msg->subsYaEnviado, (void*)socket);
	}
	list_destroy(listMsjs);
}

msgMemoriaBroker*  buscarMensajeEnMemoriaParticiones(uint32_t idMensajeBuscado){
	particion* particion1;
	for(int i = 0; i<sizeListaMutex(particionesOcupadas); i++){
		particion1 = (particion*) getListaMutex (particionesOcupadas, i);
		if(particion1->mensaje->idMensaje == idMensajeBuscado){
			return particion1->mensaje;
		}
	}
	return NULL;
}

