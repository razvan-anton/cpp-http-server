#ifndef FILE_MAPPER
#define FILE_MAPPER

#define _FILE_OFFSET_BITS 64
// to force the 32-bit systems to use a 8 byte value for size to support larger file

#include <string>
#include <string_view>
#include <sys/mman.h> // for mmap, munmap..
#include <cstddef> // for size_t
#include <fcntl.h>  // for open
#include <sys/stat.h>   // for fstat
#include <unistd.h> // for close
#include <iostream> // to error logs
#include <cerrno>

class FileMapper{
public:
    FileMapper(const std::string &path);

    ~FileMapper();

    const void *get_data();

    size_t get_size();




private:
    const void * data_; // type void * cuz it points to raw bytes
    size_t size_;
};



#endif