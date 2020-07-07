/*
 * archivoHeader.c
 *
 *  Created on: 26 jun. 2020
 *      Author: utnso
 */

#include "gamecard.h"

bool estaAbierto(archivoHeader* metadata){
	if(metadata->tipo==ARCHIVO){
			t_config* config=config_create(metadata->pathArchivo);
			char* valorOpen=config_get_string_value(config,"OPEN");
			config_destroy(config);
			if(strcmp(valorOpen, "Y")==0){
				return true;
			}


		}
	return false;
}

void cerrarArchivo(archivoHeader* metadata, FILE* archivo){
	if(metadata->tipo==ARCHIVO){
		fclose(archivo);
		t_config* config=config_create(metadata->pathArchivo);
		config_set_value(config,"OPEN","N");
		config_save(config);
		config_destroy(config);
		metadata->estaAbierto=false;
	}
}

FILE* abrirArchivo(archivoHeader* metadata){
	if(metadata->tipo==ARCHIVO){
		metadata->estaAbierto=true;
		t_config* config=config_create(metadata->pathArchivo);
		config_set_value(config,"OPEN","Y");
		config_save(config);
		config_destroy(config);
		return fopen(metadata->pathArchivo,"r+");

	}
	return NULL;
}

void modificarSize(archivoHeader* metadata,uint32_t size){
	if(metadata->tipo==ARCHIVO){
		t_config* config=config_create(metadata->pathArchivo);
		metadata->tamanioArchivo=size;
		config_set_value(config,"SIZE",string_itoa(size));
		config_save(config);
		config_destroy(config);
	}
}

void agregarBloque(archivoHeader* metadata, blockHeader* bloque){
	if(metadata->tipo==ARCHIVO){
		t_config* config=config_create(metadata->pathArchivo);
		list_add(metadata->bloquesUsados,(void*) bloque);
		char* listaB=obtenerStringListaBloques(metadata);
		config_set_value(config,"BLOCKS",listaB);
		config_save(config);
		config_destroy(config);
		free(listaB);
	}

}



char* obtenerStringListaBloques(archivoHeader* metadata){
	t_list* listaBloques=metadata->bloquesUsados;
	char* buffer=malloc(500);
	for(uint32_t i=0; i<list_size(listaBloques);i++){
		uint32_t idBloqueActual=((blockHeader*) list_get(listaBloques,i))->id;

		if(i==0){
			sprintf(buffer, "[%i]", idBloqueActual);
		}else{
			buffer=agregarIdBloque(buffer,idBloqueActual);

		}


	}
	return buffer;
}

char* agregarIdBloque(char* string, uint32_t id){
	uint32_t pos = strlen(string) - 1;
	char* buffer=malloc(500);
	uint32_t i=0;
	while(1){
		if(i!=pos){
			buffer[i]=string[i];
		}else{
			sprintf((buffer+i), ",%d]", id);
			free(string);
			return buffer;
		}
		i++;
	}
}

void removerBloque(archivoHeader* metadata, uint32_t idBloque){
	t_config* config=config_create(metadata->pathArchivo);
	int32_t pos= obtenerPosicionBloqueEnLista(metadata->bloquesUsados, idBloque);
	if(pos!=-1){
		list_remove(metadata->bloquesUsados,pos);
	}

	char* listaB=obtenerStringListaBloques(metadata);
	config_set_value(config,"BLOCKS",listaB);
	config_save(config);
	config_destroy(config);
	free(listaB);
}

int32_t obtenerPosicionBloqueEnLista(t_list* listaBloques , uint32_t idBloque){
	for(uint32_t i=0; i<list_size(listaBloques);i++){
		blockHeader* bloqueActual= (blockHeader*) list_get(listaBloques, i);
		if(bloqueActual->id==idBloque){
			return i;
		}
	}
	return -1;
}

void disminuirCapacidad(blockHeader* bloque, int32_t bytes){
	bloque->bytesLeft-=bytes;
}

bool tieneCapacidad(blockHeader* bloque, int32_t capacidad){
	return bloque->bytesLeft>=capacidad;
}

char* obtenerStringArchivo(char* pokemon){
	char* buffer=string_new();
	archivoHeader* headerPoke= buscarArchivoHeaderPokemon(pokemon);
	for(uint32_t i=0; i<list_size(headerPoke->bloquesUsados);i++){
		blockHeader* actual= list_get(headerPoke->bloquesUsados,i);
		char* bufferBloque=leerBloque(actual);
		string_append(&buffer, bufferBloque);
		free(bufferBloque);
	}

	return buffer;
}

char* pathBloque(uint32_t idBloque){
	return string_from_format("%s/Blocks/%d.bin", puntoMontaje, idBloque);
}
char* leerBloque(blockHeader* bloque){
	char* path=pathBloque(bloque->id);
	FILE* archivoBloque= fopen(path,"r");


	fseek(archivoBloque, 0, SEEK_END);
	uint32_t pos= ftell(archivoBloque);
	fseek(archivoBloque, 0, SEEK_SET);
	char* buffer= malloc(pos+1);
	fread(buffer, pos+1, 1, archivoBloque);
	if(string_contains(buffer, "\n")){
		log_info(gamecardLogger2, "Encontre el  barra n.");
	}
	fclose(archivoBloque);
	return buffer;

}

uint32_t posBloque(blockHeader* bloque){
	char* path=pathBloque(bloque->id);
	FILE* archivoBloque= fopen(path,"r");
	fseek(archivoBloque, 0, SEEK_END);
	uint32_t pos= ftell(archivoBloque);
	fclose(archivoBloque);
	return pos;
}

t_list* obtenerListaPosicionesString(char* posiciones){
	t_list* lista=list_create();
	char* buffer=string_new();
	for(uint32_t i=0;i<(strlen(posiciones)+1);i++){
		char caracterActual= posiciones[i];
		if(caracterActual=='\n'){
			list_add(lista, (void*) buffer);
			buffer=string_new();
		}else{
			char* cadena=malloc(1);
			sprintf(cadena, "%c", caracterActual);
			string_append(&buffer, cadena);
			free(cadena);
		}
	}
	return lista;
}

posicionCantidad* obtenerPosicionCantidadDeString(char* stringPos){
	char* bufferPosX=string_new();
	char* bufferPosY=string_new();
	char* bufferCantidad=string_new();
	bool posX=true;
	bool posY=false;
	bool cantidad=false;

		for(uint32_t i=0;i<(strlen(stringPos));i++){
			char caracterActual= stringPos[i];
			if(posX){
				if(caracterActual=='-'){
					posX=false;
					posY=true;
				}else{
					char* cadena=malloc(1);
					sprintf(cadena, "%c", caracterActual);
					string_append(&bufferPosX, cadena);
					free(cadena);
				}
			}else if(posY){
				if(caracterActual=='='){
					posY=false;
					cantidad=true;
				}else{
					char* cadena=malloc(1);
					sprintf(cadena, "%c", caracterActual);
					string_append(&bufferPosY, cadena);
					free(cadena);
				}
			}else if(cantidad){
				char* cadena=malloc(1);
				sprintf(cadena, "%c", caracterActual);
				string_append(&bufferCantidad, cadena);
				free(cadena);
			}

		}

	posicionCantidad* pos=malloc(sizeof(posicionCantidad));
	(pos->posicion).x=atoi(bufferPosX);
	(pos->posicion).y=atoi(bufferPosY);
	pos->cantidad=atoi(bufferCantidad);

	return pos;

}

t_list * obtenerListaPosicionCantidad(t_list* listaString){
	t_list* lista= list_create();
	for(uint32_t i=0; i<list_size(listaString);i++){
		char* stringActual= (char*) list_get(listaString,i);
		posicionCantidad* posActual=obtenerPosicionCantidadDeString(stringActual);
		list_add(lista, (void*) posActual);
	}
	list_destroy_and_destroy_elements(listaString, free);
	return lista;
}

t_list* obtenerListaPosicionCantidadDeString(char* string){
	return obtenerListaPosicionCantidad(obtenerListaPosicionesString(string));
}
