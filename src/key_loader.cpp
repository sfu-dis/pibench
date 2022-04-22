#include "key_loader.hpp"

thread_local std::default_random_engine key_loader_t::generator_;
thread_local uint32_t key_loader_t::seed_;
thread_local uint64_t key_loader_t::current_id_ = 1;

uint64_t key_loader_t::get_number_lines(char const *filename)
{
    uint64_t number_of_lines = 0;
    std::string line;
    std::ifstream myfile(filename);

    while (std::getline(myfile, line))
        number_of_lines++;
    std::cout << "Number of lines in text file: " << number_of_lines << std::endl;
    return number_of_lines;
}


key_loader_t::key_loader_t()
{
    this->buffer_len = this->get_number_lines(this->filename);
    this->keys = new char*[this->buffer_len];
    this->key_len = new uint16_t[this->buffer_len];
}


void key_loader_t::fill_buffer()
{    
    FILE* fd = fopen(this->filename, "r");  
    size_t len = 0;
    ssize_t read;
    char* line = NULL;
    uint64_t idx = 0;
    //uint64_t max = 0;
    if (!fd)
    {
        std::cout << "Error open selected file." << std::endl;
        return;
    }

    while((read = getline(&line, &len, fd)) != -1)
    {
        if (line[0] == '\n')    
            continue;
        else if (line[read - 1] == '\n')        
            line[read - 1] = '\0';
        this->keys[idx] = new char[max_length]; // for now use max length
        // this->keys[idx] = new char[read - 1];
        memcpy(this->keys[idx], line, read - 1);
        this->key_len[idx] = read - 1;
	//if (read-1 > max)
	    //max = read - 1;
        // std::cout << "line: " << this->keys[idx] << std::endl;
        // std::cout << "read: " << this->key_len[idx] << std::endl;
        idx++;
    }
    //printf("The max length of key is %lld\n", max);
}


std::pair<char*, uint64_t> key_loader_t::next()
{
    uint64_t random_idx = next_id() % this->buffer_len;
    return std::make_pair(this->keys[random_idx], this->key_len[random_idx]);
}


