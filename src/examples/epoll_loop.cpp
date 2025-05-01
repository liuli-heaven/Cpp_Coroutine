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
#include "when_any.hpp"
#include "when_all.hpp"
#include "and_then.hpp"

using namespace std::chrono_literals;

namespace mini_async{
    struct EpollFileAwaiter;
    auto checkError(auto res, std::source_location const &loc = std::source_location::current()){
        //把C语言错误码转换为C++异常
        if(res == -1)[[unlikely]]{
            throw std::system_error(errno, std::system_category(), (std::string)loc.file_name() + ":" + std::to_string(loc.line()));
        }
        return res;
    }

    struct EpollFilePromise : Promise<uint32_t>{
        auto get_return_object() {
            return std::coroutine_handle<EpollFilePromise>::from_promise(*this);
        }

        inline ~EpollFilePromise();

        EpollFilePromise& operator=(EpollFilePromise&&)  = delete;
        
        struct EpollFileAwaiter* mAwaiter;
    };

    struct EpollLoop{
        void addListener(EpollFilePromise& promise);
        void removeListener(int fileNo);
        void tryRun(std::optional<std::chrono::system_clock::duration> timeout);

        EpollLoop& operator=(EpollLoop&&) = delete;

        ~EpollLoop(){
            close(mEpoll);
        }

        int mEpoll = checkError(epoll_create1(0));
        struct epoll_event mEventBuf[64];
    };

    struct EpollFileAwaiter{
        bool await_ready() const noexcept{
            return false;
        }

        void await_suspend(std::coroutine_handle<EpollFilePromise> coroutine) {
            auto& promise = coroutine.promise();
            promise.mAwaiter = this;
            mLoop.addListener(promise);
        }

        uint32_t await_resume() const noexcept{
            return mResumeEvents;
        }

        using ClockType = std::chrono::system_clock;

        EpollLoop& mLoop;
        int mFileNo;
        uint32_t mEvents;
        uint32_t mResumeEvents;
    };

    EpollFilePromise::~EpollFilePromise(){
        if(mAwaiter)[[unlikely]]
        {
            mAwaiter->mLoop.removeListener(mAwaiter->mFileNo);
        }
    }

    void EpollLoop::addListener(EpollFilePromise& promise){
        struct epoll_event event{0};
        event.events = promise.mAwaiter->mEvents;
        event.data.ptr = &promise;
        checkError(epoll_ctl(mEpoll, EPOLL_CTL_ADD, promise.mAwaiter->mFileNo, &event));
    }
    void EpollLoop::removeListener(int fileNo){
        checkError(epoll_ctl(mEpoll, EPOLL_CTL_DEL, fileNo, NULL));
    }
    void EpollLoop::tryRun(std::optional<std::chrono::system_clock::duration> timeout){
        int timeoutInMs = -1;
        if(timeout)
        {
            timeoutInMs = std::chrono::duration_cast<std::chrono::milliseconds>(*timeout).count();
        }
        int res = checkError(epoll_wait(mEpoll, mEventBuf, std::size(mEventBuf), timeoutInMs));
        for(int i = 0; i < res; i++)
        {
            auto& event = mEventBuf[i];
            auto& promise = *(EpollFilePromise*)event.data.ptr;
            promise.mAwaiter->mResumeEvents = event.events;
        }
        for(int i = 0; i < res; i++)
        {
            auto& event = mEventBuf[i];
            auto& promise = *(EpollFilePromise*)event.data.ptr;
            std::coroutine_handle<EpollFilePromise>::from_promise(promise).resume();
        }
    }

    inline Task<uint32_t, EpollFilePromise>
    wait_file(EpollLoop& loop, int fileNo, uint32_t events){
        auto resumeEvents =  co_await EpollFileAwaiter(loop, fileNo, events | EPOLLONESHOT);
        co_return resumeEvents;
    }
}

mini_async::EpollLoop loop;
mini_async::TimerLoop timer_loop;

mini_async::Task<std::string> reader(int fileNo){
    using namespace mini_async;
    
    co_await wait_file(loop, fileNo, EPOLLIN);
    std::string s;
    size_t chunk = 8;

    while(true){
        char c;
        size_t exist = s.size();
        s.resize(exist + chunk);
        ssize_t len = read(fileNo, s.data() + exist, chunk);
        if(len == -1)
        {
            if(errno != EWOULDBLOCK)[[unlikely]]{
                throw std::system_error(errno, std::system_category());
            }
            break;
        }
        if(len != chunk){
            s.resize(exist + len);
            break;
        }
        if(chunk < 65535) chunk *= 4;
    }
    co_return s;
}

mini_async::Task<int> async_main(){
    int file = mini_async::checkError(open("/dev/stdin", O_RDONLY | O_NONBLOCK));
    while (true)
    {
        debug(), "开始读取";
        auto v = co_await when_any(reader(STDIN_FILENO), reader(file));
        std::string data;
        std::visit([&](const std::string& v){ data = v; }, v);
        debug(), "读到了:", data;
        if(data == "quit\n") break;
    }
}

int main(){
    int attr = 1;
    ioctl(0, FIONBIO, &attr);

    auto t = async_main();
    t.mCoroutine.resume();
    while(!t.mCoroutine.done()){
        auto timeout = timer_loop.run();
        loop.tryRun(timeout);
    }
    return 0;
}