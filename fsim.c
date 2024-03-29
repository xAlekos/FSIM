#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stddef.h>

#include "filesystem.h"



static void *myfs_init(struct fuse_conn_info *conn,
			struct fuse_config *cfg)
{
	(void) conn;
	printf("Init Conn Kernel Module\n");
	cfg->kernel_cache = 1;
	return NULL;
	
}

static int myfs_getattr(const char *path, struct stat *stbuf,
			 struct fuse_file_info *fi)
{
	(void) fi;
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));

	printf("Get attr %s\n",path);

	if(strcmp(path, "/") == 0){
		stbuf->st_mode = S_IFDIR | 0444;
		stbuf->st_nlink = 2;
		return 0;
	}
	
	filenode_t* req_file = FileFromPath(path);


	if(req_file != NULL){

		stbuf->st_mode = req_file->mode;
		stbuf->st_nlink = 1;
		stbuf->st_size = req_file->file_size;
	}
	else
		return -ENOENT;

	return res;
}





static int myfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi,
			 enum fuse_readdir_flags flags)
{
	(void) offset;
	(void) fi;
	(void) flags;
	filenode_t* req_dir = FileFromPath(path);
	printf("Readdir %s\n",path);

	if (req_dir == NULL || req_dir->type != DIR)
		return -ENOENT;
	
	filler(buf, ".", NULL, 0, 0);
	filler(buf, "..", NULL, 0, 0);

	for(int i = 0; i < DIR_ENTRIES_DIM; i++){

		if(req_dir->dir_content[i] != NULL && req_dir->dir_content[i]->filenode != NULL)
			ReadCollisionList(req_dir->dir_content[i],buf,filler);

	}

	return 0;
}


static const struct fuse_operations myfs_oper = {

	.init       = myfs_init,
	.getattr	= myfs_getattr,
	.readdir	= myfs_readdir,
	.open		= myfs_open,
	.read		= myfs_read,
	.write 		= myfs_write,
	.mkdir      = myfs_mkdir,
	.create     = myfs_create,
	.truncate   = myfs_truncate,
	.rename     = myfs_rename,
	.unlink     = myfs_unlink,
	.chmod   	= myfs_chmod
};

int main(int argc, char *argv[])
{
	
	int ret;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	ret = fuse_main(args.argc, args.argv, &myfs_oper, NULL);
	fuse_opt_free_args(&args);
	return ret;
}
