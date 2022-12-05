#include "seamagic.h"

int     tmpfd;
void    *offmap;
size_t  offlen;

void
log_cleanup(void *dummy)
{
  close(tmpfd);
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
log_routine(void *dummy)
{
  const struct sched_param  param = { .sched_priority = 0 };

  struct timespec           tick, tock;

  size_t                    ofs;
  RECORD                    *rp;

  pthread_cleanup_push(log_cleanup, NULL);

  if ( (errno = pthread_setschedparam(pthread_self(), SCHED_IDLE, &param))
     
                                    != 0) err(1, "pthread_setschedparam");

  prctl(PR_SET_NAME, "log_routine");

  for ( ;; )
  {
    clock_gettime(CLOCK_MONOTONIC, &tick);

    if ( (tmpfd = creat(tmppath, 0644)) < 0) err(1, "creat");

    if ( (ofs = lseek(htbd, 0, SEEK_DATA)) < 0) err(1, "lseek data");

    for ( ; ofs < HTBSIZE; ofs = lseek(htbd, ofs, SEEK_DATA))
    {
      pthread_testcancel();

      for (rp = (RECORD*) (htb_map + ofs); rp->present; rp++)
      {
        /* append app-specific format to tmpfd */
        dumprec(rp);

        ofs += sizeof (RECORD);
      }
      ofs = (ofs / BUCKETSIZE + 1) * BUCKETSIZE;
    }
    offlen = lseek(offd, 0, SEEK_END);

    if ( (offmap = mmap(NULL, offlen, PROT_READ, MAP_SHARED, offd, 0))
        
                   != MAP_FAILED)

        for (rp = offmap; (void*) rp < offmap + offlen; rp++)
                     
            dumprec(rp);
    
    munmap(offmap, offlen);

    if (close(tmpfd) < 0) err(1, "close(tmp)");

    if (rename(tmppath, logpath) < 0) err(1, "rename");

    clock_gettime(CLOCK_MONOTONIC, &tock);

    if ( (tock.tv_sec -= tick.tv_sec) < THROTTLESEC)
    {
      tick.tv_sec = THROTTLESEC - tock.tv_sec;
      nanosleep(&tick, NULL);
    }
  }

  pthread_cleanup_pop(1);
  return NULL;
}
