#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>

#define MAX_FILE_NAME 256
#define MAX_FILE_CONTENT 1024       //Ogni file può essere composto massimo da 4 blocchi 
#define BLOCK_SIZE 256              
#define MAX_BLOCKS_NUM 256
#define MAX_BLOCKS_PER_NODE 251
#define MAX_INODES 256
#define MAX_DIR_ENTRIES 256
#define MAX_FILE_SIZE 4096

#define SEEK_FREESPACE_TABLE_SET 256


typedef uint8_t inode_num_t;
typedef uint8_t block_num_t;
typedef uint16_t file_name_lenght_t;

/*

Blocco di 256 byte in cui i primi 4 byte rappresentano i permessi ed il tipo del file, 
i seguenti 251 byte rappresentano gli indici dei blocchi all'interno dei quali si trovano i dati
del file rappresentato dall'inode.

*/
typedef struct inode{

    mode_t mode;
    size_t size;
    block_num_t index_vector[MAX_BLOCKS_PER_NODE];

}inode_t;

/*
Rappresentazione del contenuto di una directory, ogni file contenuto in una directory 
è rappresentato da una dir_entry. 
*/
typedef struct dir_entry{
    
    block_num_t inode_index;
    file_name_lenght_t name_lenght;

    char name[MAX_FILE_NAME + 1];

}dir_entry_t;



/*
Rappresentazione in memoria di un file.
*/
typedef struct file{

    char name[MAX_FILE_NAME];
    char content[MAX_FILE_CONTENT];

    inode_num_t inode_num;

    size_t size;
    mode_t mode;
    dir_entry_t entries[MAX_DIR_ENTRIES];

}file_t;

typedef struct filesystem{

    FILE* file;
    uint8_t* free_space_table;
    block_num_t* inode_table;
    file_t* open_file;

}filesystem_t;




inode_num_t inode_from_path(const char* path,filesystem_t* fs);
uint32_t block_free_space_left(block_num_t block_num,filesystem_t* fs);
void move_to_block(block_num_t block_num,off_t offset ,filesystem_t* fs);
block_num_t assign_block_to_inode(inode_num_t inode,filesystem_t* fs);
void sync_fs(filesystem_t* fs);
int8_t new_file_to_dir(file_t file,char* path , filesystem_t* fs);

/*
    Carica un file system da un file
*/
FILE* load_fs(char* path){
    FILE* fs = fopen(path,"rb+");
    if(fs == NULL)
        return NULL;
    return fs;
}


void format_fs(FILE* fs){
    
    uint8_t zeroes[BLOCK_SIZE*MAX_BLOCKS_NUM];
    memset(zeroes,0,BLOCK_SIZE*MAX_BLOCKS_NUM);
    fseek(fs,0,0);
    fwrite(zeroes,1,BLOCK_SIZE*MAX_BLOCKS_NUM,fs);
}

/* Gestione tabella degli inode

Nel primo blocco del dispositivo di memorizzazione (un file) è presente una tabella degli inode che 
indicizzata per numero di inode associa all'inode il blocco in cui questo è contentuto.

*/
block_num_t* init_inode_table(){

    block_num_t* new_inode_table =  calloc(MAX_INODES , sizeof(uint8_t));
    
    if(new_inode_table == NULL)
        return NULL;
    
    return new_inode_table; 

}
/*
    Salva sul file che rappresenta il dispositivo di memorizzazione del file system lo stato attuale 
    della tabella degli inode
*/
void sync_inode_table(filesystem_t* fs){
    
    fseek(fs->file,0,SEEK_SET);
    fwrite(fs->inode_table,sizeof(block_num_t),MAX_INODES,fs->file);
    fflush(fs->file);

}

/*
    Legge da un file che rappresenta in dispositivo di memorizzazione del file system lo stato attuale della 
    tabella degli inode
*/
void read_inode_table(filesystem_t* fs){
    
    fseek(fs->file,0,SEEK_SET);
    fread(fs->inode_table,sizeof(block_num_t),MAX_INODES,fs->file);

}
/*                             
    Scorre la tabella degli inode fino a trovare un inode libero,
    ritorna il numero di inode libero, 0 altrimenti.
    (0 non potrà mai essere libero in quanto è assegnato alla directory root)
*/
inode_num_t get_free_inode_number(filesystem_t* fs){

    int i = 0;
    while(i < MAX_INODES && fs->inode_table[i] != 0)
        i++;
    
    if(fs->inode_table[i] == 0)
        return i;
    else 
        return 0;

}

/*
    Dato un numero di inode ne legge dal file il contenuto, ritorna una rappresentazione 
    dell'inode letto come variabile di tipo inode_t
*/
inode_t read_inode(uint8_t inode_num, filesystem_t* fs){

    inode_t inode = {0};
    block_num_t block = fs->inode_table[inode_num];
   

    inode.size = 0;

    move_to_block(block,0,fs);
    
    
    fread(&(inode.mode),sizeof(mode_t),1,fs->file);
    
    fread(&(inode.size),sizeof(size_t),1,fs->file);
    
    fread(&(inode.index_vector),sizeof(block_num_t),251,fs->file);
    
    return inode;
}





/* Gestione dello spazio libero
    Nel secondo blocco del dipositivo di memorizzazione è memorizzato un vettore 
    che indicizzato per numero di blocco indica se lo stesso è occupato o meno
*/
uint8_t* init_freespace_table(){

    uint8_t* new_freespace_table =  calloc(MAX_BLOCKS_NUM , sizeof(uint8_t));
    new_freespace_table[0] = 1; //Il blocco 0 contiene la tabella degli inode
    new_freespace_table[1] = 1; //Il blocco 1 contiene la tabella dello spazio libero 

    if(new_freespace_table == NULL)
        return NULL;
    
    return new_freespace_table; 

}

/*
    Salva lo stato attuale del vettore dello spazio libero all'interno del file usato come dispositivo di memorizzazione
*/
void sync_freespace_table(filesystem_t* fs){

    fseek(fs->file,SEEK_FREESPACE_TABLE_SET,0);
    fwrite(fs->free_space_table,sizeof(uint8_t),MAX_BLOCKS_NUM,fs->file);
    fflush(fs->file);

}


/*
    Legge da un file usato come dipositivo di memorizzazione lo stato del vettore dello spazio libero
*/
void read_freespace_table(uint8_t* freespace_table, FILE* fs){
    
    fseek(fs,SEEK_FREESPACE_TABLE_SET,0);
    fread(freespace_table,sizeof(uint8_t),MAX_BLOCKS_NUM,fs);

}

/*
    Scorre la tabella dello spazio libero finchè non trova un blocco libero
*/
uint8_t get_free_block(uint8_t* freespace_table){
    
    int i = 0;
    
    while(i < MAX_BLOCKS_NUM && freespace_table[i] != 0)
        i++;
    
    if(freespace_table[i] == 0)
        return i;
    
    else 
        return 0;
    

}

/*
    Scorre la tabella dello spazio libero finchè non trova un blocco libero 
    e imposta come occupato il blocco trovato libero.
    ritorna il numero del blocco libero trovato.
*/
uint8_t get_and_set_free_block(filesystem_t* fs){
    
    uint8_t i = 0;
    
    while(i < MAX_BLOCKS_NUM && fs->free_space_table[i] != 0)
        i++;
    
    if(fs->free_space_table[i] == 0){
        fs->free_space_table[i] = 1;
        sync_freespace_table(fs);
        return i;
    }
    
    else 
        return 0;
    

}


/*
    Utils
*/


/*
    Se il blocco in cui stanno venendo scritti dei dati risulta pieno, assegna un nuovo blocco
    all'inode che rappresenta il file sul quale si sta scrivendo e sposta la posizione all'interno
    del file che rappresenta il file system a quella di questo nuovo blocco.
*/
block_num_t reach_new_block_if_full(inode_num_t inode_num,block_num_t starting_block,filesystem_t* fs){

    block_num_t new_block;

    if(block_free_space_left(starting_block,fs) == 0){

            new_block = assign_block_to_inode(inode_num,fs);
            move_to_block(new_block,0,fs);

            return new_block;
        }

    else

        return starting_block;

}

/*
    Scrive sul file che rappresenta il file system le informazioni necessarie
    ad indicare che un file si trova all'interno della directory: Numero di inode,
    lunghezza del nome e nome del file. Queste informazioni vanno scritte all'interno di un blocco dati
    facente parte di una directory, ogni campo viene scritto byte per byte.
*/
void write_file_info(file_t file,inode_num_t dir_inode_num ,block_num_t starting_block,filesystem_t* fs){

    file_name_lenght_t file_name_lenght = strlen(file.name);
    block_num_t block = starting_block;
    uint8_t inode_num_bytes[sizeof(inode_num_t)];
    uint8_t file_name_lenght_bytes[sizeof(file_name_lenght_t)];

    for(uint8_t j = 0; j < sizeof(inode_num_t); j++){
        inode_num_bytes[j] = file.inode_num & (0xff >> j * 8); 
    }

    for(uint8_t j = 0; j < sizeof(file_name_lenght); j++){
        file_name_lenght_bytes[j] = file_name_lenght & (0xff >> j * 8); 
    }

    for(uint8_t j = 0; j < sizeof(inode_num_t);j++){
        block = reach_new_block_if_full(dir_inode_num,block,fs);
        fwrite(inode_num_bytes + j,1,1,fs->file);
    }
    
    for(uint8_t j = 0; j < sizeof(file_name_lenght);j++){
        block = reach_new_block_if_full(dir_inode_num,block,fs);
        fwrite(file_name_lenght_bytes + j,1,1,fs->file);
    }

    for(file_name_lenght_t j = 0; j < file_name_lenght; j++){

        block = reach_new_block_if_full(dir_inode_num,block,fs);
        fwrite(file.name + j,1,1,fs->file);
    }

    fflush(fs->file);
}




/*
    Sposta la posizione all'interno del file che rappresenta il file system ad un blocco dato.
*/
void move_to_block(block_num_t block_num,off_t offset ,filesystem_t* fs){
    fseek(fs->file,block_num*BLOCK_SIZE + offset,SEEK_SET);
}


/*
    
    Dato un blocco sposta la posizione all'interno del file che rappresenta il file system, si sposta 
    al primo byte libero all'interno del blocco.
    ritorna -1 se il blocco è pieno, altrimenti lo scostamento all'interno del blocco.
    Se il blocco in esame è un inode questo va segnalato tramite il parametro is_inode.

*/
int16_t move_to_empty_space_in_block(block_num_t block_num,uint8_t is_inode,filesystem_t* fs){

    uint8_t block[BLOCK_SIZE];
    uint8_t offset;

    move_to_block(block_num,0,fs);
    fread(block,1,BLOCK_SIZE,fs->file);
    
    if(is_inode == 1)
         offset = sizeof(mode_t) + sizeof(size_t);
    else
        offset = 0;

    int i = 0 + offset;
    int free_space_candidate = -1;
    
    while(i < BLOCK_SIZE){

        if(block[i] == 0 && free_space_candidate == -1)
            free_space_candidate = i;

        if(block[i] != 0 && free_space_candidate != -1)
            free_space_candidate = -1;

        i++;
    }

    if(free_space_candidate == -1)
        return -1;

    else
        move_to_block(block_num,free_space_candidate,fs);

    return 0;

}

/*
Sposta la posizione all'interno del file all'ultimo blocco dati di un file a partire dal numero di inode, non dovesse essere presente un blocco dati libero,
ne assegna uno. TODO  QUESTA VA USATA SOLO SE NON SI TROVA NESSUNO SPAZIO LIBERO NEI BLOCCHI GIA' OCCUPATI.
IN TAL CASO SI VA ALLA FINE DELLO SPAZIO DISPONIBILE PER IL FILE CON QEUSTA FUNZIONE, AGGIUNGERE UNA FUNZIONI CHE TROVI SPAZIO
*/

block_num_t reach_data_end(inode_num_t inode_num, filesystem_t* fs){

    uint8_t i = 0;
    inode_t inode = read_inode(inode_num,fs); 

    while(inode.index_vector[i] != 0){

        if(move_to_empty_space_in_block(inode.index_vector[i],0,fs) != -1)
            break;
        else
            i++;
    }

    if(inode.index_vector[i] == 0){
        assign_block_to_inode(inode_num,fs);
        inode= read_inode(inode_num,fs);
        move_to_block(inode.index_vector[i],0,fs);
    }

    return inode.index_vector[i];

}

/*
Assegna un inode ad un blocco, questo blocco conterrà gli indici di tutti i blocchi facenti parti del file
rappresentato dall'inode
*/
void assign_inode_to_block(inode_num_t inode, block_num_t block ,filesystem_t* fs){
    
    fs->inode_table[inode] = block;
    fs->free_space_table[block] = 1;
    sync_fs(fs);

}

/*
Assegna un blocco libero ad un inode, imposta il blocco assegnato come occupato.
*/
block_num_t assign_block_to_inode(inode_num_t inode,filesystem_t* fs){
    
    uint8_t ret;
    block_num_t block_num = get_and_set_free_block(fs);
    inode_num_t inode_block_num = fs->inode_table[inode];

    ret = move_to_empty_space_in_block(inode_block_num,1,fs);
    
    if(ret == -1) //Il blocco è pieno 
        return 0;

    fwrite(&block_num,sizeof(block_num_t),1,fs->file);
    fflush(fs->file);
    return block_num;
}


uint32_t block_free_space_left(block_num_t block_num,filesystem_t* fs){
    
    uint32_t pos = ftell(fs->file);
    uint32_t space_left =  BLOCK_SIZE*(block_num + 1) - pos;

    return space_left;

}


/*----------------------------------------*/

/*Gestione Filesystem*/

void sync_fs(filesystem_t* fs){
    
    sync_inode_table(fs);
    sync_freespace_table(fs);
    fflush(fs->file);
}


filesystem_t* init_fs(filesystem_t** fs){
    
    filesystem_t* new_fs = malloc(sizeof(filesystem_t));
    new_fs->free_space_table = init_freespace_table();
    new_fs->inode_table = init_inode_table();
    new_fs->file = load_fs("FS");
    new_fs->open_file = NULL;
    format_fs(new_fs->file);

    if(new_fs->inode_table == NULL || new_fs->free_space_table == NULL)
        return NULL;

    sync_fs(new_fs);
    *fs = new_fs;

    return new_fs;

}


/*---------------------------*/


/*Manipolazione dei file*/

int8_t sync_new_file(file_t* file, filesystem_t* fs){
    
    inode_num_t inode_num = get_free_inode_number(fs);
    block_num_t block_num = get_and_set_free_block(fs);

    if(block_num == 0)
        return -1;

    file->inode_num = inode_num;

    assign_inode_to_block(inode_num, block_num, fs);
    move_to_block(block_num,0,fs);
    fwrite(&(file->mode) ,sizeof(mode_t),1,fs->file); //salva sul dispositivo di memorizzazione i metadati del file
    fwrite(&(file->size) ,sizeof(size_t),1,fs->file);
    fflush(fs->file); 
    sync_fs(fs);
    
    return block_num;
}


void update_file_size(inode_num_t file_inode ,size_t new_size,filesystem_t* fs){
    
    inode_num_t inode_block = fs->inode_table[file_inode];
    move_to_block(inode_block,4,fs);
    fwrite(&new_size,sizeof(size_t),1,fs->file);

}


void init_root_dir(filesystem_t* fs){

    file_t new_file;

    new_file.mode = S_IFDIR | 0644;
    new_file.size = 0;
    sync_new_file(&new_file,fs);

}

void sync_test_files(filesystem_t* fs,uint8_t num){

    file_t new_file;

    new_file.mode = S_IFREG | 0644;
    new_file.size = 0;

    for(int i = 0; i < num ; i++){

    snprintf(new_file.name,256,"Test_%d",i);
    new_file_to_dir(new_file,"/",fs);

    }

}

void sync_test_dir(filesystem_t* fs,uint8_t num){

    file_t new_file;

    new_file.mode = S_IFDIR | 0755;
    new_file.size = 0;

    for(int i = 0; i < num ; i++){

    snprintf(new_file.name,256,"DirTest_%d",i);
    new_file_to_dir(new_file,"/",fs);

    }

}



uint8_t is_inode_full(inode_num_t dir_inode_num, filesystem_t* fs){
    
    uint8_t i = 0;
    inode_t inode = read_inode(dir_inode_num,fs); 
    
    while(inode.index_vector[i] != 0 && move_to_empty_space_in_block(inode.index_vector[i],0,fs) == -1){
        i++;
    }

    if(i == MAX_BLOCKS_PER_NODE && inode.index_vector[i] != 0 && move_to_empty_space_in_block(inode.index_vector[i],0,fs) == -1)
        
        return 1;

    else 

        return 0;
    
}

int8_t new_file_to_dir(file_t file,char* path , filesystem_t* fs){

    inode_num_t dir_inode_num;
    uint8_t ret;
    block_num_t block;

    if(strcmp(path,"/") == 0)
        dir_inode_num = 0;
    else
        dir_inode_num = inode_from_path(path,fs);
             

    ret = is_inode_full(dir_inode_num,fs);
    
    if(ret == 1)
        return -1;

    sync_new_file(&file,fs);
    block = reach_data_end(dir_inode_num,fs);
    write_file_info(file,dir_inode_num,block,fs);
    fflush(fs->file);

    return 0;
}

uint8_t read_dir_entries(file_t* dir ,inode_t inode , filesystem_t* fs){

    
    block_num_t new_block = inode.index_vector[0];
    uint32_t k = 0;
    uint32_t last_entry_num = 0;

    uint8_t inode_num_bytes[sizeof(inode_num_t)];
    uint8_t file_name_lenght_bytes[sizeof(file_name_lenght_t)];
    uint8_t byte = 0;
    
    if(inode.index_vector[last_entry_num] == 0)
        return 0;      
    move_to_block(new_block,0,fs);  

    while(inode.index_vector[k] != 0 && last_entry_num < MAX_DIR_ENTRIES){

        for(uint8_t j = 0; j < sizeof(inode_num_t); j++){

            if(block_free_space_left(new_block, fs) == 0){
                k++;
                new_block = inode.index_vector[k];
                move_to_block(new_block,0,fs);
            }

            fread(&byte, 1,1,fs->file);    
            inode_num_bytes[j] = byte & (0xff >> j * 8); 
        }

        memcpy(&dir->entries[last_entry_num].inode_index, inode_num_bytes, sizeof(inode_num_t));

        if(dir->entries[last_entry_num].inode_index == 0)
            break;
            

        for(uint8_t j = 0; j < sizeof(file_name_lenght_t); j++){

            if(block_free_space_left(new_block, fs) == 0){
                k++;
                new_block = inode.index_vector[k];
                move_to_block(new_block,0,fs);
            }

            fread(&byte, 1,1,fs->file);    
            file_name_lenght_bytes[j] = byte & (0xff >> j * 8); 
        }

        memcpy(&dir->entries[last_entry_num].name_lenght, file_name_lenght_bytes, sizeof(file_name_lenght_t));
        
        for(uint32_t i = 0; i < (dir->entries[last_entry_num].name_lenght); i++){

            if(block_free_space_left(new_block, fs) == 0){
                k++;
                new_block = inode.index_vector[k];
                move_to_block(new_block,0,fs);
            }

            fread(&(dir->entries[last_entry_num].name[i]), 1,1,fs->file);
            dir->entries[last_entry_num].name[i+1] = '\0';
        }
    
        last_entry_num++;
    }        
    dir->entries[last_entry_num].inode_index = 0;
    return 1;
}

/*

Tokenizzazione path e lookup

*/


uint32_t tokenize_path(const char* path, char** tokens_buffer){
    
    char path_buffer[1000];
    strcpy(path_buffer,path);
    int n_tokens = 0;
    char* saveptr; //Usato per garantire la rientranza di strtok_r
    char* path_token = strtok_r(path_buffer,"/",&saveptr);
    
    while(path_token != NULL){
        
        strcpy(tokens_buffer[n_tokens],path_token);
        path_token = strtok_r(NULL,"/",&saveptr);
        n_tokens++;
    }

    return n_tokens;
}


/*
Ritorna il numero di inode di un elemento all'interno di una directory
dato il nome.
*/
uint8_t get_dir_element_inode(char* name ,inode_num_t inode_num,filesystem_t* fs){
    
    file_t dir = {0};
    inode_t dir_inode = read_inode(inode_num,fs);
    uint32_t i = 0;
    int8_t ret = read_dir_entries(&dir,dir_inode,fs);
    
    if(ret == 0)
        return 0;

    while(i < MAX_DIR_ENTRIES && (strcmp(dir.entries[i].name,name) != 0)){
        i++;
    }

    if(i < MAX_DIR_ENTRIES && strcmp(dir.entries[i].name, name) == 0)
        return dir.entries[i].inode_index;
    else
        return 0;

}


inode_num_t inode_from_path(const char* path,filesystem_t* fs){
	
	if(strcmp(path,"/") == 0)
		return 0;

    char* tokens[500];
    uint8_t i = 0;

    for(int i = 0 ; i<500; i++)
        tokens[i] = malloc(500);

    uint32_t n_tokens = tokenize_path(path,tokens);

    if(n_tokens == 1)
        return get_dir_element_inode(tokens[0],0,fs);


    inode_num_t last_element_inode = get_dir_element_inode(tokens[i++],0,fs);

    if(last_element_inode == 0)
        return 0;

    n_tokens--;
    
    while(n_tokens>0){
        last_element_inode = get_dir_element_inode(tokens[i++],last_element_inode,fs);
        
        if(last_element_inode == 0)
            return 0;

        n_tokens--;
    }

    for(int j = 0 ; j<500; j++)
        free(tokens[j]);

   return last_element_inode;
}


/*
Dato un path ritorna la directory che contiene il file individuato dal path

@path il path del file di cui si vuole ottenere riferimento alla directory in cui è contenuto
*/
inode_num_t parent_dir_inode_from_path(const char* path,filesystem_t* fs){

    char* tokens[500];
    uint8_t i = 0;

    for(int i = 0 ; i<500; i++)
        tokens[i] = malloc(500);

    uint32_t n_tokens = tokenize_path(path,tokens);

    if(n_tokens <= 1)
        return 0;

    inode_num_t last_file = get_dir_element_inode(tokens[i++],0,fs);

    if(last_file == 0)
        return 0;

    n_tokens--;
    
    while(n_tokens>1){
        last_file = get_dir_element_inode(tokens[i++],last_file,fs);
        
        if(last_file == 0)
            return 0;

        n_tokens--;
    }

    for(int j = 0 ; j<500; j++)
        free(tokens[j]);

   return last_file;

}
 
/*-------------------------*/


/*Operazioni */

uint32_t write_to_file(inode_num_t inode_num,const char* buf, size_t size,off_t offset,filesystem_t* fs){

    uint8_t i = 0;
    uint32_t j = 0;
    uint8_t block_offset = offset / BLOCK_SIZE;
    inode_t inode = read_inode(inode_num,fs);
    block_num_t block;

    while(i < block_offset){

        if(inode.index_vector[i] == 0)
            inode.index_vector[i] = assign_block_to_inode(inode_num,fs);
        i++;

    }

        if(inode.index_vector[i] == 0)   
            inode.index_vector[i] = assign_block_to_inode(inode_num,fs);

    block = inode.index_vector[i];
    move_to_block(block,0,fs);

    while(j < size){

        block = reach_new_block_if_full(inode_num,block,fs);
        fwrite(buf + j,1,1,fs->file);
        j++;

    }

    uint32_t new_size = (inode.size - (inode.size - offset)) + size;
    update_file_size(inode_num,new_size,fs);

    fflush(fs->file);
    return new_size;
}

void read_file(char* buf ,inode_num_t inode_num ,off_t offset ,filesystem_t* fs){

    inode_t inode = read_inode(inode_num,fs);
    block_num_t block = inode.index_vector[0];
    uint32_t block_offset = offset / BLOCK_SIZE;

    move_to_block(block,0,fs);  

    while(inode.index_vector[block_offset] != 0){
        
        for(uint32_t i = 0; i < inode.size ; i++){

            if(block_free_space_left(block, fs) == 0){
                block = inode.index_vector[block_offset];
                move_to_block(block,0,fs);
            }
            fread(buf + i, 1,1,fs->file);
        }
        block_offset++;
    }        

}







/*-----------------------*/