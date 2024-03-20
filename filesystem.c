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


typedef struct inode{

    mode_t mode;
    uint8_t index_vector[252];

}inode_t;

typedef struct dir_entry{
    
    uint8_t inode_address;
    uint8_t name_lenght;
    char* name[MAX_FILE_NAME];

}dir_entry_t;


typedef struct file{

    char* name[MAX_FILE_NAME];
    char* content[MAX_FILE_CONTENT];

    mode_t mode;
    dir_entry_t entries[MAX_DIR_ENTRIES];

}file_t;


FILE* load_fs(char* path){
    FILE* fs = fopen(path,"rb+");
    if(fs == NULL)
        return NULL;
    return fs;
}

/* Gestione tabella degli inode*/
uint8_t* init_inode_table(){

    uint8_t* new_inode_table =  calloc(MAX_INODES , sizeof(uint8_t));
    
    if(new_inode_table == NULL)
        return NULL;
    
    return new_inode_table; 

}

void sync_inode_table(uint8_t* inode_table, FILE* fs){
    
    fseek(fs,0,SEEK_INODES_TABLE_SET);
    fwrite(inode_table,sizeof(uint8_t),MAX_INODES,fs);

}

void read_inode_table(uint8_t* inode_table, FILE* fs){
    
    fseek(fs,0,SEEK_INODES_TABLE_SET);
    fread(inode_table,sizeof(uint8_t),MAX_INODES,fs);

}
/*                                                        */


/* Gestione dello spazio libero */
uint8_t* init_freespace_table(){

    uint8_t* new_freespace_table =  calloc(MAX_BLOCKS_NUM , sizeof(uint8_t));
    new_freespace_table[0] = 1; //Il blocco 0 contiene la tabella degli inode
    new_freespace_table[1] = 1; //Il blocco 1 contiene la tabella dello spazio libero 

    if(new_freespace_table == NULL)
        return NULL;
    
    return new_freespace_table; 

}

void sync_freespace_table(uint8_t* freespace_table, FILE* fs){
    
    fseek(fs,0,SEEK_FREESPACE_TABLE_SET);
    fwrite(freespace_table,sizeof(uint8_t),MAX_BLOCKS_NUM,fs);

}

void read_freespace_table(uint8_t* freespace_table, FILE* fs){
    
    fseek(fs,0,SEEK_FREESPACE_TABLE_SET);
    fread(freespace_table,sizeof(uint8_t),MAX_BLOCKS_NUM,fs);

}

uint8_t find_free_block(uint8_t* freespace_table){
    
    int i = 0;
    
    while(freespace_table[i] != 0 && i < MAX_BLOCKS_NUM)
        i++;
    
    if(freespace_table[i] == 0)
        return i;
    
    else 
        return 0;
    

}
/*----------------------------------------*/


void create_file(){



}


int main(){
    FILE* fs = load_fs("./FS");
    
    if(fs == NULL){
        perror("Errore apertura file");
        return 1;
    }

    uint8_t* inode_table = init_inode_table();
    read_inode_table(inode_table,fs);
    uint8_t* free_space_table = init_freespace_table();
    read_freespace_table(free_space_table,fs);

    if(inode_table == NULL)
        return 1;

    

    printf("filesystem\n");
}