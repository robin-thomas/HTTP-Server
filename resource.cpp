#include <iostream>
#include <algorithm>
#include <sstream>
#include <unistd.h>
#include <fcntl.h>
#include "resource.h"


void Resource::addUrl(std::string path, int conn) {
    this->path = "./htdocs" + path ;
    this->conn = conn ;
}


// Function to check whether we can actually access the resource
int Resource::checkResource() {
    return (this->resource = open(this->path.c_str(), O_RDONLY)) ;
}


std::string Resource::getPath() {
    return this->path ;
}

// Function to decode url encoded string
void Resource::decodeUrl() {
    std::ostringstream ss ;
    for (std::string::const_iterator i = this->path.begin(); i != this->path.end(); i++) {
        std::string::value_type c = *i ;
        if (c == '+') {
            ss << std::string::value_type(' ') ;
        } else if (c == '%') {
            std::string str(i + 1, i + 3) ;
            ss << std::string::value_type(strtol(str.c_str(), NULL, 16)) ;
            i += 2 ;
        } else {
            ss << c ;
        }
    }
    this->path = ss.str() ;
}


std::string Resource::substring() {
    std::string::size_type type = this->path.rfind('.') ;
    if (type != std::string::npos) {
        return this->path.substr(type + 1) ;
    } else {
        return std::string("") ;
    }
}


// Function to remove extra spaces from the url
void Resource::trimUrl() {
    this->path.erase(remove_if(this->path.begin(), this->path.end(), [](const char& c) {
        return (c == ' ') ;
    }), this->path.end()) ;
}


// Function to send the requested resource through the socket
void Resource::getResource() {
    int noBytes ;
    char buf[4096] ;
    while ((noBytes = read(this->resource, buf, 4096)) > 0) {
        size_t offset = 0 ;
        int sent ;
        while ((sent = write(this->conn, buf + offset, noBytes)) > 0 || (sent == -1 && errno == EINTR)) {
            if (sent > 0) {
                offset  += sent ;
                noBytes -= sent ;
            }
        }
    }
}
