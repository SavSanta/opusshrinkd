/*
 * opusshrinkd.c
 * Making a daemon using question below
 * https://stackoverflow.com/questions/17954432/creating-a-daemon-in-linux/17955149#17955149
 * Fork this code: https://github.com/pasce/daemon-skeleton-linux-c
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
#include <time.h>


/* Terminal colored output via escape codes */
#define NORMAL_COLOR  "\x1B[0m"
#define GREEN  "\x1B[32m"
#define RED  "\x1B[33m"
#define MAXFILES 3000		// decrease from 10000 to stop SIGSEVs
#define FILELEN 1000
#define BASEPATH "/root/voicecalls"
#define SAVEPATH "/root/opusvoicecalls"
#define ENDSUFFIX ".opus"


// Global Vars
/* Create a 200 file character ptr array to hold strings of 128 chars max each */
char *filelist[MAXFILES][128] = { '\0' };
/* Create time objects */
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

      /* Catch, ignore and handle signals */
      //TODO: Implement a working signal handler */
      signal(SIGCHLD, SIG_IGN);
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

    int main()
    {
      opus_shrink_daemon();
      syslog(LOG_NOTICE, "Opus Shrink has started.");
      
      // Set variable to current file directory contents
      updatefiles();
      syslog(LOG_NOTICE, "Opus Shrink completed initial listing of directory generation.");

      while (1)  //mainloop
        {
          
          if (current_t > trigger_t)
            {
              updatefiles();
              updatetrigger();
              syslog(LOG_NOTICE, "Opus Shrink updated listing and timestamp after 7200 seconds.");
              sleep(10);
            }
          else
            {
              fileconvert();
              sleep(500);          // Aid to decrease polling    
            }

        }

      syslog(LOG_NOTICE, "Opus Shrink has terminated.");
      closelog();

      return EXIT_SUCCESS;
    }

    void updatetrigger(void)
    {
        
        /* Initialize start time to current time */
        current_t = time(NULL); 
        
        /* Adds a two hours in seconds to the trigger time */
        trigger_t = (long int) current_t + 7200;

    }

    void fileconvert(void)
    {
       int count;
       
       for (count = 0 ; count < MAXFILES ; count++)
        if (strlen(filelist[count]) > 0)
           {
              // derive basename and create a new destination filename with opus suffix
              int err;
              char *bname;
              char outfile[200] = {'\0'};
              strcat(outfile, SAVEPATH);
              strcat(outfile, "/");
              bname = basename(filelist[count]);
              strcat(outfile, bname);
              strcat(outfile, ENDSUFFIX);     
              
              // Call out to ffmpeg to do the conversion
              char *env[1] = { 0 };
              char *argv[7] = { "ffmpeg", "-i", filelist[count], "-b:a", "20k", outfile, 0 };
              err = execve("/usr/bin/ffmpeg", argv, env);
              
              
              // Check exit code and delete file if safe. Otherwise log to syslog.
              if ( err = 0 )
              {
                //delete file
              }
              else
              {
                char * errbuff[200];
                sprintf(errbuff, "FFmpeg Transcode Error! Exit Code: %i on %s-> %s", err, outfile);
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
          // call readdir to read the next file in the dirent directory and assign it to dir.
          while ((dir = readdir (d)) != NULL)
        {
          //Check if dirent is valued as a regular file. Only expected to work on unix per https://linux.die.net/man/3/readdir
          if (dir->d_type == DT_REG)
            {

              // following sections builds the full path to the files in the location incrementally using `strcat`
              full_path[0] = '\0';
              strcat(full_path, BASEPATH);
              strcat(full_path, "/");
              strcat(full_path, dir->d_name);

              //Copy the full path to the filelist array
              strcpy (filelist[count], full_path);
              //printf("%s\n", filelist[count]);

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
