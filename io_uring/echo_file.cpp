#include <iostream>
#include <bits/stdc++.h>
#include <liburing.h>
#include <unistd.h>

char buf[1024] = {0};

int echo(int fd) {
    io_uring ring;
    io_uring_queue_init(32, &ring, 0);

    io_uring_cqe *sqe = io_uring_get_sqe(&ring);
    io_uring_prep_read(sqe, fd, buf, sizeof(buf), 0);
    io_uring_submit(&ring);

    io_uring_cqe *res;
    io_uring_wait_cqe(&ring, &res);

    assert(res);
    std::cout << "read bytes: " << res->res << " \n";
    std::cout << buf << std::endl;
    io_uring_cqe_seen(&ring, res);

    io_uring_queue_exit(&ring);
    return 0;
}

int main(const char **argc, int argv){
    if(argv<2){
        std::cout << "usage: echo_file {path}" << std::endl;
        exit(-1);
    }
    const char *path = argc[1];
    int fd = open(path, O_RDONLY, 0);
    echo(argc[1]);
    return 0;
}
