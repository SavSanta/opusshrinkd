/*
 * opusshrinkd.c
 * Daemon made using StackO
 * https://stackoverflow.com/questions/17954432/creating-a-daemon-in-linux/17955149#17955149
 * Forked skeleton: https://github.com/pasce/daemon-skeleton-linux-c
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <string.h>
#include <dirent.h>
#include <libgen.h>
#include <time.h>

/* Version */
#define PROG_VER "0.8.0"

/* Terminal colored output via escape codes */
#define NORMAL_COLOR  "\x1B[0m"
#define GREEN  "\x1B[32m"
#define RED  "\x1B[33m"

/* Configurable ifdefines */
#define MAXFILES 2500		// decrease from 10000 to stop SIGSEVs
#define FILELEN 600
#define BASEPATH "/home/acr/voicecalls"
#define SAVEPATH "/home/acr/opusvoicecalls"
#define TRASHPATH "/home/acr/trashvoicecalls"
#define BITRATE "10k"
#define ENDSUFFIX ".opus"


// Global Vars
/* Create a 200 file character ptr array to hold strings of 128 chars max each */
char *filelist[MAXFILES][128] = { '\0' };
/* Create global time objects */
time_t current_t, trigger_t;


    static void opus_shrink_daemon ()
    {

      /* Process below will cause the fork off into a daemon */
      pid_t pid;

      /* Fork off the parent process */
      pid = fork();
      /* If pid is less than zero after fork, then an error occurred. Exit */
      if (pid < 0)
        exit(EXIT_FAILURE);
      /* Success: Let the parent terminate */
      if (pid > 0)
        exit(EXIT_SUCCESS);
      /* On success: The child process becomes session leader */
      if (setsid() < 0)
        exit(EXIT_FAILURE);

      /* Catch signal to silently (and portably) reap children so they dont zombify */
      signal(SIGCHLD, SIG_IGN);
        
      /* Catch signal to remove lockfile and terminate gracefully */ 
      signal(SIGHUP, SIG_IGN);

      /* Fork off for the second time */
      pid = fork();

      /* An error occurred if pid is less than zero */
      if (pid < 0)
        exit(EXIT_FAILURE);

      /* Success: Let the parent terminate */
      if (pid > 0)
        exit(EXIT_SUCCESS);

      /* Set new file permissions to 755 */
      umask(022);

      /* Change the working directory to the root directory */
      /* or another appropriated directory */
      chdir (SAVEPATH);

      /* Close all open file descriptors */
      int x;
      for (x = sysconf(_SC_OPEN_MAX); x >= 0; x--)
        {
          close(x);
        }

      /* Open the log file */
      openlog("opusshrink", LOG_PID, LOG_DAEMON);
    }

    void checkrunning(void)
    {
        FILE *file;
        
        if ((file = fopen("/var/lock/opusshrinkd.lock", "r")))
        {
            syslog(LOG_ERR, "Error! Opusshrinkd daemon lockfile (/var/lock/opusshrinkd.lock) indicates already. Exiting.");
            fclose(file);
            exit(-2);
        }
        else
        {
           syslog(LOG_NOTICE, "No lockfile found. New instance to launch.");
           file = fopen("/var/lock/opusshrinkd.lock", "w");
           fclose(file);
           return; 
        }

    }
    

    void checkfilesize(char filename1[], char filename2[])
    {
      // Function to check file sizes. Generally speaking the opus files should be within 50-75% of the original
      // If not alert to check everyone. 
      // Alternatively can just figure a method to check duration time of each
    }

    void sigtermcleanup(void) 
    {
        int status;
        status = remove("/var/lock/opusshrinkd.lock");
        
        // Logic to check if loop stopped or manually stop loop if in middle of process

        if (status == 0)
        {
            syslog(LOG_NOTICE, "The lockfile was deleted.\n");
            closelog();
            syslog(LOG_NOTICE, "Opus Shrink terminated successfully.");
            exit(0);
        }            
        else
        {
            syslog(LOG_NOTICE, "Unable to delete the lockfile. Exiting with error.\n");
            closelog();
            syslog(LOG_NOTICE, "Opus Shrink terminated with an error.");
            exit(-3); 
        }
    }
    
    
    
    void updatetrigger(void)
    {
        /* Initialize start time to current time */
        current_t = time(NULL); 
        
        /* Adds about two hours (in seconds) to create the next trigger time */
        trigger_t = (long int) current_t + 5400;
    }

    void fileconvert(void)
    {
       int count;
       
       for (count = 0 ; count < MAXFILES ; count++)
        if (strlen(filelist[count]) > 0)
           {
              // derive basename and create a new destination filename with opus suffix
              int err;
              char * bname;
              char outfile[200] = {'\0'};
              strcat(outfile, SAVEPATH);
              strcat(outfile, "/");
              bname = basename(filelist[count]);
              strcat(outfile, bname);
              strcat(outfile, ENDSUFFIX);


              /* Forking protocol done here. Neccessary to get return child exit status cod */
              
              int pid = fork();
              
              if (pid == 0)
              {
              // Call out to ffmpeg to do the conversion
              char *argv[10] = { "ffmpeg", "-hide_banner", "-loglevel", "fatal", "-i", filelist[count], "-b:a", BITRATE, outfile, 0 };
              err = execv("/usr/bin/ffmpeg", argv);
              
              // This will only be reached if we got an error in the child call.
              char errbuff[200];
              sprintf(errbuff, "Error! Could not execute FFmpeg for file %s. Exiting with %i", filelist[count], err);
              syslog(LOG_ERR, errbuff);
              exit(err); 
              
              }
              else if(pid < 0) 
              {
                syslog(LOG_ERR, "Forking failed with error code %d\n.", pid);
                exit(-1);
              }

              // wait for child status
              int status;
              wait(&status); 
              
              // Check exit code and delete file if safe
              if ( status == 0 )
              {
                char trash[200] = TRASHPATH;
                strcat(trash, "/");
                strcat(trash, bname);
                rename(filelist[count], trash);

                // Set the string of the path to null and log file to syslog
                filelist[count][0] = '\0';
                char * msgbuff[200];
                sprintf(msgbuff, "NULLed out %s from filelist.", outfile);
                syslog(LOG_NOTICE, msgbuff);
              }
              else
              {
                // Create a new error message buffer and special format values before sending to syslog
                char * errbuff[200];
                sprintf(errbuff, "FFmpeg Transcode Error! PID %i - Exit Code: %i on -> %s", pid, status, outfile);
                syslog(LOG_ERR, errbuff); 
              }
           }
        else {
            
            // skip for now but maybe in future write to log file
            
            }
       
        
    }

    void updatefiles(void)
    {
      /* Standard declarations */
      DIR *d;
      struct dirent *dir;

      /* declare and initialize a counter variable for files in directory */
      int count = 0;

      /* Debug Testing Omit This code in production
         printf("\nEnter the directory you want to list out!");
         scanf("%s", &path);
       */

      /* Set the path of the d variable */
      d = opendir(BASEPATH);

      /* Create a full path variable */
      char full_path[FILELEN];

      // Check if value isnt a negative..ie a fail
      if (d)
        {
          // Call readdir to read the next file in the dirent directory and assign it to dir.
          while ((dir = readdir (d)) != NULL)
        {
          // Check if dirent is valued as a regular file. Only expected to work on unix per https://linux.die.net/man/3/readdir
          if (dir->d_type == DT_REG)
            {

              // Following section builds the full path to the files in the location incrementally using `strcat`
              full_path[0] = '\0';
              strcat(full_path, BASEPATH);
              strcat(full_path, "/");
              strcat(full_path, dir->d_name);

              // Copy the full path to the filelist array
              strcpy(filelist[count], full_path);

              count++;
            }
        }
          closedir(d);
        }
      else
        {
          printf("Error occured opening the directory! Exiting");
          syslog(LOG_ERR, "Error occured opening the directory! Exiting with failure");
          exit(EXIT_FAILURE);
        }
        
      return;

    }
    
    int main()
    {
      checkrunning();
      opus_shrink_daemon();
      syslog(LOG_NOTICE, "Opus Shrink has started.");

      // Set variable to current file directory contents
      updatefiles();
      syslog(LOG_NOTICE, "Opus Shrink completed initial listing of directory generation.");

      while (1)  //mainloop
        {
          
          if ( time(NULL) > trigger_t)
            {
              updatefiles();
              updatetrigger();
              syslog(LOG_NOTICE, "Opus Shrink updated filelisting and timestamp after 5400 seconds.");
              sleep(25);
            }
          else
            {
              fileconvert();
              syslog(LOG_NOTICE, "Fileconvert iteration complete. Sleeping 500 secs.");
              sleep(500);          // Aid to decrease polling    
            }

        }

      return EXIT_FAILURE;      //realistically this should never be reached since we're in a forever while loop
    }
    
