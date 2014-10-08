#include "header.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>

std::string genRandomStr(size_t len = 10) {
    srand(time(NULL)) ;                    
    std::string str ;
    static const char alpha[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    for (int i = 0; i < len; ++i) {
        str += alpha[rand() % strlen(alpha)] ;
    }
    return str ;
}

std::string Header::replaceCRLF(std::string temp) {
    size_t pos1 = 0, pos2 ;
    static int flag = 0 ;
    while (true) {
        size_t posr2 = temp.find('\r', pos1) ;
        size_t posn2 = temp.find('\n', pos1) ;
        if (posr2 == std::string::npos && posn2 == std::string::npos) {                
            break ;
        } else if (posr2 != std::string::npos && posn2 == std::string::npos) {
            pos2 = posr2 ;
        } else if (posr2 == std::string::npos && posn2 != std::string::npos) {
            pos2 = posn2 ;
        } else if (posr2 != std::string::npos && posn2 != std::string::npos) {
            if (posn2 == posr2 + 1) {
                temp[posr2] = '\n' ;
                temp.erase(posn2, 1) ;
                pos1 = posr2 + 1 ;
                if (temp[pos1] == '\r' && temp[pos1 + 1] == '\n') {
                    temp[pos1] = '\n' ;
                    temp.erase(pos1 + 1, 1) ;
                    return temp ;
                } else if (temp[pos1] == '\n') {
                    return temp ;
                }
                continue ;
            }
            pos2 = posr2 > posn2 ? posn2 : posr2 ;
        }
        temp[pos2] = '\n' ;
        pos1 = pos2 + 1 ;
    }
    return temp ;
}

std::string Header::sanitize(std::string temp) {
    std::string str = this->replaceCRLF(temp) ;
    if (str.find("--\n") == 0) {
        str.erase(0, 3) ;
    }
    if (str.find('\n') == 0) {
        str.erase(0, 1) ;
	}
    if (str.rfind('\n') == str.length() - 1) {
		str.erase(str.rfind("\n"), 1) ;
    }
    return str ;
}


void Header::parseMultiFormData() {
    this->boundary = "--" + this->boundary ;
    size_t pos, start = 0 ;
    std::string query ;

    while ((pos = this->queryString.find(this->boundary, start)) != std::string::npos) {
        if (pos != 0) {
            std::string temp = this->sanitize(this->queryString.substr(start, pos - start)) ;                    
            size_t newPos,
                   tempPos,
                   newStart = temp.find("; ") + 2,
                   lastEnd  = temp.find("\n\n", newStart) ;
            int flag = 0 ;
            std::string mime ;
            if (temp.find("filename", newStart) != std::string::npos &&
			(tempPos = temp.find("Content-Type: ", newStart)) != std::string::npos) {
                mime = temp.substr(tempPos + 14, lastEnd - tempPos - 14) ;
            }

            while ((newPos = temp.find("; ", newStart)) != std::string::npos && newPos < lastEnd) {                
                flag = 1 ;
                size_t start = temp.find("\"", newStart) ;
                size_t stop  = temp.find("\"", start + 1) ;
                std::string word = temp.substr(newStart, start - newStart - 1) ;
                if (word == "filename") {
                    word = "name" ;
                } else if (word == "mime") {
                    word = "type" ;
                }
                this->files += word + "=" + temp.substr(start + 1, stop - start - 1) + "&" ;
                newStart = newPos + 2 ;
            }
            size_t start = temp.find("\"", newStart) ;
            size_t stop  = temp.find("\"", start + 1) ;
            if (flag == 0) {
                query += temp.substr(start + 1, stop - start - 1) + "=" + temp.substr(lastEnd + 2, std::string::npos) + "&" ;
                std::transform(query.begin(), query.end(), query.begin(), [](char ch) {
                    return ch == ' ' ? '+' : ch ;
                }) ;
            } else {
                this->files += "name=" + temp.substr(start + 1, stop - start - 1) + "&" ;
                std::transform(files.begin(), files.end(), files.begin(), [](char ch) {
                    return ch == ' ' ? '+' : ch ;
                }) ;
                std::string tmp_name = "/tmp/" + genRandomStr() ;
                std::string body = temp.substr(lastEnd + 2, std::string::npos) ;
                this->files += "type=" + mime + "&tmp_name=" + tmp_name + "&size=" + std::to_string(body.size()) ;
                FILE* ptr = fopen(tmp_name.c_str(), "wb") ;
                fwrite(body.c_str(), body.size(), 1, ptr) ;
                fclose(ptr) ;
            }
        }
        start = pos + this->boundary.length() ;
    }
    this->queryString = query ;
}


void Header::serviceRequest() {
    if (this->getRequest(0, 1) >= 0) {
        if (this->boundary.empty() == 0) {            
            this->parseMultiFormData() ;
        }

        if (this->status != 400 && this->status != 404 && this->status != 501 && this->status != 304) {
            this->statMsg = "OK" ;
            this->status  = 200 ;
            if ((this->method == "POST" || this->cgi) && this->execCGI() != 0) {
                this->status  = 500 ;
                this->statMsg = "Internal Server Error" ;
                this->errMsg  = "Unable to execute CGI" ;
            }
        }

        this->pushRequest() ;
    }
}


int Header::getRequest(bool body, bool reset) {
    struct timeval tv = {5, 0} ;
    static fd_set fds ;
    while (true) {
        if (reset == 1) {
            this->header.str("") ;
            this->header.clear() ;
            FD_ZERO(&fds) ;
            FD_SET(this->conn, &fds) ;
        }
        if (select(this->conn + 1, &fds, NULL, NULL, &tv) == 1) {           
            if (body == 0) {
                unsigned char buf[BUF_SIZE] ;
                int size ;
                while ((size = recv(this->conn, buf, BUF_SIZE, 0)) > 0) {                   
                    std::string temp = replaceCRLF(std::string(buf, buf + size)) ;
                    size_t pos ;
                    if ((pos = temp.find("\n\n")) != std::string::npos) {
                        if (pos != temp.size() - 2) {
                            this->queryString = temp.substr(pos + 2, std::string::npos) ;
                        }
                        this->header << temp.substr(0, pos) ;                      
                        this->parseHeader() ;
                        if (this->method == "POST" && this->contentLen > this->queryString.size()) {                         
                            return this->getRequest(1, 0) ;
                        }
                        return 0 ;
                    } else {
                        this->header << temp ;
                    }
                }
                if (size <= 0) {
                    perror("Unable to read from socket!") ;
                    return -1 ;
                }
            } else {
                if (this->contentLen == 0) {
                    this->status  = 400 ;
                    this->statMsg = "Bad Request" ;
                    this->errMsg  = "Your client sent HTTP/" + this->version + " request without Content-Length Header" ;
                    return 1 ;
                } else {
                    this->contentLen = this->contentLen - this->queryString.size() ;
                    while (this->contentLen > 0) {
                        int temp ;
                        unsigned char buf[BUF_SIZE] ;
                        if ((temp = recv(this->conn, buf, BUF_SIZE, 0)) > 0) {
                            this->contentLen -= temp ;
                            this->queryString += std::string(buf, buf + temp) ;
                        } else {
                            return -1 ;
                        }
                    }
                    return 0 ;
                }
            }
        } else {
            return -1 ;
        }
    }
}


void Header::parseHeader() {
    // Get the HTTP request line
    std::string word ;
    std::getline(this->header, word) ;
    std::stringstream header(word) ;

    // Get HTTP request method
    if (header >> word && (word == "GET" || word == "HEAD" || word == "POST")) {
        this->method = word ;
    } else {
        //std::cout << "h1" << this->header.str() << "h2" << std::endl ;
        this->status  = 400 ;
        this->statMsg = "Bad Request" ;
        this->errMsg  = "The request line contained invalid characters following the request method string" ;                
        return ;
    }

    // Get the requested resource
    if (header >> word) {
        if (word == "/") {
            this->path.addUrl("/index.html", this->conn) ;
        } else {
            size_t pos ;
            if ((pos = word.find('?')) != std::string::npos) {
                this->queryString = word.substr(pos + 1, std::string::npos) ;
                this->path.addUrl(word.substr(0, pos), this->conn) ;
                this->cgi = 1 ;
            } else {
                this->path.addUrl(word, this->conn) ;
            }
        }
        this->mime = this->path.substring() ;
        if (this->mime.empty() == 0) {
            if (this->mime == "html" || this->mime == "htm") {
                this->mime = "text/html" ;
            } else if (this->mime == "css") {
                this->mime = "text/css" ;
            } else if (this->mime == "js") {
                this->mime = "application/javascript" ;
            } else if (this->mime == "jpeg" || this->mime == "jpg") {
                this->mime = "image/jpeg" ;
            } else if (this->mime == "png") {
                this->mime = "image/png" ;
            } else if (this->mime == "gif") {
                this->mime = "image/gif" ;
            } else if (this->mime == "bmp") {
                this->mime = "image/bmp" ;
            } else if (this->mime == "txt") {
                this->mime = "text/plain" ;
            } else if (this->mime == "php") {
                this->mime = "text/html" ;
                this->cgi = 1 ;
            } else {
                this->status  = 501 ;
                this->statMsg = "Not Implemented" ;
                this->errMsg  = "Support for " + this->mime + "is not yet implemented" ;
                return ;
            }
        } else {
            this->status  = 400 ;
            this->statMsg = "Bad Request" ;
            this->errMsg  = "The request line contained invalid characters following the requested resource string" ;
            return ;        
        }        

        if (this->path.checkResource() < 0) {
            this->status  = 404 ;
            this->statMsg = "Not Found" ;
            this->errMsg  = "The requested URL was not found on this server" ;
            return ;
        }
    } else {
        this->status  = 400 ;
        this->statMsg = "Bad Request" ;
        this->errMsg  = "The request line contained invalid characters following the requested resource string" ;                
        return ;
    }

    // Get HTTP version
    if (header >> word) {
        if (word == "HTTP/1.1") {
            this->version = "1.1" ;
        } else if (word == "HTTP/1.0") {
            this->version = "1.0" ;
        }
    } else {
        this->status  = 400 ;
        this->statMsg = "Bad Request" ;
        this->errMsg  = "The request line contained invalid characters following the protocol string" ;
        return ;
    }

    while (std::getline(this->header, word)) {
        std::stringstream header(word) ;
        header >> word ;

        if (word == "Host:") {
            header >> this->host ;
        } else if (word == "Content-Length:") {
            header >> word ;
            this->contentLen += std::stoi(word) ;
        } else if (word == "Connection:") {
            header >> word ;
            if (word != "keep-alive" && this->version == "1.0") {
                this->connection = "close" ;
            } else if (word == "close" && this->version == "1.1") {
                this->connection = "close" ;
            } else if (word != "close" && this->version == "1.1") {
                this->connection = "keep-alive" ;
            }
        } else if (word == "If-Modified-Since:") {
            std::string caching ;
            std::getline(header, caching) ;
            struct tm t ;
            strptime(caching.c_str(), " %a, %d %B %Y %H:%M:%S", &t) ;
            this->caching = mktime(&t) ;
        } else if (word == "Content-Type:") {
            size_t pos ;
            if ((pos = header.str().find("boundary")) != std::string::npos) {
                this->boundary = header.str().substr(pos + 9, std::string::npos) ;
            }
        }
    }

    if (this->version == "1.1" && this->host.empty()) {
        this->status  = 400 ;
        this->statMsg = "Bad Request" ;
        this->errMsg  = "Your client sent HTTP/1.1 request without hostname" ;
        return ;
    }

    if (this->method != "POST") {
        struct stat st ;
        stat (this->path.getPath().c_str(), &st) ;                        
        struct tm *t = gmtime(&st.st_mtime) ;
        char header[100] ;
        strftime(header, 100, "%a, %d %b %Y %H:%M:%S GMT", t) ;
        this->mtime = header ;         
        time_t sec = mktime(t) ;
        if (sec > this->caching) {
            this->size  = st.st_size ;
        } else {
            this->status  = 304 ;
            this->statMsg = "Not Modified" ;
            this->size    = 0 ;
        }
    }
}


int Header::execCGI() {
    pid_t pid ;
    int cgiOut[2] ;

    if (pipe(cgiOut)) {
        this->status  = 500 ;
        this->statMsg = "Internal Server Error" ;
        this->errMsg  = "Unable to execute CGI" ;
        return -1 ;
    }
    if ((pid = fork()) < 0) {
        this->status  = 500 ;
        this->statMsg = "Internal Server Error" ;
        this->errMsg  = "Unable to execute CGI" ;
        return -1 ;
    } else if (pid == 0) {
        close (cgiOut[0]) ;
        dup2 (cgiOut[1], 1) ;
        if (this->method == "POST") {
            size_t start = this->files.find("=") ;
            size_t stop  = this->files.find("&", start + 1) ;
            std::string fileName = this->files.substr(start + 1, stop - start - 1) ;
            this->files = this->files.substr(stop + 1, std::string::npos) ;
            std::string parse = "parse_str($argv[2], $_POST); parse_str($argv[3], $_FILES['" + fileName + "']); include $argv[1];" ;
            execl ("/usr/bin/php", "php", "-r", parse.c_str(), this->path.getPath().c_str(), this->queryString.c_str(),
                this->files.c_str(), (char*)0) ;
        } else {
            execl ("/usr/bin/php", "php", "-r", "parse_str($argv[2], $_GET); include $argv[1];",
                this->path.getPath().c_str(), this->queryString.c_str(), (char*)0) ;
        }
    } else {
        close (cgiOut[1]) ;

        int status ;
        waitpid(pid, &status, 0) ;

        char c ;
        int i, flag = 0 ;
        this->body.clear() ;
        while (true) {
            if ((i = read(cgiOut[0], &c, 1)) < 0) {
                std::cout << "Unable to read the requested resource!" << std::endl ;
                flag = 1 ;
                break ;
            } else if (i == 0) {
                flag = 0 ;
                break ;
            }
            this->body += c ;
            this->size++ ;
        }
        close (cgiOut[0]) ;

        if (flag) {
            return -1 ;
        } else {
            return 0 ;
        }
    }
}


void Header::pushRequest() {
    if (this->status != 200 && this->status != 304) {
        this->errHeader() ;
    }

    char header[100] ;
    sprintf(header, "HTTP/1.1 %d %s\r\n", this->status, this->statMsg.c_str()) ;
    this->writeHeader(header, strlen(header)) ;

    if (this->connection.empty()) {
        this->writeHeader("Connection: close\r\n", 19) ;
    } else {
        sprintf(header, "Connection: %s\r\n", this->connection.c_str()) ;
        this->writeHeader(header, strlen(header)) ;
    }

    this->writeHeader("Server: Apache/1.3.41 (Unix) mod_perl/1.31\r\n", 44) ;

    time_t t = time (NULL) ;
    struct tm *tm = gmtime (&t) ;
    strftime (header, 100, "Date: %a, %d %b %Y %H:%M:%S GMT\r\n", tm) ;
    this->writeHeader (header, strlen(header)) ;

    if (this->status == 200 && this->cgi == 0) {
        sprintf(header, "Last-Modified: %s\r\n", this->mtime.c_str()) ;
        this->writeHeader(header, strlen(header)) ;
    }

    if (this->method != "HEAD" && this->cgi) {
        sprintf(header, "Content-Length: %lu\r\n", this->body.size()) ;
        this->writeHeader(header, strlen(header)) ;
    } else {
        sprintf(header, "Content-Length: %lu\r\n", this->size) ;
        this->writeHeader(header, strlen(header)) ;
    }

    if (this->mime.empty()) {
        sprintf(header, "Content-Type: text/html; charset=UTF-8\r\n") ;
    } else {
        sprintf(header, "Content-Type: %s; charset=UTF-8\r\n", this->mime.c_str()) ;
    }
    this->writeHeader(header, strlen(header)) ;
    this->writeHeader("\r\n", 2) ;

    if (this->method != "HEAD" && this->status != 304) {
        if (this->status == 200 && this->cgi == 0) {
            this->path.getResource() ;
        } else {
            this->writeHeader(this->body.c_str(), this->body.size()) ;
        }
    }
}


void Header::writeHeader(const char *header, size_t nbytes) {
    size_t offset = 0, sent ;
    while ((sent = send(this->conn, header + offset, nbytes, 0)) > 0 || (sent == -1 && errno == EINTR)) {
        if (sent > 0) {
            offset += sent ;
            nbytes -= sent ;
        }
    }
}


void Header::errHeader() {
    this->errMsg = this->errMsg.empty() ? "The request could not be completed" : this->errMsg ;

    char header[300] ;
    sprintf(header, "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\"\"http://www.w3.org/TR/html4/strict.dtd\">\r\n"
                    "<HTML>\r\n"
                    " <HEAD><TITLE>%d %s</TITLE></HEAD>\r\n"
                    " <BODY>\r\n"
                    "  <H1>%s</H1>\r\n"
                    "  <P>%s</P>\r\n"
                    " </BODY>\r\n"
                    "</HTML>", this->status, this->statMsg.c_str(), this->statMsg.c_str(), this->errMsg.c_str()) ;
    this->body = header ;
}
