#include <ctype.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

void appendVar(std::vector<std::string> &v, std::string varname, bool *white)
{
  if (varname == "")
  {
    throw std::runtime_error("empty var name");
  }
  if (varname[0] == 'l')
  {
    v.push_back("[" + varname + "]");
    v.push_back("[" + varname + "]");
    v.push_back("[" + varname + "]");
    *white = true;
  }
  else if (*white)
  {
    v.push_back("[" + varname + "]");
  }
  else
  {
    v.back() += std::string("[" + varname + "]");
  }
}

bool iswhite(char ch)
{
  switch (ch) {
    case ' ': return true;
    case '\t': return true;
  }
  return false;
}
bool isallowed(char ch)
{
  if (isalpha((unsigned char)ch))
    return true;
  if (isdigit((unsigned char)ch))
    return true;
  return false;
}

std::vector<std::string> expand(std::string original)
{
  std::istringstream iss(original);
  std::vector<std::string> v;
  bool white = 0;
  char ch;
  for (;;)
  {
    iss.exceptions(std::ios_base::badbit);
    ch = iss.get();
    if (!iss)
    {
      break;
    }
    if (ch == '$')
    {
      std::ostringstream vaross;
      iss.exceptions(std::ios_base::eofbit | std::ios_base::failbit | std::ios_base::badbit);
      ch = iss.get();
      if (ch == '(')
      {
        for (;;)
        {
          ch = iss.get();
          if (ch == ')')
          {
            break;
          }
          else if (isallowed(ch))
          {
            vaross << ch;
          }
          else
          {
            throw std::runtime_error("unallowed char in var name");
          }
        }
      }
      else
      {
        throw std::runtime_error("expecting '('");
      }
      appendVar(v, vaross.str(), &white);
      white = 0;
    }
    else if (iswhite(ch))
    {
      do {
        ch = iss.get();
      } while(iss && iswhite(ch));
      if (iss)
      {
        iss.putback(ch);
      }
      white = true;
    }
    else
    {
      if (v.empty() || white)
      {
        v.push_back(std::string());
      }
      white = false;
      v.back() += ch;
    }
  }
  return v;
}

int main(int argc, char **argv)
{
  std::string str("foo bar baz $(FOO)$(BAR) -I$(BAZ) $(lBARF) -I$(lFOO) quux");
  std::vector<std::string> v = expand(str);
  for (auto it = v.begin(); it != v.end(); it++)
  {
    std::cout << *it << "." << std::endl;
  }
}
