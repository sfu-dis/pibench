#include "key_loader.hpp"



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


key_loader_t::key_loader_t()
{
    this->buffer_len = this->get_number_lines(this->filename);
    this->keys = (char**)calloc(this->buffer_len, sizeof(char*));
    this->key_len = new u_int64_t[this->buffer_len];
}


void key_loader_t::fill_buffer()
{    
    FILE* fd = fopen(this->filename, "r");  
    size_t len = 0;
    ssize_t read;
    char* line = NULL;
    u_int64_t idx = 0;

    if (!fd)
    {
        std::cout << "Error open selected file." << std::endl;
        return;
    }

    while((read = getline(&line, &len, fd)) != -1)
    {
        if (line[0] == '\n')    
            continue;
        else if (line[read-1] == '\n')        
            line[read-1] = '\0';
        printf("line: %s\n", line);
        this->keys[idx] = line;
        this->key_len[idx] = read;
        idx++;
    }
}