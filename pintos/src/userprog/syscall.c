#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "list.h"
#include "process.h"

//-----------------------------------------------------------------------
	typedef void(*CALL_PROC)(struct intr_frame *);
	CALL_PROC fpt[21];
	void toWrite(struct intr_frame*);
	void toExit(struct intr_frame*);
	void toCreate(struct intr_frame*);
	void toOpen(struct intr_frame*);
	void toClose(struct intr_frame*);
	void toRead(struct intr_frame*);
	void toFileSize(struct intr_frame*);
	void toExec(struct intr_frame*);
	void toWait(struct intr_frame*);
	void toSeek(struct intr_frame*);
	void toRemove(struct intr_frame*);
	void toTell(struct intr_frame*);
	void toHalt(struct intr_frame*);	

//------------------------------------------------------------------------

static void syscall_handler (struct intr_frame *);
void* check_address(const void*);
struct proc_file* list_search(struct list* files, int fd);

struct proc_file {
	struct file* ptr;
	int fd;
	struct list_elem elem;
};

void syscall_init (void) {
  	intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");

	for(int i=0; i<21; i++) fpt[i] = NULL;

	fpt[SYS_WRITE] 		= toWrite;
	fpt[SYS_EXIT]  		= toExit;
	fpt[SYS_CREATE] 	= toCreate;
	fpt[SYS_OPEN]  		= toOpen;
	fpt[SYS_CLOSE]  	= toClose;
	fpt[SYS_READ]  		= toRead;
	fpt[SYS_FILESIZE]  	= toFileSize;
	fpt[SYS_EXEC]  		= toExec;
	fpt[SYS_WAIT]  		= toWait;
	fpt[SYS_SEEK]  		= toSeek;
	fpt[SYS_REMOVE]  	= toRemove;
	fpt[SYS_TELL]  		= toTell;
	fpt[SYS_HALT]  		= toHalt;

}

static void syscall_handler (struct intr_frame *f ) {
  	int * p = f->esp;
	check_address(p);
	int No = *((int*)(f->esp));
	fpt[No](f);

}


//====================syscal0===============

void toHalt(struct intr_frame *f){
	shutdown_power_off();		
	f->eax = NULL;//0
}

//====================syscal1===============


void toOpen(struct intr_frame *f){
	int * p = f->esp;
	check_address(p+1);
	check_address(*(p+1));
	acquire_filesys_lock();
	struct file* fptr = filesys_open (*(p+1));
	release_filesys_lock();
	if(fptr==NULL){
		f->eax = -1;
	}else{
		struct proc_file *pfile = malloc(sizeof(*pfile));
		pfile->ptr = fptr;
		pfile->fd = thread_current()->fd_count;
		thread_current()->fd_count++;
		list_push_back (&thread_current()->files, &pfile->elem);
		f->eax = pfile->fd;

	}
}


void toExit(struct intr_frame *f){
	int * p = f->esp;
	check_address(p+1);
	exit_proc(*(p+1));
}

void toRemove(struct intr_frame *f){
	int *p = f->esp;
	check_address(p+1);
	check_address(*(p+1));
	char *ff = (char*)*((int*)f->esp+1);
	f->eax = filesys_remove(ff);
}

void toClose(struct intr_frame *f){
	int * p = f->esp;
	check_address(p+1);
	acquire_filesys_lock();
	close_file(&thread_current()->files,*(p+1));
	release_filesys_lock();
}

void toTell(struct intr_frame *f){
	int * p = f->esp;
	check_address(p+1);
	acquire_filesys_lock();
	f->eax = file_tell(list_search(&thread_current()->files, *(p+1))->ptr);
	release_filesys_lock();
}

void toFileSize(struct intr_frame *f){
	int * p = f->esp;
	check_address(p+1);
	acquire_filesys_lock();
	f->eax = file_length (list_search(&thread_current()->files, *(p+1))->ptr);
	release_filesys_lock();
}


void toExec(struct intr_frame *f){
	int * p = f->esp;
	check_address(p+1);
	check_address(*(p+1));
	f->eax = exec_proc(*(p+1));
}


void toWait(struct intr_frame *f){
	int * p = f->esp;
	check_address(p+1);
	f->eax = process_wait(*(p+1));
}
//====================syscal2===============
// offset = 3
void toCreate(struct intr_frame *f){
	int *p = f->esp;
	check_address(p+5);
	check_address(*(p+4));
	acquire_filesys_lock();
	f->eax = filesys_create(*(p+4),*(p+5));
	release_filesys_lock();
}

void toSeek(struct intr_frame *f){
	int * p = f->esp;
	check_address(p+5);
	acquire_filesys_lock();
	file_seek(list_search(&thread_current()->files, *(p+4))->ptr,*(p+5));
	release_filesys_lock();
}

//====================syscal3================
// offset = 4


void toRead(struct intr_frame *f){
	int * p = f->esp;
	check_address(p+7);
	check_address(*(p+6));
	if(*(p+5)==0){
		int i;
		uint8_t* buffer = *(p+6);
		for(i=0;i<*(p+7);i++)
			buffer[i] = input_getc();
		f->eax = *(p+7);
	}
	else{
		struct proc_file* fptr = list_search(&thread_current()->files, *(p+5));
		if(fptr==NULL){
			f->eax=-1;
		}else{
			acquire_filesys_lock();
			f->eax = file_read (fptr->ptr, *(p+6), *(p+7));
			release_filesys_lock();
		}
	}
}

void toWrite(struct intr_frame *f){
		int *esp = (int*)f->esp;
		check_address(esp+7);
		check_address(*(esp+6));
		int fd = *(esp+2);
		char *buffer = (char*)*(esp+6);
		unsigned size = *(esp+7);  
		if(fd == STDOUT_FILENO){
			putbuf(buffer, size);
			f->eax = 0;
		}else{
			int *p = f->esp;
			struct proc_file* fptr = list_search(&thread_current()->files, *(p+5));
			if(fptr==NULL){
				f->eax=-1;
			}else{
				acquire_filesys_lock();
				f->eax = file_write (fptr->ptr, *(p+6), *(p+7));
				release_filesys_lock();
			}		
		}
	}

//=============Other functions================

int exec_proc(char *file_name){
	acquire_filesys_lock();
	char * fn_cp = malloc (strlen(file_name)+1);
	  strlcpy(fn_cp, file_name, strlen(file_name)+1);  
	  char * save_ptr;
	  fn_cp = strtok_r(fn_cp," ",&save_ptr);
	  struct file* f = filesys_open (fn_cp);
	  if(f==NULL){
	  	release_filesys_lock();
	  	return -1;
	  }
	  else{
	  	file_close(f);
	  	release_filesys_lock();
	  	return process_execute(file_name);
	  }
}

void exit_proc(int status){
	struct list_elem *e;
      	for (e = list_begin (&thread_current()->parent->child_proc); e != list_end (&thread_current()->parent->child_proc); e = list_next (e)){
          struct child *f = list_entry (e, struct child, elem);
          if(f->tid == thread_current()->tid){
          	f->used = true;
          	f->exit_error = status;
          }
        }
	thread_current()->exit_error = status;
	if(thread_current()->parent->waitingon == thread_current()->tid)
		sema_up(&thread_current()->parent->child_lock);

	thread_exit();
}

void* check_address(const void *vaddr){
	if (!is_user_vaddr(vaddr)){
		exit_proc(-1);
	}
	void *ptr = pagedir_get_page(thread_current()->pagedir, vaddr);
	if (!ptr){
		exit_proc(-1);
	}
	return ptr;
}

struct proc_file* list_search(struct list* files, int fd){
	struct list_elem *e;
      	for (e = list_begin (files); e != list_end (files); e = list_next (e)){
          	struct proc_file *f = list_entry (e, struct proc_file, elem);
          	if(f->fd == fd){
          		return f;
		}
        }
   return NULL;
}

void close_file(struct list* files, int fd){
	struct list_elem *e;
	struct proc_file *f;
      	for (e = list_begin (files); e != list_end (files); e = list_next (e)){
          f = list_entry (e, struct proc_file, elem);
          if(f->fd == fd){
          	file_close(f->ptr);
          	list_remove(e);
          }
        }
    free(f);
}








