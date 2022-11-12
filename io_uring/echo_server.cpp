#include <arpa/inet.h>
#include <bits/stdc++.h>
#include <liburing.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>


const int BUFSIZE = 1024;

struct ioreq {
    enum STATE { ACCEPT, READ, WRITE };
    int fd;
    STATE state;
    union {
        struct {
            sockaddr_in ip;
            socklen_t len;
        } addr;
        char buf[BUFSIZE];
    };
};


class net_uring {
    io_uring ring;
public:
    net_uring(int queue_size) { 
        io_uring_queue_init(queue_size, &ring, 0); 
    }

    ~net_uring() { 
        io_uring_queue_exit(&ring); 
    }

    void seen(io_uring_cqe* cqe) { 
        io_uring_cqe_seen(&ring, cqe); 
    }

    int wait(io_uring_cqe** cqe) { 
        return io_uring_wait_cqe(&ring, cqe); 
    }

    int submit() { 
        return io_uring_submit(&ring); 
    }

    void accpet_async(int svr_fd, ioreq* body) {
        body->state = ioreq::ACCEPT;
        body->fd = svr_fd;
        body->addr.len = sizeof(sockaddr_in);

        io_uring_sqe* sqe = io_uring_get_sqe(&ring);
        io_uring_prep_accept(sqe, svr_fd, (sockaddr*)&(body->addr.ip), &(body->addr.len), 0);
        io_uring_sqe_set_data(sqe, body);
    }

    void read_async(int client_fd, ioreq* body) {
        body->state = ioreq::READ;
        body->fd = client_fd;

        io_uring_sqe* sqe = io_uring_get_sqe(&ring);
        io_uring_prep_read(sqe, client_fd, body->buf, sizeof(body->buf), -1);
        io_uring_sqe_set_data(sqe, body);
    }

    void write_async(int client_fd, ioreq* body) {
        body->state = ioreq::WRITE;
        body->fd = client_fd;

        io_uring_sqe* sqe = io_uring_get_sqe(&ring);
        io_uring_prep_write(sqe, client_fd, body->buf, sizeof(body->buf), -1);
        io_uring_sqe_set_data(sqe, body);
    }
};


//implement as echo server
class server {
    const int port_;
    int svr_fd_;
    net_uring ring_;
public:
    server(int port):
        port_(port),
        ring_(1024) {
        fd_ = socket(AF_INET, SOCK_STREAM, 0);

        sockaddr_in sock_addr;
        {
            sock_addr.sin_port = htons(port_);
            sock_addr.sin_family = AF_INET;
            sock_addr.sin_addr.s_addr = INADDR_ANY;
        }

        int ret = bind(fd_, (sockaddr*)&sock_addr, sizeof(sock_addr));
        listen(fd_, 32);
    }

    int run(){
        ring.accpet_async(svr_fd_, new ioreq);
        ring.submit();
        while (true) {
            io_uring_cqe* cqe;
            ring.wait(&cqe);
            ioreq* res = (ioreq*)cqe->user_data;
            switch (res->state) {
                case ioreq::ACCEPT:
                    if (cqe->res > 0) {
                        int client_fd = cqe->res;
                        ring.accpet_async(sock_fd, res);
                        ring.read_async(client_fd, new ioreq);
                        ring.submit();
                    }
                    std::cout << cqe->res << std::endl;
                    break;
                case ioreq::READ:
                    if (cqe->res > 0) { 
                        std::cout << res->buf << std::endl;
                    }
                    //TODO: how to read continously
                    ring.write_async(res->fd, res); 
                    ring.submit();
                    break;
                case ioreq::WRITE:
                    if (cqe->res > 0) {
                        close(res->fd);
                        delete res;
                    }
                    break;
                default:
                    std::cout << "error " << std::endl;
                    break;
            }
            ring.seen(cqe);
        }
    }
}

int main() {
    server svr(8000);
    svr.run();
    return 0;
}
