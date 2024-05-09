//	100% Public Domain.
//
//	Original C Code
//	 -- Steve Reid <steve@edmweb.com>
//	Small changes to fit into bglibs
//	  -- Bruce Guenter <bruce@untroubled.org>
//	Translation to simpler C++ Code
//	  -- Volker Grabsch <vog@notjusthosting.com>
//	Safety fixes
//	  -- Eugene Hopkinson <slowriot at voxelstorm dot com>
//  Adapt for project
//      Dmitriy Khaustov <khaustov.dm@gmail.com>
//
// File created on: 2017.02.25

// SHA1.h

#pragma once

#include <cstdint>
#include <iostream>
#include <string>

namespace toolkit {

class SHA1 final
{
public:
    SHA1();

    void update(const std::string &s);
    void update(std::istream &is);
    std::string final();
    std::string final_bin();

    static std::string from_file(const std::string &filename);

    static std::string encode(const std::string &s);
    static std::string encode_bin(const std::string &s);

private:
    uint32_t digest[5];
    std::string buffer;
    uint64_t transforms;
};

}//namespace toolkit