#include <cassert>
#include <fstream>

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "Http/HttpBody.h"
#include "Util/File.h"
#include "Util/util.h"

using namespace mediakit;
using namespace toolkit;

static std::string readFile(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

int main() {
    auto directory = std::string("http-file-storage-") + makeRandStr(16) + "/";
    assert(mkdir(directory.c_str(), 0755) == 0);

    auto destination = directory + "destination";
    assert(File::saveFile("old", destination));
    {
        HttpFileStorage storage(destination);
        storage.writeData("new", 3, 4);
    }
    assert(readFile(destination) == "old");

    {
        HttpFileStorage storage(destination);
        storage.writeData("new", 3, 3);
        assert(readFile(storage.filePath()) == "new");
    }

    auto long_name = directory + std::string(240, 'x');
    {
        HttpFileStorage storage(long_name);
        storage.writeData("long", 4, 4);
    }
    assert(readFile(long_name) == "long");

    auto invalid_destination = directory + "directory";
    assert(mkdir(invalid_destination.c_str(), 0755) == 0);
    bool replace_failed = false;
    try {
        HttpFileStorage storage(invalid_destination);
        storage.writeData("fail", 4, 4);
    } catch (const std::runtime_error &) {
        replace_failed = true;
    }
    assert(replace_failed && File::is_dir(invalid_destination));

#ifndef _WIN32
    auto target = directory + "target";
    auto link = directory + "link";
    assert(File::saveFile("old", target));
    assert(symlink("target", link.c_str()) == 0);
    {
        HttpFileStorage storage(link);
        assert(storage.filePath() == link);
        storage.writeData("linked", 6, 6);
    }
    struct stat status;
    assert(lstat(link.c_str(), &status) == 0 && S_ISLNK(status.st_mode));
    assert(readFile(target) == "linked");
#endif

    assert(File::delete_file(directory) == 0);
    return 0;
}
