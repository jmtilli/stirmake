#ifndef _CMDDB_H_
#define _CMDDB_H_

#include <map>
#include <vector>
#include <string>
#include <iostream>

class CmdDb {
  public:
    std::map<std::string, std::vector<std::vector<std::string> > > cmds;
    void serialize(std::ostream &os);
    CmdDb(void) {}
    CmdDb(std::istream &is);
};

#endif
