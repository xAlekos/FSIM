/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPLv2.
  See the file COPYING.
*/

/** @file
 *
 * minimal example filesystem using high-level API
 *
 * Compile with:
 *
 *     gcc -Wall hello.c `pkg-config fuse3 --cflags --libs` -o hello
 *
 * ## Source code ##
 * \include hello.c
 */


#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include "filesystem.h"



filesystem_t* filesystem;

static void *hello_init(struct fuse_conn_info *conn,
			struct fuse_config *cfg)
{
	(void) conn;
	cfg->kernel_cache = 1;
	cfg->use_ino = 1;

	init_root_dir(filesystem);
	sync_test_files(filesystem,53);
	sync_test_dir(filesystem,5);
	fflush(filesystem->file);

	return NULL;
}

static int hello_getattr(const char *path, struct stat *stbuf,
			 struct fuse_file_info *fi)
{
	(void) fi;
	inode_t inode;
	inode_num_t inode_num; 
	printf("getattr %s\n",path);

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		inode_num = 0;
		inode = read_inode(inode_num,filesystem);
		stbuf->st_mode = inode.mode;
		stbuf->st_size = inode.size;
		stbuf->st_nlink = 2;
		stbuf->st_ino = inode_num;
		return 0;
	} 
	
	inode_num = inode_from_path(path,filesystem);

	if(inode_num == 0)
		return -ENOENT;
	
	if(inode_num != 0){
		inode = read_inode(inode_num,filesystem);	
		stbuf->st_mode = inode.mode;
		stbuf->st_size = inode.size;
		stbuf->st_nlink = 2;
		stbuf->st_ino = inode_num;
		return 0;
	}
	
	return -ENOENT;
}

static int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi,
			 enum fuse_readdir_flags flags)
{
	(void) offset;
	(void) fi;
	(void) flags;
	printf("readdir %s\n",path);

	if (strcmp(path, "/") != 0)
		return -ENOENT;
	
	file_t dir = {0};
	inode_t inode = read_inode(0,filesystem);
	read_dir_entries(&dir,inode,filesystem);
	uint16_t i;

	filler(buf, ".", NULL, 0, 0);
	filler(buf, "..", NULL, 0, 0);

	while(dir.entries[i].inode_index != 0){
		filler(buf, dir.entries[i].name, NULL, 0, 0);
		i++;
	}
	return 0;
}

static int hello_open(const char *path, struct fuse_file_info *fi)
{

	printf("open %s\n",path);
	//if ((fi->flags & O_ACCMODE) != O_RDONLY)
	//	return -EACCES;

	return 0; 
}


/*

static int myfs_create(const char* path, mode_t mode, struct fuse_file_info * fi){
	//TODO sistemare rilevazione errori
	(void)fi;
	filenode_t* parent_dir = GetParentDir(path);
	
	char* name = FileNameFromPath(path);
	filenode_t* newfile = AddNewFileToDir(parent_dir, name, NULL , S_IFREG | mode, REG , NULL);
	if(newfile == NULL)
		return -EEXIST;
	free(name);
	printf("create file %s\n",path);
	return 0;
*/


static int myfs_write(const char *path, const char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	(void) fi;

	uint8_t inode_num = inode_from_path(path,filesystem);
	uint16_t new_size; 

	printf("Writing to file %s\n",path);

	new_size = write_to_file(inode_num,buf,size,offset,filesystem);

	return new_size;
}

static int myfs_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	(void) fi;

	uint8_t inode_num = inode_from_path(path,filesystem);
	inode_t inode = read_inode(inode_num,filesystem);
	size_t len;
	printf("Reading file %s\n",path);
	
	if(inode_num == 0)
		return -ENOENT;
		
	//if file->size == 0 return 0

	len = inode.size;


	if(offset < len) {

		if (offset + size > len)
			size = len - offset;
		read_file(buf,inode_num,offset,filesystem);

	} 
	
	else
		size = 0;

	return size;
}


static const struct fuse_operations hello_oper = {
	.init           = hello_init,
	.getattr	= hello_getattr,
	.readdir	= hello_readdir,
	.open		= hello_open,
	.read		= myfs_read,
	.write	= myfs_write
	//.create		= hello_create
};

int main(int argc, char *argv[])
{
	int ret;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	init_fs(&filesystem);
	ret = fuse_main(args.argc, args.argv, &hello_oper, NULL);
	fuse_opt_free_args(&args);
	return ret;
}
