#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stddef.h>

#include "filesystem.h"

filesystem_t* filesystem;

static void *myfs_init(struct fuse_conn_info *conn,
			struct fuse_config *cfg)
{
	(void) conn;
	file_t* root_dir;
	file_t* test;
	
	cfg->kernel_cache = 1;

	printf("Init Conn Kernel Module\n");
	

	root_dir = create_root_dir(); //TODO sistemare
    sync_new_file(root_dir,filesystem);
	test = create_test_file();

    for(int i = 0; i < 65;i++){
        new_file_to_dir(test,"/",filesystem);
	}

	free(root_dir);
	free(test);
	
	return NULL;
	
}

static int myfs_getattr(const char *path, struct stat *stbuf,
			 struct fuse_file_info *fi){
	(void) fi;
	inode_t inode;
	memset(stbuf, 0, sizeof(struct stat));
	
	if(strcmp(path,"/") == 0){
		inode = read_inode(0,filesystem);
		stbuf->st_mode = inode.mode;
		stbuf->st_size = inode.size;
		return 0;
	}
	else
		return -ENOENT;
}





static int myfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi,
			 enum fuse_readdir_flags flags)
{
	(void) offset;
	(void) fi;
	(void) flags;
	
	filler(buf, ".", NULL, 0, 0);
	filler(buf, "..", NULL, 0, 0);

	return 0;
}


static const struct fuse_operations myfs_oper = {

	.init       = myfs_init,
	.getattr	= myfs_getattr,
	.readdir	= myfs_readdir,

};

int main(int argc, char *argv[])
{
	
	int ret;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	init_fs(&filesystem);
	ret = fuse_main(args.argc, args.argv, &myfs_oper, NULL);
	fuse_opt_free_args(&args);
	return ret;
}
