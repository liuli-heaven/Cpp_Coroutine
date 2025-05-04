#include <system_error>
#include <cerrno>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <chrono>
#include <fcntl.h>

#include "debug.hpp"
#include "task.hpp"
#include "timer_loop.hpp"
#include "epoll_loop.hpp"
#include "when_any.hpp"
#include "when_all.hpp"
#include "and_then.hpp"

using namespace std::chrono_literals;

mini_async::EpollLoop epoll_loop;
mini_async::TimerLoop timer_loop;

mini_async::Task<std::string> read_string(mini_async::AsyncFile& file){
    co_await wait_file_event(epoll_loop, file, EPOLLIN);
    std::string s;
    size_t chunk = 8;
    while(true){
        char c;
        size_t exist = s.size();
        s.resize(exist + chunk);
        std::span<char> buffer(s.data() + exist, chunk);
        auto len = co_await mini_async::read_file(epoll_loop, file, buffer);
        if(len != chunk){
            s.resize(exist + len);
            break;
        }
        if(chunk < 65536) chunk *= 4;
    }
    co_return s;
}

mini_async::Task<int> async_main(){
    mini_async::AsyncFile file(STDIN_FILENO);
    while(true){
        auto s = co_await read_string(file);
        debug(), "读到了:", s;
        if(s == "quit\n") break;
    }
}

int main(){
    auto t = async_main();
    t.mCoroutine.resume();
    while(!t.mCoroutine.done()){
        auto timeout = timer_loop.run();
        epoll_loop.run(timeout);
    }
    return 0;
}