#include "key_loader.hpp"


key_loader_t::key_loader_t()
{
    this->buffer_len = this->get_number_lines(this->filename);
    this->keys = calloc(this->buffer_len, sizeof(char*));
    this->values = calloc(this->buffer_len, sizeof(char*));
}

u_int64_t key_loader_t::get_number_lines(char* filename)
{
    uint64_t number_of_lines = 0;
    std::string line;
    std::ifstream myfile(filename);

    while (std::getline(myfile, line))
        number_of_lines++;
    std::cout << "Number of lines in text file: " << number_of_lines;
    return number_of_lines;
}