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

static int makeDirectory(const std::string &path) {
#ifdef _WIN32
    return mkdir(path.c_str(), 0);
#else
    return mkdir(path.c_str(), 0755);
#endif
}

static std::string readFile(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

int main() {
    auto directory = std::string("http-file-storage-") + makeRandStr(16) + "/";
    assert(makeDirectory(directory) == 0);

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

    auto old_reader = std::make_shared<HttpFileBody>(destination);
    {
        HttpFileStorage storage(destination);
        storage.writeData("latest", 6, 6);
    }
    auto new_reader = std::make_shared<HttpFileBody>(destination);
    auto new_data = new_reader->readData(6);
    assert(new_data && std::string(new_data->data(), new_data->size()) == "latest");

    {
        HttpFileStorage storage(destination);
        storage.writeData("overflow", 8, 4);
    }
    assert(readFile(destination) == "latest");

    auto long_name = directory + std::string(240, 'x');
    {
        HttpFileStorage storage(long_name);
        storage.writeData("long", 4, 4);
    }
    assert(readFile(long_name) == "long");

    auto invalid_destination = directory + "directory";
    assert(makeDirectory(invalid_destination) == 0);
    bool replace_failed = false;
    try {
        HttpFileStorage storage(invalid_destination);
        storage.writeData("fail", 4, 4);
    } catch (const std::runtime_error &) {
        replace_failed = true;
    }
    assert(replace_failed && File::is_dir(invalid_destination));

#ifndef _WIN32
    assert(chmod(destination.c_str(), 0600) == 0);
    {
        HttpFileStorage storage(destination);
        storage.writeData("mode", 4, 4);
    }
    struct stat destination_status;
    assert(stat(destination.c_str(), &destination_status) == 0);
    assert((destination_status.st_mode & 0777) == 0600);

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

    auto dangling_target = directory + "new-target";
    auto dangling_link = directory + "dangling-link";
    assert(symlink("new-target", dangling_link.c_str()) == 0);
    {
        HttpFileStorage storage(dangling_link);
        storage.writeData("created", 7, 7);
    }
    assert(readFile(dangling_target) == "created");
#endif

    assert(File::delete_file(directory) == 0);
    return 0;
}
