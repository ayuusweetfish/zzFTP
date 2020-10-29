#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
  if (argc < 6) {
    printf("usage: %s <file-A> <file-A-start>"
                    " <file-B> <file-B-start> <len>\n",
      argv[0]);
    return 1;
  }

  FILE *a = fopen(argv[1], "rb"),
       *b = fopen(argv[3], "rb");
  if (a == NULL || b == NULL) {
    if (a == NULL) printf("Cannot open %s\n", argv[1]);
    if (b == NULL) printf("Cannot open %s\n", argv[3]);
    return 1;
  }

  int a_start = strtoll(argv[2], NULL, 10);
  int b_start = strtoll(argv[4], NULL, 10);
  int len = strtoll(argv[5], NULL, 10);

  fseek(a, a_start, SEEK_SET);
  fseek(b, b_start, SEEK_SET);
  unsigned char a_ch, b_ch;
  for (int i = 0; i < len; i++)
    if ((a_ch = fgetc(a)) != (b_ch = fgetc(b))) {
      printf("Byte %d: a = %3d, b = %3d\n",
        i, (int)a_ch, (int)b_ch);
    }
  printf("Compare finished\n");

  return 0;
}
