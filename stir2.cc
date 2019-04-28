#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <vector>
#include <string>
#include <iostream>

class Cmd {
  public:
    std::vector<std::string> args;
    Cmd(std::vector<std::string> v): args(v) {}
};

int children = 0;
const int limit = 2;

void fork_child(Cmd cmd)
{
  std::vector<char*> args;
  pid_t pid;

  pid = fork();
  if (pid < 0)
  {
    abort();
  }
  else if (pid == 0)
  {
    for (auto it = cmd.args.begin(); it != cmd.args.end(); it++)
    {
      args.push_back(strdup(it->c_str()));
    }
    args.push_back(NULL);
    execvp(args[0], &args[0]);
    //write(1, "Err\n", 4);
    _exit(1);
  }
  else
  {
    children++;
  }
}

std::vector<Cmd> cmds;

int main(int argc, char **argv)
{
  std::vector<std::string> args{"sleep", "2"};
  std::vector<std::string> argp{"echo", "bar", "baz"};
  Cmd cs(args);
  Cmd cp(argp);
  cmds.push_back(cp);
  cmds.push_back(cs);
  cmds.push_back(cs);
  cmds.push_back(cs);
  cmds.push_back(cs);

  while (children < limit && !cmds.empty())
  {
    std::cout << "forking1 child" << std::endl;
    fork_child(cmds.back());
    cmds.pop_back();
  }

  for (;;)
  {
    int wstatus = 0;
    pid_t pid;
    pid = wait(&wstatus);
    if (wstatus != 0 && pid > 0)
    {
      if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0)
      {
        std::cout << "error exit status" << std::endl;
        std::cout << WIFEXITED(wstatus) << std::endl;
        std::cout << WIFSIGNALED(wstatus) << std::endl;
        std::cout << WEXITSTATUS(wstatus) << std::endl;
        return 1;
      }
    }
    if (children <= 0)
    {
      if (pid < 0 && errno == ECHILD)
      {
        return 0;
      }
      abort();
    }
    children--;
    while (children < limit && !cmds.empty())
    {
      std::cout << "forking child" << std::endl;
      fork_child(cmds.back());
      cmds.pop_back();
    }
  }
  return 0;
}
