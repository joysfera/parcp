#define TABLE_SIZE  256

static unsigned long crc32tab[TABLE_SIZE];

void init_CRC32(void)
{
  int i, inx;
  int carry32;
  unsigned long entry32;

  for (inx = 0; inx < TABLE_SIZE; ++inx) {
    entry32 = inx;

    for (i = 0; i < 8; ++i) {
      carry32 = entry32 & 1;
      entry32 >>= 1;
      if (carry32)
        entry32 ^= 0xedb88320;
    }

    crc32tab[inx] = entry32;
  }
}

unsigned long compute_CRC32(char *buf, int count)
{
  int i;
  unsigned char inx32;
  unsigned long crc32;

  crc32 = 0xffffffff;   /* pre-condition - bits all ones */

  for (i = 0; i < count; ++i) {
    inx32 = buf[i] ^ crc32;
    crc32 >>= 8;
    crc32 ^= crc32tab[inx32];
  }
  crc32 ^= 0xffffffff;  /* post-condition - one's complement */

  return crc32;
}
