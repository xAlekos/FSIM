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

#define SEEK_FREESPACE_TABLE_SET 256


typedef struct filesystem{

    FILE* file;
    uint8_t* free_space_table;
    uint8_t* inode_table;

}filesystem_t;

/*

Blocco di 256 byte in cui i primi 4 byte rappresentano i permessi ed il tipo del file, 
i seguenti 252 byte rappresentano gli indici dei blocchi all'interno dei quali si trovano i dati
del file rappresentato dall'inode.

*/
typedef struct inode{

    mode_t mode;
    uint16_t size;
    uint8_t index_vector[MAX_BLOCKS_PER_NODE];

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

    char name[MAX_FILE_NAME];
    char content[MAX_FILE_CONTENT];

    uint8_t inode_num;

    uint16_t size;
    mode_t mode;
    dir_entry_t entries[MAX_DIR_ENTRIES];

}file_t;


uint16_t block_free_space_left(uint8_t block_num,filesystem_t* fs);
void move_to_block(uint8_t block_num,uint8_t offset ,filesystem_t* fs);
uint8_t assign_block_to_inode(uint8_t inode,filesystem_t* fs);
void sync_fs(filesystem_t* fs);

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
void sync_inode_table(filesystem_t* fs){
    
    fseek(fs->file,0,SEEK_SET);
    fwrite(fs->inode_table,sizeof(uint8_t),MAX_INODES,fs->file);

}

/*
    Legge da un file che rappresenta in dispositivo di memorizzazione del file system lo stato attuale della 
    tabella degli inode
*/
void read_inode_table(filesystem_t* fs){
    
    fseek(fs->file,0,SEEK_SET);
    fread(fs->inode_table,sizeof(uint8_t),MAX_INODES,fs->file);

}
/*                             
    Scorre la tabella degli inode fino a trovare un inode libero,
    ritorna il numero di inode libero, 0 altrimenti.
    (0 non potrà mai essere libero in quanto è assegnato alla directory root)
*/
uint8_t get_free_inode_number(filesystem_t* fs){

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

    inode_t inode;
    uint8_t block = fs->inode_table[inode_num];
    move_to_block(block,0,fs);
    fread(&(inode.mode),4,1,fs->file);
    move_to_block(block,4,fs);
    fread(&(inode.mode),1,1,fs->file);
    move_to_block(block,5,fs);
    fread(&(inode.index_vector),1,251,fs->file);
    
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
uint8_t reach_new_block_if_full(uint8_t inode_num,uint8_t starting_block,filesystem_t* fs){

    uint8_t new_block;
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
    facente parte di una directory, è dunque necessario spostarsi nel blocco corretto prima di chimare questa funzione.
*/
void write_file_info(file_t* file,uint8_t dir_inode_num ,uint8_t starting_block,filesystem_t* fs){

    uint8_t i = 0;
    uint8_t file_name_lenght = strlen(file->name);
    uint8_t block = starting_block;

    printf("write to: %d\n",ftell(fs->file));
    block = reach_new_block_if_full(dir_inode_num,block,fs);
    fwrite(&(file->inode_num),1,1,fs->file);
    block = reach_new_block_if_full(dir_inode_num,block,fs);
    printf("write to: %d\n",ftell(fs->file));
    fwrite(&file_name_lenght,1,1,fs->file);

    while(i < file_name_lenght){

        block = reach_new_block_if_full(dir_inode_num,block,fs);
        printf("write to: %d\n",ftell(fs->file));
        fwrite(file->name + i,1,1,fs->file);
        i++;

    }

}

/*
    Sposta la posizione all'interno del file che rappresenta il file system ad un blocco dato.
*/
void move_to_block(uint8_t block_num,uint8_t offset ,filesystem_t* fs){
    uint16_t ret = fseek(fs->file,block_num*BLOCK_SIZE + offset,SEEK_SET);
}


/*
    
    Dato un blocco sposta la posizione all'interno del file che rappresenta il file system, si sposta 
    al primo byte libero all'interno del blocco.
    ritorna -1 se il blocco è pieno, altrimenti lo scostamento all'interno del blocco.
    Se il blocco in esame è un inode questo va segnalato tramite il parametro is_inode.

*/
int16_t move_to_empty_space_in_block(uint8_t block_num,uint8_t is_inode,filesystem_t* fs){

    uint16_t i = 0;
    char ch = 0;
    uint8_t offset = 0;
    uint16_t start_pos;

    if(is_inode == 1)
        offset = 5;

    move_to_block(block_num,offset,fs);
    start_pos = ftell(fs->file);

    fread(&ch,1,1,fs->file);
    fseek(fs->file,start_pos,SEEK_SET);

    while(ch != 0 && i <= BLOCK_SIZE - offset){
        
        i++;
        fseek(fs->file,start_pos + i,SEEK_SET);
        fread(&ch,1,1,fs->file); 
        if(ch == 0)
            fseek(fs->file,start_pos + i,SEEK_SET);

    }

    if(i >= BLOCK_SIZE - offset) //Se il blocco è pieno!
        return -1;

    else{
        return i;
    }
}

/*
Sposta la posizione all'interno del file all'ultimo blocco dati di un file a partire dal numero di inode, non dovesse essere presente un blocco dati libero,
ne assegna uno.
*/

uint8_t move_to_data_block(uint8_t inode_num, filesystem_t* fs){

    uint8_t i = 0;
    inode_t inode = read_inode(inode_num,fs); 
    uint16_t ret;
    while(inode.index_vector[i] != 0){
        //sono stupido! questa non ha senso chiamarla se l'inode non è l'ultimo!
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
void assign_inode_to_block(uint8_t inode, uint8_t block ,filesystem_t* fs){
    
    fs->inode_table[inode] = block;
    fs->free_space_table[block] = 1;
    sync_fs(fs);

}

/*
Assegna un blocco libero ad un inode, imposta il blocco assegnato come occupato.
*/
uint8_t assign_block_to_inode(uint8_t inode,filesystem_t* fs){
    
    uint8_t ret;
    uint8_t block_num = get_and_set_free_block(fs);
    uint8_t inode_block_num = fs->inode_table[inode];

    ret = move_to_empty_space_in_block(inode_block_num,1,fs);
    
    if(ret == -1) //Il blocco è pieno 
        return 0;

    fwrite(&block_num,1,1,fs->file);
    
    return block_num;
}


uint16_t block_free_space_left(uint8_t block_num,filesystem_t* fs){
    
    uint16_t pos = ftell(fs->file);
    uint16_t space_left =  BLOCK_SIZE*(block_num + 1) - pos;

    return space_left;

}


/*----------------------------------------*/

/*Gestione Filesystem*/

void sync_fs(filesystem_t* fs){
    
    sync_inode_table(fs);
    sync_freespace_table(fs);

}


filesystem_t* init_fs(filesystem_t* fs){
    
    filesystem_t* new_fs = malloc(sizeof(filesystem_t));
    new_fs->free_space_table = init_freespace_table();
    new_fs->inode_table = init_inode_table();
    new_fs->file = load_fs("./FS");
    format_fs(new_fs->file);

    if(new_fs->inode_table == NULL || new_fs->free_space_table == NULL)
        return NULL;

    sync_fs(new_fs);    
    return new_fs;

}


/*---------------------------*/


/*Manipolazione dei file*/

file_t* create_test_file(){

    file_t* new_file = malloc(sizeof(file_t));
    memmove(new_file->name,"Testa",5);
    new_file->size = 0;
    new_file->mode = S_IFREG | 0755;

    return new_file;

}

file_t* create_root_dir(){

    file_t* new_file = malloc(sizeof(file_t));

    new_file->mode = S_IFDIR | 0444;
    new_file->size = 0;
    memset(new_file->entries,0,sizeof(dir_entry_t) * MAX_DIR_ENTRIES);
    return new_file;
}

int8_t sync_new_file(file_t* file, filesystem_t* fs){
    
    uint8_t inode_num = get_free_inode_number(fs);
    uint8_t block_num = get_and_set_free_block(fs);
    
    if(block_num == 0)
        return -1;

    file->inode_num = inode_num;
    assign_inode_to_block(file->inode_num, block_num, fs);
    move_to_block(block_num,0,fs);
    fwrite(&(file->mode),sizeof(file->mode),1,fs->file); //salva sul dispositivo di memorizzazione i metadati del file
    fwrite(&(file->size),sizeof(file->size),1,fs->file); 
    sync_fs(fs);


}

uint8_t is_inode_full(uint8_t dir_inode_num, filesystem_t* fs){
    
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

int8_t new_file_to_dir(file_t* file,char* path , filesystem_t* fs){

    uint8_t dir_inode_num;
    uint8_t ret;
    uint8_t block;

    if(strcmp(path,"/") == 0)
        dir_inode_num = 0;
    /*TODO ! 1 Altrimenti trova la dir dal path.*/
             

    ret = is_inode_full(dir_inode_num,fs);
    
    if(ret == 1)
        return -1;

    sync_new_file(file,fs);
    block = move_to_data_block(dir_inode_num,fs);
    write_file_info(file,dir_inode_num,block,fs);
    
}



int main(){

    //format_fs(fs);
    
    filesystem_t* filesystem = init_fs(filesystem);


    file_t* root_dir = create_root_dir();
    sync_new_file(root_dir,filesystem);
    file_t* test= create_test_file();

    for(int i = 0; i < 65;i++)
        new_file_to_dir(test,"/",filesystem);
    while(getchar() == 'a'){
        new_file_to_dir(test,"/",filesystem);
        getchar();
    }
    //new_file_to_dir(test,"/",filesystem);

    fclose(filesystem->file);
    
}