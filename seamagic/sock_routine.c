#include "seamagic.h"

int sockfd[MAX_THREADS_COUNT];

void
sock_cleanup(void *fd)
{
  close(*(int*) fd);
}

void*
sock_routine(void *n)
{
  struct sockaddr_un  peer, sa = { .sun_family = AF_UNIX };
  socklen_t           addrlen;

  RECORD              *rp, rec = { .present = 0 };
  const size_t        msglen = sizeof rec.pkey + sizeof rec.sig + sizeof rec.id;
  const unsigned      mask = (1 << SEL_BIT_LENGTH % 8) - 1;
  const unsigned      rib = BUCKETSIZE / sizeof rec;

  pthread_cleanup_push(sock_cleanup, &sockfd[*(int*) n]);

  if (snprintf(sa.sun_path, 108, "%s%d", sock_prefix, *(int*) n) >= 108) errx(1,

                             "%s%d is longer than 108", sock_prefix, *(int*) n);

  prctl(PR_SET_NAME, "sock_routine");

  unlink(sa.sun_path);

  if ( (sockfd[*(int*) n] = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) err(1, "socket");

  if (bind(sockfd[*(int*) n], (struct sockaddr*) &sa, sizeof sa) < 0) err(1, "bind");

  if (chown(sa.sun_path, SOCK_OWNER_UID, -1) < 0) err(1, "chown");

  for ( ;; )
  {
    int     i;
    size_t  len;

    addrlen = sizeof peer;
    if ( (len = recvfrom(sockfd[*(int*) n], &rec, msglen, MSG_TRUNC,

                          (struct sockaddr*) &peer, &addrlen)) < 0) err(1, "recvfrom");

    if (len != msglen)
    {
      printf("Strange payload of %zu bytes: ignoring\n", len);
      continue;
    }

    len = rec.sig[SIG_FROM1_BYTE - 1]; // from 0

    if (mask) len &= mask;
    //printf("%zu ", len);

    for (i = 0; i < (SEL_BIT_LENGTH - 1) / 8; i++)
    {
      len *= 0x100;
      len += rec.sig[SIG_FROM1_BYTE + i];
      //printf("%hhu ", rec.sig[SIG_FROM1_BYTE + i]);
    }
    //printf("\nGot selector %zu\n", len);

    rp = (RECORD*) (htb_map + len * BUCKETSIZE);

    for (i = 0; i < rib; i++)
    {
      if (rp->present)
      {
        if (memcmp(rp->sig, rec.sig, sizeof rec.sig) == 0)
        {
          /* id exists: fail this request */
          if (sendto(sockfd[*(int*) n], "0", 0, 0,
                
                      (struct sockaddr*) &peer, addrlen) < 0) err(1, "sendto(0)");
          goto bye;
        }
      }
      else
      {
        memcpy(rp, &rec, sizeof rec);
        rp->present = 1;

        break;
      }
      rp++;
    }
    if (unlikely(i >= rib))
    {
      /* fall back on overflow file waiving uniqueness */
      if (write(offd, &rec, sizeof rec) < 0) err(1, "append");
    }
    if (sendto(sockfd[*(int*) n], "1", 1, 0,

                (struct sockaddr*) &peer, addrlen) < 0) err(1, "sendto(1)");

bye: ;
  }

  pthread_cleanup_pop(1);
  return NULL;
}
