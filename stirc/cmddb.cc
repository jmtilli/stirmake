#include "cmddb.h"
#include <iomanip>
#include <sstream>

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

void skip_whitespace(std::istream &is)
{
  char ch;
  for (;;)
  {
    is >> ch;
    if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t')
    {
      is.putback(ch);
      break;
    }
  }
}

bool ishex(char ch)
{
  switch (ch)
  {
    case '0': return 1;
    case '1': return 1;
    case '2': return 1;
    case '3': return 1;
    case '4': return 1;
    case '5': return 1;
    case '6': return 1;
    case '7': return 1;
    case '8': return 1;
    case '9': return 1;
    case 'A': return 1;
    case 'B': return 1;
    case 'C': return 1;
    case 'D': return 1;
    case 'E': return 1;
    case 'F': return 1;
    case 'a': return 1;
    case 'b': return 1;
    case 'c': return 1;
    case 'd': return 1;
    case 'e': return 1;
    case 'f': return 1;
    default: return 0;
  }
}

std::string read_string(std::istream &is)
{
  std::ostringstream oss;
  char ch = '\0';
  skip_whitespace(is);
  is >> ch;
  if (ch != '"')
  {
    throw std::runtime_error("expecting string");
  }
  for (;;)
  {
    is >> ch;
    if (ch == '\\')
    {
      is >> ch;
      switch (ch)
      {
        case '"': oss << '"'; break;
        case '\\': oss << '\\'; break;
        case '/': oss << '/'; break;
        case 'b': oss << '\b'; break;
        case 'f': oss << '\f'; break;
        case 'n': oss << '\n'; break;
        case 'r': oss << '\r'; break;
        case 't': oss << '\t'; break;
        case 'u':
        {
          char ch1, ch2, ch3, ch4;
          uint16_t u16;
          is >> ch1 >> ch2 >> ch3 >> ch4;
          if (!ishex(ch1) || !ishex(ch2) || !ishex(ch3) || !ishex(ch4))
          {
            throw std::runtime_error("expecting hex");
          }
          std::ostringstream oss2;
          oss2 << ch1 << ch2 << ch3 << ch4;
          std::istringstream iss2(oss2.str());
          iss2 >> std::hex >> u16;
          if (u16 < 128)
          {
            oss << (char)u16;
          }
          else if (u16 < 2048)
          {
            oss << (char)((u16>>6)|0xC0);
            oss << (char)((u16&0x3F));
          }
          else
          {
            oss << (char)((u16>>12)|0xE0);
            oss << (char)(((u16>>6)&0x3F));
            oss << (char)((u16&0x3F));
          }
          break;
        }
        default: throw std::runtime_error("invalid escape");
      }
    }
    else if (ch == '"')
    {
      break;
    }
    else if (ch < 0x20)
    {
      throw std::runtime_error("expecting unescaped char");
    }
    else
    {
      oss << ch;
    }
  }
  return oss.str();
}

std::vector<std::vector<std::string> > get_cmds(std::istream &is)
{
  char ch;
  std::vector<std::vector<std::string> > cmds;
  skip_whitespace(is);
  is >> ch;
  if (ch != '[')
  {
    throw std::runtime_error("expecting [");
  }
  for (;;)
  {
    skip_whitespace(is);
    is >> ch;
    if (ch == ']') // RFE disallow [,] etc.
    {
      break;
    }
    if (ch != '[')
    {
      throw std::runtime_error("expecting [");
    }
    cmds.push_back(std::vector<std::string>());
    for (;;)
    {
      skip_whitespace(is);
      is >> ch;
      if (ch == ']') // RFE disallow [,] etc.
      {
        break;
      }
      is.putback(ch);
      cmds.back().push_back(read_string(is));
      skip_whitespace(is);
      is >> ch;
      if (ch == ']')
      {
        break;
      }
      else if (ch != ',')
      {
        throw std::runtime_error("expecting comma");
      }
    }
    skip_whitespace(is);
    is >> ch;
    if (ch == ']')
    {
      break;
    }
    else if (ch != ',')
    {
      throw std::runtime_error("expecting comma");
    }
  }
  return cmds;
}

// RFE skip unknown objects
CmdDb::CmdDb(std::istream &is)
{
  char ch = '\0';
  skip_whitespace(is);
  is >> ch;
  if (ch != '{')
  {
    throw std::runtime_error("expecting '{'");
  }
  if (read_string(is) != "tgts")
  {
    throw std::runtime_error("expecting tgts");
  }
  skip_whitespace(is);
  is >> ch;
  if (ch != ':')
  {
    throw std::runtime_error("expecting ':'");
  }
  skip_whitespace(is);
  is >> ch;
  if (ch != '{')
  {
    throw std::runtime_error("expecting '{'");
  }
  for (;;)
  {
    skip_whitespace(is);
    is >> ch;
    if (ch == '}')
    {
      break;
    }
    if (ch != '"')
    {
      throw std::runtime_error("expecting string");
    }
    is.putback(ch);
    std::string tgt = read_string(is);
    skip_whitespace(is);
    is >> ch;
    if (ch != ':')
    {
      throw std::runtime_error("expecting ':'");
    }
    skip_whitespace(is);
    is >> ch;
    if (ch != '{')
    {
      throw std::runtime_error("expecting ':'");
    }
    if (read_string(is) != "cmds")
    {
      throw std::runtime_error("expecting cmds");
    }
    skip_whitespace(is);
    is >> ch;
    if (ch != ':')
    {
      throw std::runtime_error("expecting ':'");
    }
    cmds[tgt] = get_cmds(is);
    // get values
    skip_whitespace(is);
    is >> ch;
    if (ch == '}')
    {
      break;
    }
    else if (ch != ',')
    {
      throw std::runtime_error("expecting comma");
    }
  }
  skip_whitespace(is);
  is >> ch;
  if (ch != '}')
  {
    throw std::runtime_error("expecting '}'");
  }
/*
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
*/
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
  std::ostringstream oss;
  db.cmds["a.txt"] = cmds;
  db.serialize(oss);
  std::istringstream iss(oss.str());
  std::string str3 = "\n"
"{\n"
"   \"tgts\" : {\n"
"      \"a.txt\" : {\n"
"         \"cmds\" : [\n"
"            [\n"
"               \"echo\",\n"
"               \"foo\"\n"
"            ],\n"
"            [\n"
"               \"touch\",\n"
"               \"a.txt\"\n"
"            ]\n"
"         ]\n"
"      }\n"
"   }\n"
"}\n";
  CmdDb db2(iss);
  std::istringstream iss3(str3);
  CmdDb db3(iss3);
  db3.serialize(std::cout); std::cout << std::endl;
  return 0;
}
