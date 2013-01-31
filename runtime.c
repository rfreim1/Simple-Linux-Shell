/***************************************************************************
 *  Title: Runtime environment 
 * -------------------------------------------------------------------------
 *    Purpose: Runs commands
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.3 $
 *    Last Modification: $Date: 2009/10/12 20:50:12 $
 *    File: $RCSfile: runtime.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: runtime.c,v $
 *    Revision 1.3  2009/10/12 20:50:12  jot836
 *    Commented tsh C files
 *
 *    Revision 1.2  2009/10/11 04:45:50  npb853
 *    Changing the identation of the project to be GNU.
 *
 *    Revision 1.1  2005/10/13 05:24:59  sbirrer
 *    - added the skeleton files
 *
 *    Revision 1.6  2002/10/24 21:32:47  sempi
 *    final release
 *
 *    Revision 1.5  2002/10/23 21:54:27  sempi
 *    beta release
 *
 *    Revision 1.4  2002/10/21 04:49:35  sempi
 *    minor correction
 *
 *    Revision 1.3  2002/10/21 04:47:05  sempi
 *    Milestone 2 beta
 *
 *    Revision 1.2  2002/10/15 20:37:26  sempi
 *    Comments updated
 *
 *    Revision 1.1  2002/10/15 20:20:56  sempi
 *    Milestone 1
 *
 ***************************************************************************/
#define __RUNTIME_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

/************Private include**********************************************/
#include "runtime.h"
#include "io.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

/************Global Variables*********************************************/
int PATHSIZE = 512;
#define NBUILTINCOMMANDS (sizeof BuiltInCommands / sizeof(char*))




/*Set current running process to non-existent*/
//crpid=-1;
/************Function Prototypes******************************************/
/* run command */
static void
RunCmdFork(commandT*, bool);
/* runs an external program command after some checks */
static void
RunExternalCmd(commandT*, bool, bool);
/* resolves the path and checks for exutable flag */
static bool
ResolveExternalCmd(commandT*, char*);
/* forks and runs a external program */
static void
Exec(char*, commandT*, bool, bool);
/* runs a builtin command */
static void
RunBuiltInCmd(commandT*);
/* checks whether a command is a builtin command */
static bool
IsBuiltIn(char*);
/*Checks whether a command should be run in background*/
static bool
IsBG(commandT*);
/*Makes a job for the job list*/
bgjobL* 
MakeJob(pid_t, char*, char*);
/*GetNewJid*/
int 
GetNewJid();
/*Creates a string from the argument list */
char* 
stringFromArgv(char**, int);
/*Gets a job given its jid */
bgjobL* 
GetJobFromJid(int);
/*Gets the most recently add job */
bgjobL* 
GetMRJob();

/*Looks for redirection commands and redirects IO*/
void
RedirectIO(commandT*, int*, int*);

/*Resets IO to stdin and stdout*/
void
ResetIO(int*, int*);

aliasL*
makeAlias(commandT*);

void 
addAlias(commandT*);

void 
deleteAlias(char*);

void
printAliasList();

bool
isAlias(char*);

aliasL*
getAlias(char* cmdName);
/************External Declaration*****************************************/

/**************Implementation***********************************************/


/*
 * RunCmd
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: none
 *
 * Runs the given command.
 */
void
RunCmd(commandT* cmd)
{
  if (cmd->argc <= 0){
   return; 
  } 
  if (IsBG(cmd)){
   //printf("this is BGGGG");
   RunCmdBg(cmd); 
  }
  else{
    RunCmdFork(cmd, TRUE);
  }
  
} /* RunCmd */


/*
 * RunCmdFork
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   bool fork: whether to fork
 *
 * returns: none
 *
 * Runs a command, switching between built-in and external mode
 * depending on cmd->argv[0].
 */
void
RunCmdFork(commandT* cmd, bool fork)
{
  if (cmd->argc <= 0)
    return;
  
  if (IsBuiltIn(cmd->argv[0]))
    {
      RunBuiltInCmd(cmd);
    }
  else
    {
      RunExternalCmd(cmd, fork, FALSE);
    }
} /* RunCmdFork */


/*
 * RunCmdBg
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: none
 *
 * Runs a command in the background.
 */
void
RunCmdBg(commandT* cmd)
{
  // TODO
  
  RunExternalCmd(cmd, TRUE, TRUE);
  //free(newCmd);
  return;
} /* RunCmdBg */


/*
 * RunCmdPipe
 *
 * arguments:
 *   commandT *cmd1: the commandT struct for the left hand side of the pipe
 *   commandT *cmd2: the commandT struct for the right hand side of the pipe
 *
 * returns: none
 *
 * Runs two commands, redirecting standard output from the first to
 * standard input on the second.
 */
void
RunCmdPipe(commandT* cmd)
{
  commandT* t = cmd;
  int i = 0;
  int j;
  while (t != NULL){
	t = t->next;
	i++;
  }
  int cmdCount = i;
  commandT* temp = cmd;
  sigset_t *set;
  set = 0;
  sigemptyset(set);
  sigaddset(set, SIGCHLD);
  sigprocmask(SIG_BLOCK, set, 0);
  pid_t pid;

  int status=0;
  /*Create cmdCount file descriptors for piping*/  
  int fd[cmdCount-1][2];

  /*Set pipe for first command reading from stdin, writing to fd[0][1]*/
  pipe(fd[0]);
  pid=fork();

  if ( pid == 0){
      /*first child*/
      close(fd[0][0]);
      /*Redirect stdout to output pipe of file descriptor 0*/
      dup2(fd[0][1], STDOUT_FILENO);
      close(fd[0][1]);

      sigprocmask(SIG_UNBLOCK, set, 0);
      setpgid(0,0);

      /*Resolve external path*/
      char* path = (char*)malloc(sizeof(char)*PATHSIZE);
      if(ResolveExternalCmd(temp, path)){
	if(execv(path,temp->argv)==-1){
	    printf("%s\n", path);
            printf("Error: %s\n", strerror(errno));
        }
      }
      free(path);
      exit(1);
    }
    else if (pid > 0)
    {

      /*For all future commands, reading from pipe i-1 and write to i*/
      for(i=1; i<cmdCount; i++)
      {
	temp = temp->next;
	/*create pipe to write into unless it is the last command*/
        if ((i+1)!=cmdCount)
          pipe(fd[i]);

        pid = fork();
        if (pid == 0)
        {
	  /*set up input for child to read from.*/
          close(fd[i-1][1]);
          dup2(fd[i-1][0], STDIN_FILENO);
          close(fd[i-1][0]);

	  /*Unless this is the last command, write into current pipe for next command to read*/
          if(i+1 != cmdCount)
          {
            close(fd[i][0]);
            dup2(fd[i][1], STDOUT_FILENO);
            close(fd[i][1]);
          }

	  /*Close all pipes*/
          for(j=0;j<i;j++)
          {
            close(fd[j][1]);
            close(fd[j][0]);
          }

          sigprocmask(SIG_UNBLOCK, set, 0);
          setpgid(0,0);
      	  /*Resolve external path*/
      	  char* path = (char*)malloc(sizeof(char)*PATHSIZE);
      	  if(ResolveExternalCmd(temp, path)){
	     if(execv(path,temp->argv)==-1){
	    	//printf("%s\n", path);
            	//printf("Error: %s\n", strerror(errno));
              }
      	  }
      	  free(path);
          exit(1);
        }
      }
      /*Make sure pipes are closed*/
      for (j=0;j<cmdCount-1;j++)
      {
        close(fd[j][1]);
        close(fd[j][0]);
      }

      sigprocmask(SIG_UNBLOCK, set, 0);

      waitpid(pid,&status,WUNTRACED);
      while (!WIFEXITED(status));
    }
    else
    {
      fprintf(stderr, "error in piping.");
    }
  } 
 /* RunCmdPipe */

/*RedirectIO
 * 
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: none
 *
 * Checks for "<" and >".
 * Updates argv to include up to first redirection.
 * Redirect stdin and stdout.
 */

void
RedirectIO(commandT* cmd, int* stdinFd, int* stdoutFd){
  int i = 0;
  bool hasRedirection = FALSE;
  while (cmd->argv[i] != 0){
	if (cmd->argv[i][0] == '<'){
		//printf("Found '<'.\n");
		RunCmdRedirIn(cmd, cmd->argv[i+1], stdinFd);
		hasRedirection = TRUE;
	}
	else if (cmd->argv[i][0] == '>'){
		//printf("Found '>'. Next: %s\n", cmd->argv[i+1]);
		RunCmdRedirOut(cmd, cmd->argv[i+1], stdoutFd);
		hasRedirection = TRUE;
	}
	i++;
  }
  if (hasRedirection){
	/*Updating argv*/
	i=0;
	while ((cmd->argv[i][0] != '>') && (cmd->argv[i][0] != '<')){
		i++;
	}
	/*Free arguments from index i and higher*/
	while (cmd->argv[i] != 0){
		free(cmd->argv[i]);
		cmd->argv[i] = 0;
		cmd->argc--;
		i++;
	}
  }
  return;
}
/*RedirectIO*/

/*
 * RunCmdRedirOut
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   char *file: the file to be used for standard output
 *
 * returns: none
 *
 * Runs a command, redirecting standard output to a file.
 */
void
RunCmdRedirOut(commandT* cmd, char* file, int* stdoutFd)
{
  fflush(stdout);
  *stdoutFd = dup(1);
  int fid = open(file, O_WRONLY | O_CREAT, 0755);
  close(1);
  dup(fid);
  close(fid);
  return;
} /* RunCmdRedirOut */


/*
 * RunCmdRedirIn
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   char *file: the file to be used for standard input
 *
 * returns: none
 *
 * Runs a command, redirecting a file to standard input.
 */
void
RunCmdRedirIn(commandT* cmd, char* file, int* stdinFd)
{
  //printf("file: %s\n", file);
  *stdinFd = dup(0);
  int fid = open(file, O_RDONLY);
  close(0);
  dup(fid);
  close(fid);
  return;
}  /* RunCmdRedirIn */


/*
 * RunExternalCmd
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   bool fork: whether to fork
 *
 * returns: none
 *
 * Tries to run an external command.
 */
static void
RunExternalCmd(commandT* cmd, bool fork, bool bground)
{
  char* path = (char*)malloc(sizeof(char)*PATHSIZE); 
  
  if(ResolveExternalCmd(cmd, path)){
      Exec(path, cmd, fork, bground);
  }

  free(path);
}  /* RunExternalCmd */


/*ResetIO
 *
 * arguments:
 *	int *stdinFd: File descriptor to close and set stdin to
 *	int *stdoutFd: File descriptor to close and set stdout to
 *
 * returns: none
 *
 * Resets IO to stdin and stdout*/

void
ResetIO(int* stdinFd, int* stdoutFd)
{
  if (*stdinFd != -1){
	close(*stdinFd);
	dup(0);
	close(0);
  }
  if (*stdoutFd != -1){
	close(*stdoutFd);
	dup(1);
	close(1);	
 }
 return;
}

/*ResetIO*/

/*
 * ResolveExternalCmd
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: bool: whether the given command exists
 *
 * Determines whether the command to be run actually exists.
 */
static bool
ResolveExternalCmd(commandT* cmd, char* path)
{
  //printf("Command: %s\n", cmd->name);
  //Check to see if in home directory:
  if(*(cmd->argv[0])=='.'){
    
    char* cwd = getCurrentWorkingDir();
    //char* cwd;
    //getCurrentWorkingDir(cwd);
    sprintf(path,"%s/%s",cwd,cmd->name);
    free(cwd);
    return TRUE;

  }

  char** memLocs = getPath();
 
  char dest[500];
  int i=0;
  struct stat buf;


 /*If already absolute path*/
 if(stat(cmd->name,&buf)==0){
   
   /*Set path = to entered absolute path*/
   strcpy(path,cmd->name);
   freePath(memLocs);
   return TRUE;

  }

  while(memLocs[i]!=NULL){
    //Concatanate Paths with cmd->name:
    int size = (snprintf( dest, 499,"%s/%s",memLocs[i],cmd->name)+1)*sizeof(char);
    char* exeName =  (char*)malloc(size);
    sprintf(exeName,"%s/%s",memLocs[i],cmd->name);
    //printf("%s/%s\n", memLocs[i], cmd->name); 
    //Check to see if exists and executable:
    if(stat(exeName,&buf)==0){
      if(S_ISREG(buf.st_mode) && buf.st_mode & 0111){
         
         strncpy(path,exeName,size);
         
         freePath(memLocs);
         free(exeName);
         return TRUE;
      }
       
      char* error = malloc(PATHSIZE * sizeof(char*));
      strcpy(error, "line 1: "); //since script reading is not in the functionality, it will be an error on line 1
      strcat(error, cmd->argv[0]);
      PrintPError(error);
      free(error);

      freePath(memLocs);
      free(exeName);
      return FALSE;
    } 
    
    i++;
    free(exeName);
  }
  
  char* error = malloc(PATHSIZE * sizeof(char*));
  strcpy(error, "line 1: "); //since script reading is not in the functionality, it will be an error on line 1
  strcat(error, cmd->argv[0]);
  PrintPError(error);
  free(error);

  freePath(memLocs);
  return FALSE;
} /* ResolveExternalCmd */


/*
 * Exec
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   bool forceFork: whether to fork
 *
 * returns: none
 *
 * Executes a command.
 */
static void
Exec(char* path, commandT* cmd, bool forceFork, bool bground)
{

   if (cmd->piped){
	RunCmdPipe(cmd);
   }
  else{
   pid_t child_pid;
   int childStat;
   sigset_t mask;

   
   sigemptyset(&mask);
   sigaddset(&mask, SIGCHLD);

   sigprocmask(SIG_BLOCK, &mask, NULL);
 
   child_pid=fork();

   /*Fork Worked?*/
   if(child_pid >= 0){
      /*If child process*/
      if(child_pid==0){
	int stdinFd = -1;
	int stdoutFd = -1;	
	sigprocmask(SIG_UNBLOCK, &mask, NULL);
  	RedirectIO(cmd, &stdinFd, &stdoutFd);
	if(bground){
	 cmd->argc--;
	 free(cmd->argv[cmd->argc]);
	 cmd->argv[cmd->argc] = 0; 
	}
	
        setpgid(0,0); 
	if(execv(path,cmd->argv)==-1)   
  	   printf("Error: %s\n", strerror(errno));
	 
	ResetIO(&stdinFd, &stdoutFd);
	fflush(stdout);
      }
      /*In Parent Process*/	
      else{
	crpid = child_pid;
	crName = stringFromArgv(cmd->argv, cmd->argc);
	if (!bground){
	  sigprocmask(SIG_UNBLOCK, &mask, NULL);
	  waitpid(crpid, &childStat, WUNTRACED);
	  crName = NULL;
	  crpid = 0;
	}
	else{
	 AddJob(crpid, stringFromArgv(cmd->argv, cmd->argc-1), "Running"); 
	 sigprocmask(SIG_UNBLOCK, &mask, NULL);
	}
        
      }
     } 
   }
} /* Exec */


/*
 * IsBuiltIn
 *
 * arguments:
 *   char *cmd: a command string (e.g. the first token of the command line)
 *
 * returns: bool: TRUE if the command string corresponds to a built-in
 *                command, else FALSE.
 *
 * Checks whether the given string corresponds to a supported built-in
 * command.
 */
static bool
IsBuiltIn(char* cmd)
{
  /*cd command*/
  /* "pwd" command*/ 
  if(!strcmp(cmd,"cd") || 
    !strcmp(cmd,"pwd") ||
    !strcmp(cmd,"jobs")||
    !strcmp(cmd,"fg")  ||
    !strcmp(cmd,"bg")  ||
    !strcmp(cmd,"alias")||
    !strcmp(cmd,"unalias")||
    !strcmp(cmd,"exit")
    ){
    return TRUE; 
  }
  else
     return FALSE;
} /* IsBuiltIn */


/*
 * RunBuiltInCmd
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: none
 *
 * Runs a built-in command.
 */
static void
RunBuiltInCmd(commandT* cmd)
{
  if(strcmp(cmd->argv[0],"cd")==0){
    changeWorkingDir(cmd->argv[1]); 
  }
  /* "pwd" command*/ 
  else if(strcmp(cmd->argv[0],"pwd")==0){
   char *dir = getCurrentWorkingDir();
   printf("%s\n",dir);   
   free(dir);
  }
  else if(!strcmp(cmd->argv[0],"jobs")){
   bgjobL* job = bgjobs;
   while (job != NULL){
    if (!strcmp(job->status, "Running")){
      printf("[%d]   %s                 %s &\n", job->jid, job->status, job->name);
    }
    else{
      printf("[%d]   %s                 %s\n", job->jid, job->status, job->name);
    }
    job = job->next;
  }
 }
  else if (!strcmp(cmd->argv[0],"fg")){
    bgjobL* job;
    if(cmd->argv[1] != NULL){
     job = GetJobFromJid(atoi(cmd->argv[1]));
    }
    else{
      job = GetMRJob();
    }
    
    //bring bg process to foreground
    if (job != NULL){
      tcsetpgrp(job->pid, STDIN_FILENO);
      crpid = job->pid;
      crName = job->name;
      DeleteJob(job->pid);
      kill(-crpid, SIGCONT);
      int status = 0;
      waitpid(crpid, &status, WUNTRACED);
      crpid = 0;
      crName = NULL;
    }
  }
  else if (!strcmp(cmd->argv[0],"bg")){
    bgjobL* job;
    if(cmd->argv[1] != NULL){
     job = GetJobFromJid(atoi(cmd->argv[1]));
     if (job != NULL){
      kill(job->pid, SIGCONT);
      job->status = "Running";
     }
    }
    else{
      job = GetMRJob();
      if (job != NULL){
	kill(job->pid, SIGCONT);
      job->status = "Running";
      }
    }
  }
  else if(!strcmp(cmd->argv[0],"alias")){
  
      addAlias(cmd);

  }
  else if(!strcmp(cmd->argv[0],"unalias")){
  
      deleteAlias(cmd->argv[1]);

  }
   else if(!strcmp(cmd->argv[0],"exit")){
  
      return;

  }
} /* RunBuiltInCmd */


/*
 * CheckJobs
 *
 * arguments: none
 *
 * returns: none
 *
 * Checks the status of running jobs.
 */
void
CheckJobs()
{
  bgjobL* job = bgjobs;
  int wait_settings;
  wait_settings = WNOHANG | WUNTRACED | WCONTINUED;
  while (job != NULL){
    int status;
    pid_t pid = waitpid(-1, &status, wait_settings);
    if (pid > 0){
      if (pid == job->pid){
	if (WIFCONTINUED(status)){
	  job->status = "Running";
	}
	else{
	  job->status = "Done";
	  printf("[%d]   %s                 %s\n", job->jid, job->status, job->name);
	  DeleteJob(job->pid);
	}
      }
    }
    job = job->next;
  }
} /* CheckJobs */



/*MakeJobs

*/
bgjobL* 
MakeJob(pid_t pid, char* name, char* status){
 //printf("making job with name: %s\n", name);
 bgjobL* newjob = (bgjobL*)malloc(sizeof(bgjobL));
 newjob->pid = pid;
 newjob->name = name;
 newjob->status = status;
 newjob->next = NULL;
 newjob->jid = GetNewJid();
 
 //printf("made job\n");
 return newjob;
}//MakeJob



/*AddJob
*
*
*
*/
void 
AddJob(pid_t pid, char* name, char* status){
 bgjobL* current = bgjobs;
 //printf("added job\n");
 if (current == NULL){
   bgjobs = MakeJob(pid, name, status);
   return;
 }
 else{
 
  while (current->next != NULL){
    current = current->next; 
  }
 
  current->next = MakeJob(pid, name, status);
  return;
 }
  
}//AddJob


/*
*DeleteJobs
*
*
*
*/
void 
DeleteJob(pid_t pid){
 bgjobL* current = bgjobs;
 bgjobL* prev = current;
 if (current == NULL){
   return;
 }
 if (current->pid == pid){
   bgjobs = current->next;
   free(current);
   return;
 }
 
 while (current->next != NULL){
  if (current->pid == pid){
    continue;
  }
  prev = current;
  current = current->next;
 }
 
 prev->next = current->next;
 free(current->name);
 free(current);
}//DeleteJob



/*GetJob
*
*
*
*/
bgjobL* 
GetJob(pid_t pid){
 bgjobL* job = bgjobs;
 
 while (job != NULL){
  if (job->pid == pid){
   break; 
  }
  job = job->next;
 }
 return job;
}//GetJob



/*
*GetMRJob
*
*Gets most recently added background job.
*
*
*/

bgjobL* 
GetMRJob(){
  bgjobL* job = bgjobs;
  
  while (job->next != NULL)
    job = job->next;
  
  return job;
}//GetMRJob

/*GetJobJid
*Gets a job based on its jid
*
*/

bgjobL* 
GetJobFromJid(int jid){
 bgjobL* job = bgjobs;
 
 while (job != NULL){
  if (job->jid == jid){
   break; 
  }
  job = job->next;
 }
 return job;
}//GetJob

/*
 * getCurrentWorkingDir (NO ARGS)
 *
 * arguments: none
 *
 * returns: outputs current directory
 *
 * Print Working Directory.
 */
char*
getCurrentWorkingDir(){
  
   long size;
   char *buf;
   char *ptr =0;

   size = pathconf(".", _PC_PATH_MAX);

   if((buf = (char*)malloc((size_t)size)) != NULL)
      ptr = getcwd(buf,(size_t)size);
   
   return ptr;
}/* getCurrentWorkingDir*/


/*
 * changeWorkingDir
 *
 * arguments: none
 *
 * returns: outputs current directory
 *
 * Print Working Directory.
 */
void 
changeWorkingDir(char* path){
   
   /*Change directory to given path and set environment "PWD" variable to match*/

   if(path==NULL){
     char* home = getenv("HOME");
     chdir(home);
   }
   else
     chdir(path);
	
  //if(chdir(path)!=0)
  //   printf("Error: %s\n", strerror(errno));

   char *cwd = getCurrentWorkingDir();
   setenv("PWD", cwd, 1);
   free(cwd);
 
      
}/*changeWorkingDir*/

/*
 * getPath
 *
 * arguments: none
 *
 * returns: Path  
 *
 * Search directories according to PATH.
 */
char** 
getPath(){
   
   /*Get copy of pointer to "PATH" string.*/
   char* mPath;
   mPath=strdup(getenv("PATH"));
   
   /*Make pointer pointing to the string pointer*/
   char* ptr = mPath;
   
   /*Create a char* [] dynamically*/
   char** subPath = (char**)malloc(sizeof(char*) * PATHSIZE); 
   
   /*Index for token loop*/
   int i =0;
  
   /****************strsep method*********************/
   //char* tempx = temp;

   //while(subPath[i] =strsep(&mainPath,":")){
      
      //subPath[i] = (char*)malloc(sizeof(char)*100);
      //strcpy(subPath[i],temp2);
      //printf("%s\n",subPath[i]); 
     // i++; 
   //}

   /**************strtok method**********************/

   subPath[i] = strtok_r(mPath,":",&mPath);

   i=1;
   while((subPath[i]=strtok_r(NULL,":",&mPath)) != NULL){
    
       i++;
   }
 
   /*Free ptr to string and return subPath. To be freed later*/
   free(ptr);
   return subPath;

}

void 
freePath(char** path){
  
  free(path);

}

/* IsBG
*  Checks if a program is asked to be run in the background
*/
static bool
IsBG(commandT* cmd){
  
   if(strcmp(cmd->argv[(cmd->argc)-1], "&")==0){
      //printf("yes\n");      
      return TRUE;
   }
   //printf("no\n");
   return FALSE;
}


/* GetNewJid
*   Makes a new Jid for a new job that is just being created
*
*/
int 
GetNewJid(){
 bgjobL* job = bgjobs;
 
 if (job == NULL){
  return 1; 
 }
 else{
  job = GetMRJob();
  return job->jid+1;
 }
}


char* stringFromArgv(char** argv, int last){
 char* string =malloc(PATHSIZE);
 int i;
  
 strcpy(string, argv[0]);
 

 for (i = 1; i < last && argv[i] != NULL; i++){
  strcat(string, " ");
  int j;
  char* quote = "";
  for (j = 0; argv[i][j] != 0; j++){
    if (argv[i][j] == ' ') {
      quote = "\"";
      break;
    }
    else{
      quote = "";
    }
  }
  
  strcat(string, quote);
  strcat(string, argv[i]);
  strcat(string, quote);
 }
 return string;
}


/*add Alias*/
aliasL* 
makeAlias(commandT* cmd){

   aliasL* alias = malloc(sizeof(aliasL) +1);
   char* line = cmd->argv[1];

  //Parse Line:
  int i;
   
  char* als = malloc(sizeof(char*) * (PATHSIZE/2));
  als[0] = 0;

  char* orig = malloc(sizeof(char*) * (PATHSIZE/2));
  orig[0] = 0;
  int whichArg = 0;
  int j = 0;
  /*TODO watch for error*/ for(i=0; line[i] != 0; i++){
      
      //If = is found, pop. other string      
      if(line[i] == '='){
            
            als[i]='\0';            
            whichArg=1;
            i++;
      }
      
      //In als sting?
      if(whichArg==0){
         
         als[i] = line[i];
      }      
      else{
      
         orig[j] = line[i];
         j++;       

      }
   }
   
   orig[j] ='\0';
  
   //printf("The alias is: %s\n", als);    
   //printf("The orig  is: %s\n", orig);

   //Populate alias struct:
   alias->alias =strdup(als);
   alias->origName =strdup(orig);
   
   //Do I need this?
   alias->next = NULL;
   alias->found=FALSE;
   free(als);
   free(orig); 
   
   return alias;
   //printf("The alias is: %s\n", alias->alias);    
   //printf("The orig  is: %s\n", alias->origName);
}


/*addAlias
*
*
*
*/
void 
addAlias(commandT* cmd){

 if(cmd->argc <=1){
   printAliasList();
   return;
 }


 aliasL* current = aliasList;
 //printf("added job\n");
 if (current == NULL){
   aliasList = makeAlias(cmd);
   //printf("aliasList->alias is %s\n", aliasList->alias);
   //printAliasList();
   return;
 }

 else{
 
  while (current->next != NULL){
    current = current->next; 
  }
 
  current->next = makeAlias(cmd);
  //printAliasList();
  return;
 }
  
 //printAliasList();
}//addAlias

/*Delete from the Alias linked list*/

void 
deleteAlias(char* als){
 aliasL* current = aliasList;
 aliasL* prev = current;
 if (current == NULL){
    /*Not found print error message*/
   char* error = malloc(PATHSIZE * sizeof(char*));
   strcpy(error, "tsh: ");
   strcat(error, "line 1: unalias: "); //since script reading is not in the functionality, it will be an error on line 1
   strcat(error, als);
   strcat(error, ": not found");//PrintPError(error);
   printf("%s\n", error);
   free(error);
   return;
 }
 if (!strcmp(current->alias, als)){
   aliasList = current->next;
   free(current);
   return;
 }
 
 while(current->next!=NULL){
      
      if(!strcmp(current->alias, als)){
         prev->next = current->next;
         free(current->alias);
         free(current->origName);         
         free(current);
      }
      prev=current;
      current=current->next;         

  }
  /*Not found print error message*/
  char* error = malloc(PATHSIZE * sizeof(char*));
  strcpy(error, "tsh: ");
  strcat(error, "line 1: "); //since script reading is not in the functionality, it will be an error on line 1
  strcat(error, als);
  strcat(error, ": not found");//PrintPError(error);
  printf("%s\n", error);
  free(error);
}//deleteAlias

void
printAliasList(){
   
   aliasL* current = aliasList;

   if(current == NULL){
      //printf("List Null\n");
      return;
   }

   printf("alias %s=\'%s\'\n",current->alias,current->origName);

   while(current->next != NULL){
   
      current = current->next;
      printf("alias %s=\'%s\'\n",current->alias,current->origName);    

   }


}/*printAliasList*/

bool
isAlias(char* cmdName){
   
   aliasL* current = aliasList;

   if(current == NULL){
      return FALSE;
   }

   if(!strcmp(current->alias, cmdName))
      //printf("alias->origName is %s\n", current->origName);
      return TRUE;

   while(current->next != NULL){
   
      current = current->next;
      if(!strcmp(current->alias, cmdName))
         return TRUE;
   }
   
   return FALSE;
}


aliasL*
getAlias(char* cmdName){
   
   aliasL* current = aliasList;

   if(current == NULL){
      return NULL;
   }

   if(!strcmp(current->alias, cmdName))
      //printf("alias->origName is %s\n", current->origName);
      return current;

   while(current->next != NULL){
   
      current = current->next;
      if(!strcmp(current->alias, cmdName))
         return current;
   }
   
   return NULL;
}

