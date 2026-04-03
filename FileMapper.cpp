#include "FileMapper.hpp"

FileMapper::FileMapper(const std::string &path) : // reference la string, ca sa nu faca copierea degeaba
    data_(nullptr), 
    size_(0)
{
    // path va fi verificat cu asutorul la File::get_abs_path(path) din PathUtils.hpp
    const char* c_path = path.c_str(); // open needs a const char*
    
    int file_fd=open(c_path,O_RDONLY);
    //open gives us the fd
    if(file_fd==-1)
    {
        throw(std::__throw_system_error);
    }

    struct stat statbuf; // this will get filled by fstat with all the file's data

    if(fstat(file_fd,&statbuf)==0)
    {
        // returns 0 on success
        if(statbuf.st_size == 0)
        {
            size_=0;
            data_=nullptr;
            // we can't send a file of 0 bytes;
        }
        else
        {
            // here is the "general" case

            data_=mmap(NULL, size_, PROT_READ, MAP_PRIVATE,file_fd, 0);
            close(file_fd); // in either case, we don't need it anymore, so we just close it

            if(data_=MAP_FAILED)
            {
                throw(std::__throw_system_error);
            }
        }
    }
    else
    {
        close(file_fd); // critial to avoid leaks
        throw(std::__throw_system_error);
    }



}

FileMapper::~FileMapper()
{
    munmap(data_, size_);
}

void * FileMapper::get_data()
{
    return data_;
}

size_t FileMapper::get_size()
{
    return size_;
}