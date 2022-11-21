#include "seamagic.h"

int     tmpfd, htbfd;
void    *offmap;
size_t  offlen;

void
log_cleanup(void*)
{
  close(tmpfd);
  close(htbfd);
  munmap(offmap, offlen);
}

static inline void
dumprec(RECORD *rp)
{
  char    *nxt, b64line[sizeof (RECORD) * 3]; // ballpark
  size_t  len;

  nxt = b64line;
  len = sizeof b64line;

  sodium_bin2base64(nxt, len, rp->pkey, sizeof rp->pkey,
                        sodium_base64_VARIANT_ORIGINAL);

  nxt += sodium_base64_ENCODED_LEN(sizeof rp->pkey,
                        sodium_base64_VARIANT_ORIGINAL);

  len -= sodium_base64_ENCODED_LEN(sizeof rp->pkey,
                        sodium_base64_VARIANT_ORIGINAL);

  *(nxt - 1) = ' ';

  sodium_bin2base64(nxt, len, rp->sig, sizeof rp->sig,
                        sodium_base64_VARIANT_ORIGINAL);

  nxt += sodium_base64_ENCODED_LEN(sizeof rp->sig,
                        sodium_base64_VARIANT_ORIGINAL);

  len -= sodium_base64_ENCODED_LEN(sizeof rp->sig,
                        sodium_base64_VARIANT_ORIGINAL);

  *(nxt - 1) = ' ';

  sodium_bin2base64(nxt, len, rp->id, sizeof rp->id,
                        sodium_base64_VARIANT_ORIGINAL);

  nxt += sodium_base64_ENCODED_LEN(sizeof rp->id,
                        sodium_base64_VARIANT_ORIGINAL);

  len -= sodium_base64_ENCODED_LEN(sizeof rp->id,
                        sodium_base64_VARIANT_ORIGINAL);

  len = sizeof b64line - len;

  *(nxt - 1) = '\n';

  if (write(tmpfd, b64line, len) < 0) err(1, "write(tmp)");
}

void*
log_routine(void*)
{
  const struct sched_param  param = { .sched_priority = 0 };

  struct timespec           tick, tock, req = { .tv_sec = THROTTLESEC };

  size_t                    ofs;
  RECORD                    *rp;

  pthread_cleanup_push(log_cleanup, NULL);

  if ( (errno = pthread_setschedparam(pthread_self(), SCHED_IDLE, &param))
     
                                    != 0) err(1, "pthread_setschedparam");

  if (clock_gettime(CLOCK_MONOTONIC, &tock) < 0) err(1, "clock_gettime");

  prctl(PR_SET_NAME, "log_routine");

  if ( (htbfd = open(htbpath, O_RDONLY)) < 0) err(1, "open(htb-rdonly)");

  for ( ;; )
  {
    tick.tv_sec = tock.tv_sec;

    if ( (tmpfd = creat(tmppath, 0644)) < 0) err(1, "creat");

    if ( (ofs = lseek(htbfd, 0, SEEK_DATA)) < 0) err(1, "lseek data");

    for ( ; ofs < HTBSIZE; ofs = lseek(htbfd, ofs, SEEK_DATA))
    {
      for (rp = (RECORD*) (htb + ofs); rp->present; rp++)
      {
        /* append app-specific format to tmpfd */
        dumprec(rp);

        ofs += sizeof (RECORD);
      }
      ofs = (ofs / BUCKETSIZE + 1) * BUCKETSIZE;
    }
    offlen = lseek(offd, 0, SEEK_CUR);
    if ( (offmap = mmap(NULL, offlen, PROT_READ, MAP_SHARED, offd, 0)) < 0) err(1, "mmap(off)");

    for (rp = offmap; (void*) rp < offmap + offlen; rp++) dumprec(rp);
    
    munmap(offmap, offlen);
    close(tmpfd);

    if (rename(tmppath, logpath) < 0) err(1, "rename");

    clock_gettime(CLOCK_MONOTONIC, &tock);
    if (tock.tv_sec - tick.tv_sec < THROTTLESEC) nanosleep(&req, NULL);
  }

  pthread_cleanup_pop(1);
  return NULL;
}
