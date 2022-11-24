/*
 * Copyright (c) 2022 Vitaly Zuevsky <vx@claw.ac>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/

#include <err.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdint.h>
#include <string.h>

/* Base64 codec from libsodium */
#include <sodium/utils.h>

#define SOCK_OWNER_UID    65534

#define PATH_SIZE         128
#define MAX_THREADS_COUNT 128

#define SOCK_PATH         "/run/"
#define LOG_PATH          "/www/nginx/logs/"

/* bucket selector is a big-endian of RECORD.sig bytes */
#define SIG_FROM1_BYTE    11
#define SEL_BIT_LENGTH    20 //28

#define BUCKETSIZE        4096
#define HTBSIZE           ((1UL << SEL_BIT_LENGTH) * BUCKETSIZE)

/* throttles htb sedimentation if it takes shorter than */
#define THROTTLESEC       20

void
*sock_routine(void*), *log_routine(void*);

extern  int   offd, htbd;
extern  char  *htb_map, sock_prefix[], htbpath[], tmppath[], logpath[];

typedef struct __attribute__((__packed__))
{
  uint8_t pkey[32];       // Ed25519 public key
  uint8_t sig[64];        // Ed25519 signature of
  uint8_t id[24];         // GUID
  union
  {
    uint32_t  present;
    char      padding[8]; // up to 128 bytes
  };
}                         RECORD;

#define unlikely(x)     __builtin_expect((x),0)
