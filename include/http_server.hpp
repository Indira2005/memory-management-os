#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

class HttpServer {
public:
    explicit HttpServer(int port);
    void run();

private:
    int port_;
};

#endif
