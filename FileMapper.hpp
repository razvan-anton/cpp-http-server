#ifndef FILE_MAPPER
#define FILE_MAPPER

#include <string_view>
#include <string>
#include <sys/mman.h> // for mmap, munmap..
#include <cstddef> // for size_t

class FileMapper{
public:
    FileMapper(const std::string &path);

    ~FileMapper();

    void *get_data();

    size_t get_size();




private:
    void * data_; // type void * cuz it points to raw bytes
    size_t size_;
};



#endif