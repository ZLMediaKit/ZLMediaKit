#include <cassert>
#include <fstream>
#include <iterator>

#ifdef _WIN32
#include <direct.h>
#else
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
    return _mkdir(path.c_str());
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

    auto empty_destination = directory + "empty";
#ifndef _WIN32
    auto previous_umask = umask(0022);
#endif
    {
        HttpFileStorage storage(empty_destination);
#ifndef _WIN32
        bool private_temporary = false;
        File::scanDir(directory, [&](const std::string &path, bool is_dir) {
            if (!is_dir && path.find(".upload-") != std::string::npos) {
                struct stat status;
                private_temporary = stat(path.c_str(), &status) == 0 && (status.st_mode & 0777) == 0600;
            }
            return true;
        }, false, true);
        assert(private_temporary);
#endif
        storage.writeData(nullptr, 0, 0);
    }
#ifndef _WIN32
    umask(previous_umask);
    struct stat empty_status;
    assert(stat(empty_destination.c_str(), &empty_status) == 0 && (empty_status.st_mode & 0777) == 0644);
#endif
    assert(File::fileExist(empty_destination) && readFile(empty_destination).empty());

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

#ifdef _WIN32
    // Keep the complete path below legacy MAX_PATH on Windows CI runners.
    auto long_name = directory + std::string(120, 'x');
#else
    auto long_name = directory + std::string(240, 'x');
#endif
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
    auto moving_directory = directory + "moving";
    auto moved_directory = directory + "moved";
    assert(makeDirectory(moving_directory) == 0);
    {
        HttpFileStorage storage(moving_directory + "/destination");
        assert(rename(moving_directory.c_str(), moved_directory.c_str()) == 0);
        storage.writeData("anchored", 8, 8);
    }
    assert(readFile(moved_directory + "/destination") == "anchored");

    auto backslash_name = directory + "back\\slash";
    {
        HttpFileStorage storage(backslash_name);
        storage.writeData("backslash", 9, 9);
    }
    assert(readFile(backslash_name) == "backslash");

    auto fifo = directory + "fifo";
    assert(mkfifo(fifo.c_str(), 0600) == 0);
    bool fifo_rejected = false;
    try {
        HttpFileStorage storage(fifo);
    } catch (const std::runtime_error &) {
        fifo_rejected = true;
    }
    struct stat fifo_status;
    assert(fifo_rejected && lstat(fifo.c_str(), &fifo_status) == 0 && S_ISFIFO(fifo_status.st_mode));

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
