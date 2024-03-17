#pragma once

#include<stdexcept>

/* Input module exceptions */
/**
 * @brief Audio mixer input module fatal error
 *        Module will therminate immediatly
 *        Audio data inside the module will be lost
 */
class module_fatal_err : public std::runtime_error
{
public:
    module_fatal_err(const char* msg) 
        : std::runtime_error(msg) {}
    const char* what() const noexcept override { return std::runtime_error::what(); }
};

/**
 * @brief Object not found error
 * File or audio source not exist.
 */
class module_object_not_found : public std::runtime_error
{
public:
    module_object_not_found(const char* msg) 
        : std::runtime_error(msg) {}
    const char* what() const noexcept override { return std::runtime_error::what(); }
};

class module_object_out_of_range : public std::out_of_range
{
public : 
    module_object_out_of_range(const char* msg) : std::out_of_range(msg) {}
    const char* what() const noexcept override { return std::out_of_range::what(); }
};
