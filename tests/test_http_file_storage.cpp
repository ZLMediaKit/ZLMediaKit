#include <cassert>
#include <fstream>
#include <iterator>
#include <type_traits>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include "Common/AtomicFile.h"
#include "Http/HttpBody.h"
#include "Util/File.h"
#include "Util/util.h"

using namespace mediakit;
using namespace toolkit;

static_assert(!std::is_copy_constructible<HttpFileStorage>::value, "HttpFileStorage owns its file handle");
static_assert(!std::is_copy_assignable<HttpFileStorage>::value, "HttpFileStorage owns its file handle");

static std::string readFile(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

static int makeDirectory(const std::string &path) {
#ifdef _WIN32
    return _mkdir(path.c_str());
#else
    return mkdir(path.c_str(), 0755);
#endif
}

int main() {
    auto directory = std::string("http-file-storage-") + makeRandStr(16) + "/";
    assert(makeDirectory(directory) == 0);
    auto destination = directory + "destination";
    assert(File::saveFile("old", destination));

    // An incomplete upload must leave the published file untouched.
    {
        HttpFileStorage storage(destination);
        storage.writeData("new", 3, 4);
    }
    assert(readFile(destination) == "old");

    // Completion publishes before the storage object is destroyed.
    {
        HttpFileStorage storage(destination);
        storage.writeData("new", 3, 3);
        assert(readFile(storage.filePath()) == "new");
    }

    // Empty files are complete when the zero-length body is delivered.
    auto empty = directory + "empty";
    {
        HttpFileStorage storage(empty);
        storage.writeData(nullptr, 0, 0);
    }
    assert(File::fileExist(empty) && readFile(empty).empty());

    // The reusable helper has the same replace-existing behavior as uploads.
    std::string temporary;
    auto file = createFileForAtomicReplace(destination, temporary);
    assert(file && fwrite("helper", 1, 6, file) == 6 && fclose(file) == 0);
    assert(atomicReplaceFile(temporary, destination));
    assert(readFile(destination) == "helper");

    // A changing declaration is rejected and the original remains published.
    bool rejected = false;
    try {
        HttpFileStorage storage(destination);
        storage.writeData("a", 1, 2);
        storage.writeData("b", 1, 3);
    } catch (const std::runtime_error &) {
        rejected = true;
    }
    assert(rejected && readFile(destination) == "helper");

    assert(File::delete_file(directory) == 0);
    return 0;
}
