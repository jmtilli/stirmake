#include "jsonyyutils.h"

int main(int argc, char **argv)
{
  struct abce abce = {};
  struct jsonyy jsonyy = {.abce = &abce};
  char *json =
"{"
"  \"foo\": [1, 2, 3],\n"
"  \"bar\": -2.3e-5\n"
"}\n"
;
  abce_init(&abce);
  if (jsonyydomemparse(json, strlen(json), &jsonyy) != 0)
  {
    printf("Parsing failure\n");
  }
  return 0;
}
