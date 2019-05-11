#include "cmddb.h"
#include <iomanip>

std::string tohex2(char ch)
{
  std::ostringstream oss;
  oss << std::hex << std::setfill('0') << std::setw(2) << (int)ch;
  return oss.str();
}

void serialize_string(std::ostream &os, const std::string &str)
{
  os << "\"";
  for (auto it = str.begin(); it != str.end(); it++)
  {
    switch (*it)
    {
      case '\n': os << "\\n"; break;
      case '\b': os << "\\b"; break;
      case '\f': os << "\\f"; break;
      case '\r': os << "\\r"; break;
      case '\t': os << "\\t"; break;
      case '"': os << "\\\""; break;
      case '\\': os << "\\\\"; break;
      default:
        if ((*it) < 0x20)
        {
          os << "\\u00" << tohex2(*it);
        }
        else
        {
          os << *it;
        }
    }
  }
  os << "\"";
}

void CmdDb::serialize(std::ostream &os)
{
  os << "{\"tgts\":";
  os << "{";
  for (auto it = cmds.begin(); it != cmds.end(); it++)
  {
    serialize_string(os, it->first);
    os << ":{\"cmds\":";
    os << "[";
    for (auto it2 = it->second.begin(); it2 != it->second.end(); it2++)
    {
      if (it2 != it->second.begin())
      {
        os << ",";
      }
      os << "[";
      for (auto it3 = it2->begin(); it3 != it2->end(); it3++)
      {
        if (it3 != it2->begin())
        {
          os << ",";
        }
        serialize_string(os, *it3);
      }
      os << "]";
    }
    os << "]";
    os << "}";
  }
  os << "}";
  os << "}";
}

int main(int argc, char **argv)
{
  CmdDb db;
  std::vector<std::string> cmd1{"echo", "foo"};
  std::vector<std::string> cmd2{"touch", "a.txt"};
  std::vector<std::vector<std::string> > cmds{cmd1, cmd2};
  db.cmds["a.txt"] = cmds;
  db.serialize(std::cout);
  std::cout << std::endl;
  return 0;
}
