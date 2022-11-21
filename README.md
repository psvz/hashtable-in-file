### Ed25519 signature as a hash in a table (hash table intuition)
This algo produces deterministic signatures. As this script demonstrates 508 out of 512 bits of the signature has equal odds of coming as 0 or 1 with changing message and the same key.

The signature is little-endian i.e., lower half of 64th (address-wise) byte and first 63 bytes have homogeneous bit distribution. That is, they are good to be a hash key.

If we identify a bucket by $n$ bits, the number of buckets in our table is $2^n$, and probability of hitting a particular one is $2^{-n}$. To hit that specific bucket, we need $N$ samples on average, so that $2^{-n}N\simeq1$. If every bucket has a capacity of $c$ slots, we may wish to hit it closer to $c$ times, rather than 1, or $2^{-n} < c/N$.
#### Ext4 filesystem and persistent hash table
For example, if size of a record is 128 bytes, and size of a filesystem logical block is 4096 bytes, as per `tune2fs -l /dev/vda1 |grep Block`, we can assume $c=2^5$ (32 records) per block. Here, filesystem's blocks are buckets. Thus, if an upper bound of the table filled is $2^{33}$ (8 billion) records, we can use 28-bits identifier for a bucket. In other words, a sparse file of size 1TB (`4096 * (1 << 28)`). Ext4 filesystem supports files up to 16TB in size.

When the table isn't fully filled, we can use `lseek()` and `copy_file_range()` functions to remove the holes in post-processing. On the other hand, overflow space outside the main file needed to handle worst cases.
### Fast uniqueness validation and logging of Ed25519-signed GUIDs
Seamagic code here works as a counterpart of [Openresty](https://openresty.com/en/) (aka Lua-enhanced nginx) and adds unique [GUIDs](https://en.wikipedia.org/wiki/Universally_unique_identifier) from incoming HTTP requests to a file-backed hash table. Signature verification by [libsodium](https://opm.openresty.org/package/jprjr/luasodium/) and non-unique case-handling is done in Lua (not part of this repo). The hash table is updated by independent threads in lock-less (mutex-free) manner...

Seamagic creates a number of unix-sockets served by one thread each. Nginx worker selects the socket by reducing key to the hash table in consistent manner. For example, if the key (specific bits of the signature configured in seamagic.h) is 98038658, and seamagic is configured to run 3 threads, Lua script sends the GUID from nginx worker to a socket number `98038658 % 3 = 2` - the socket is named `<basename>.2`. Seamagic thread checks/updates a bucket number 98038658 in the hash table and returns 1 or `nil`, signaling whether the GUID is recorded or is in the table already. That is, each thread handles own slice of the hash table and queues requests for that slice from all nginx workers.

Another gain in performance could result from integrating seamagic code into nginx workers themselves - that would avoid nginx-seamagic context switching and overhead of socket IPC. Unfortunately, nginx doesn't offer such possibility (worker-specific configuration), and it may lie outside its paradigm altogether. So, complicating it further may or may not be a good thing.

Due to alignments of data structures with memory paging, any practical crash should not result in data corruption. On the other hand, amount of data not persisted to file-storage is controlled by Linux kernel knobs (dirty page parameters in `/proc/sys/vm`). In other words, seamagic offers some configurable amount of data that is always at risk and smooth data write-outs under kernel control. Performance is block I/O bound with whole RAM size to cache.

There are two control signals. USR1 purges all data (except .log file) and HUP syncs caches to disk. The signals are not intended for use under load. They are used for flushing data in post-processing scenario.

Log file is cyclically updated on the best effort basis by sedimentation of the sparse file with the hash table into a .tmp file and, atomically, renaming the latter into .log. Every line looks like

    uBgXBIc6v49QdygE5tp79RRnnqIScyZAd9t4hkBUhjQ= QhD6qeEvSkUiYTXX84I4pgbIQp8R9sepBaoUKScIAJGpgHtn0jIsOrad2fTqxx5SP4vI02f8RIST1aUibNcdBg== HgJabbZ6Zw7Nag6RAfIxTTcxC8r0uXGa
where there are base64 encoded `public key`(32 bytes)`signature`(64 bytes)`GUID`(24 bytes). Base64 encoded strings are separated by a space in the log file. The same fields arrive in the same order without encoding (raw bytes) or separation (as they are fixed width fields) on the Unix socket from nginx.

Seamagic relies on a couple of non-portable Linux-specific features viz., `lseek(SEEK_DATA)` and `sched_setscheduler(SCHED_IDLE)`.
