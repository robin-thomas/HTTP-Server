#ifndef _HEADER_H_
#define _HEADER_H_

#include <sstream>
#include "resource.h"

#define BUF_SIZE 4096

class Header {
    private:
        std::stringstream header ;
        int status ;
        int conn ;
        size_t size ;
        int contentLen ;
        time_t caching ;
        bool cgi ;
        std::string connection ;
        std::string statMsg ;
        std::string redirect ;
        std::string mtime ;
        std::string method ;
        std::string version ;
        std::string errMsg ;
        std::string host ;
        std::string mime ;
        std::string queryString ;
        std::string body ;
        std::string phpSessID ;
        std::string boundary ;
        std::string files ;
        Resource path ;
    public:
        Header(int conn) {this->conn = conn; this->cgi = this->size = this->caching = this->status = this->contentLen = 0;}
        int getRequest(bool body = 0, bool reset = 0) ;
        void pushRequest() ;
        void serviceRequest() ;
        int readHeader(bool body = 0) ;
        void writeHeader(const char *header, size_t len) ;
        void parseHeader() ;
        void parseMultiFormData() ;
        void errHeader() ;
        int execCGI() ;
        std::string replaceCRLF(std::string temp) ;
        std::string sanitize(std::string temp) ;
};

#endif
