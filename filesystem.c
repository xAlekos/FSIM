#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>

#define MAX_FILE_NAME 100
#define MAX_FILE_CONTENT 1024       //Ogni file può essere composto massimo da 4 blocchi 
#define BLOCK_SIZE 256              
#define MAX_BLOCKS_NUM 256
#define MAX_INODES 256
#define MAX_DIR_ENTRIES 256

#define SEEK_INODES_TABLE_SET 0
#define SEEK_FREESPACE_TABLE_SET 256
#define SEEK_BLOCKS_SET 512


/*

Blocco di 256 byte in cui i primi 4 byte rappresentano i permessi ed il tipo del file, 
i seguenti 252 byte rappresentano gli indici dei blocchi all'interno dei quali si trovano i dati
del file rappresentato dall'inode.

*/
typedef struct inode{

    mode_t mode;
    uint8_t index_vector[252];

}inode_t;

/*
Rappresentazione del contenuto di una directory, ogni file contenuto in una directory 
è rappresentato da una dir_entry. 
*/
typedef struct dir_entry{
    
    uint8_t inode_index;
    uint8_t name_lenght;

    char* name[MAX_FILE_NAME];

}dir_entry_t;


/*
Rappresentazione in memoria di un file.
*/
typedef struct file{

    char* name[MAX_FILE_NAME];
    char* content[MAX_FILE_CONTENT];

    uint8_t inode_num;

    size_t size;
    mode_t mode;
    dir_entry_t entries[MAX_DIR_ENTRIES];

}file_t;


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
uint8_t* init_inode_table(){

    uint8_t* new_inode_table =  calloc(MAX_INODES , sizeof(uint8_t));
    
    if(new_inode_table == NULL)
        return NULL;
    
    return new_inode_table; 

}
/*
    Salva sul file che rappresenta il dispositivo di memorizzazione del file system lo stato attuale 
    della tabella degli inode
*/
void sync_inode_table(uint8_t* inode_table, FILE* fs){
    
    fseek(fs,0,SEEK_INODES_TABLE_SET);
    fwrite(inode_table,sizeof(uint8_t),MAX_INODES,fs);

}

/*
    Legge da un file che rappresenta in dispositivo di memorizzazione del file system lo stato attuale della 
    tabella degli inode
*/
void read_inode_table(uint8_t* inode_table, FILE* fs){
    
    fseek(fs,0,SEEK_INODES_TABLE_SET);
    fread(inode_table,sizeof(uint8_t),MAX_INODES,fs);

}
/*                                                        */

uint8_t get_free_inode_number(uint8_t* inode_table){

    int i = 0;
    while(i < MAX_INODES && inode_table[i] != 0)
        i++;
    
    if(inode_table[i] == 0)
        return i;
    else 
        return 0;

}

void assign_inode_to_block(uint8_t inode, uint8_t block ,uint8_t* inode_table, uint8_t* free_space_table){
    
    inode_table[inode] = block;
    free_space_table[block] = 1;

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
void sync_freespace_table(uint8_t* freespace_table, FILE* fs){

    fseek(fs,SEEK_FREESPACE_TABLE_SET,0);
    fwrite(freespace_table,sizeof(uint8_t),MAX_BLOCKS_NUM,fs);

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

uint8_t get_and_set_free_block(uint8_t* freespace_table){
    
    int i = 0;
    
    while(i < MAX_BLOCKS_NUM && freespace_table[i] != 0)
        i++;
    
    if(freespace_table[i] == 0){
        freespace_table[i] = 1;
        return i;
    }
    
    else 
        return 0;
    

}


/*
    Utils
*/
void move_to_block(uint8_t block_num, FILE* fs){
    fseek(fs,block_num*BLOCK_SIZE,SEEK_SET);
}



/*----------------------------------------*/

/*Manipolazione dei file*/

file_t* create_test_file(){

    file_t* new_file = malloc(sizeof(file_t));
    memmove(new_file->name,"Test",4);
    memmove(new_file->content,"Test_content",13);
    new_file->size = 13;

    return new_file;

}

file_t* create_root_dir(){

    file_t* new_file = malloc(sizeof(file_t));

    new_file->mode = S_IFDIR | 0444;
    new_file->size = 0;
    memset(new_file->entries,0,sizeof(dir_entry_t) * MAX_DIR_ENTRIES);
    return new_file;
}

void sync_file_inode(file_t* file, FILE* fs, uint8_t* inode_table,uint8_t* free_space_table){
    
    uint8_t inode_num = get_free_inode_number(inode_table);
    uint8_t block_num = get_and_set_free_block(free_space_table);
    file->inode_num = inode_num;
    assign_inode_to_block(file->inode_num, block_num, inode_table, free_space_table);
    move_to_block(block_num,fs);
    fwrite(&(file->mode),4,1,fs); //salva sul dispositivo di memorizzazione i metadati del file
    sync_freespace_table(free_space_table,fs);
    sync_inode_table(inode_table,fs);


}



void sync_new_file(file_t* file, FILE* fs, uint8_t* inode_table,uint8_t* free_space_table){
    

    sync_file_inode(file,fs,inode_table,free_space_table);
    

}

void print_file(file_t* file){

    printf("Name: %s\n",file->name);
    printf("Content: %s\n",file->content);
    printf("Size: %d\n",file->size);

}

int main(){

    FILE* fs = load_fs("./FS");
    //format_fs(fs);
    
    if(fs == NULL){
        perror("Errore apertura file");
        return 1;
    }

    uint8_t* inode_table = init_inode_table();
    uint8_t* free_space_table = init_freespace_table();
    sync_inode_table(inode_table,fs);
    sync_freespace_table(free_space_table,fs);

    if(inode_table == NULL || free_space_table == NULL)
        return 1;

    file_t* root_dir = create_root_dir();

    sync_file_inode(root_dir,fs,inode_table,free_space_table);
    fclose(fs);
    
}