### Ed25519 signature as a hash in a table (hash table intuition)
This algo produces deterministic signatures. As this script demonstrates 508 out of 512 bits of the signature has equal odds of coming as 0 or 1 with changing message and the same key.

The signature is little-endian i.e., lower half of 64th (address-wise) byte and first 63 bytes have homogeneous bit distribution. That is, they are good to be a hash key.

If we identify a bucket by $n$ bits, the number of buckets in our table is $2^n$, and probability of hitting a particular one is $2^{-n}$. To hit that specific bucket, we need $N$ samples on average, so that $2^{-n}N\simeq1$. If every bucket has a capacity of $c$ slots, we may wish to hit it closer to $c$ times, rather than 1, or $2^{-n} < c/N$.
#### Ext4 filesystem and persistent hash table
For example, if size of a record is 128 bytes, and size of a filesystem logical block is 4096 bytes, as per `tune2fs -l /dev/vda1 |grep Block`, we can assume $c=2^5$ (32 records) per block. Here, filesystem's blocks are buckets. Thus, if an upper bound of the table filled is $2^{33}$ (8 billion) records, we can use 28-bits identifier for a bucket. In other words, a sparse file of size 1TB (`4096 * (1 << 28)`). Ext4 filesystem supports files up to 16TB in size.

When the table isn't fully filled, we can use `lseek()` and `copy_file_range()` functions to remove the holes in post-processing. On the other hand, overflow space outside the main file needed to handle worst cases.
