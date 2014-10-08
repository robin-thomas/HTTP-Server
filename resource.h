#ifndef _RESOURCE_H_
#define _RESOURCE_H_

class Resource {
    private:
        int conn ;
        int resource ;
        std::string path ;
    public:
        void addUrl(std::string path, int conn) ;
        int  checkResource() ;
        void decodeUrl() ;
        void trimUrl() ;
        void getResource() ;
        std::string substring() ;
        std::string getPath() ;
};

#endif
