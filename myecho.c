#include <ctype.h>
#include <stdio.h>
extern char **environ;

int main(int argc, char *argv[]) {
  int i, j;
  for (i=0; i<argc; i++) {
    printf("argv[%d]: %c", i,0x22);
    for (j=0; argv[i][j]; j++) {
      char c = argv[i][j];
      if (isblank(c))
	putchar('_');
      else
	putchar(c);
    }
    putchar('"');
    putchar('\n');
  }	
  printf("environment:\n");
  if (environ)
    for (i=0; environ[i]; i++)
      printf("environ[%d]: %s\n",i,environ[i]);
  else
    puts("(NULL)\n");
  return 0;
}
