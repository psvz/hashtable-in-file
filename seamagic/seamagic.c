#include "seamagic.h"

int       offd, num[MAX_THREADS_COUNT];
pthread_t       tid[MAX_THREADS_COUNT];

char      *htb, htbpath[PATH_SIZE],
                tmppath[PATH_SIZE],
                offpath[PATH_SIZE],
                logpath[PATH_SIZE],
                sock_prefix[PATH_SIZE];

void
fireup(int nsock)
{
  int htbd;

  if ( (offd = open(offpath, O_WRONLY | O_CREAT | O_APPEND, 0)) < 0) err(1, "off open");

  if (lockf(offd, F_TLOCK, 0) < 0) err(1, "lockf");

  if ( (htbd = open(htbpath, O_RDWR | O_CREAT, 0)) < 0) err(1, "htb open");

  if (lseek(htbd, HTBSIZE, SEEK_SET) < 0) err(1, "htb lseek");

  if (read(htbd, &htb, 1) == 0) if (write(htbd, "q", 1) < 0) err(1, "htb write");

  if ( (htb = mmap(NULL, HTBSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, htbd, 0)) < 0) err(1, "mmap");

  close(htbd);

  for (int i = 0; i < nsock; i++)
  {
    num[i] = i;
    if ( (errno = pthread_create(&tid[i], NULL, sock_routine, &num[i])) != 0) err(1, "pthread_create(sock)");
  }
  if ( (errno = pthread_create(&tid[nsock], NULL, log_routine, NULL)) != 0) err(1, "pthread_create(log)");
}

int
main(int argc, char **argv)
{
  int       nsock;
  sigset_t  set;

  if (argc < 3) errx(1, "(usage) %s <number of workers> <filename base>\n"
                        "Files .htb, .off, .tmp, and .log will be based on\n"
                        LOG_PATH"<filename base>\nSockets will be bound as\n"
                        SOCK_PATH"<filename base>.N: [0; <number of workers>)\n"
                        , argv[0]);

  if (snprintf(htbpath, PATH_SIZE, LOG_PATH"%s.htb", argv[2]) >= PATH_SIZE) errx(1,

                                            LOG_PATH"%s.ext is too long", argv[2]);

  snprintf(offpath, PATH_SIZE, LOG_PATH"%s.off", argv[2]);
  snprintf(tmppath, PATH_SIZE, LOG_PATH"%s.tmp", argv[2]);
  snprintf(logpath, PATH_SIZE, LOG_PATH"%s.log", argv[2]);

  if (snprintf(sock_prefix, PATH_SIZE, SOCK_PATH"%s.", argv[2]) >= PATH_SIZE) errx(1,

                                                SOCK_PATH"%s. is too long", argv[2]);

  if ( (nsock = atoi(argv[1])) < 1 || nsock > MAX_THREADS_COUNT - 2) errx(1,

                                       "inappropriate <number of workers>");

  if (BUCKETSIZE % sysconf(_SC_PAGE_SIZE) != 0) errx(1, "bucket needs be page-aligned");

  setbuf(stdout, NULL);

  sigemptyset(&set);

  sigaddset(&set, SIGHUP);  // syncing data
  sigaddset(&set, SIGUSR1); // purging data

  if ( (errno = pthread_sigmask(SIG_BLOCK, &set, NULL)) != 0) err(1, "pthread_sigmask"); 

  for ( ;; )
  {
    int i;

    fireup(nsock);

    printf("\nReady\n");

    sigwait(&set, &i); switch (i)
    {
      case SIGHUP:

          printf("Syncing data ");

          if (fsync(offd) < 0) err(1, "fsync");
          if (msync(htb, HTBSIZE, MS_SYNC) < 0) err(1, "msync(sync)");

          break;

      case SIGUSR1:

          printf("Purging data ");

          for (i = 0; tid[i]; i++) pthread_cancel(tid[i]);
          for (i = 0; tid[i]; i++, tid[i] = 0) pthread_join(tid[i], NULL);

          if (munmap(htb, HTBSIZE) < 0) err(1, "munmap");
          if (unlink(htbpath) < 0) err(1, "unlink(htb)");

          if (close(offd) < 0) err(1, "close(offd)");
          if (unlink(offpath) < 0) err(1, "unlink(off)");

          break;
    }
  }

  return 0;
}
