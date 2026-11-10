#include "../network/UniqueFd.h"
#include <cassert>
#include <cerrno>
#include <fcntl.h>
#include <utility>

int main()
{
    int pipes[2];
    assert(pipe(pipes) == 0);
    {
        UniqueFd reader(pipes[0]);
        UniqueFd moved(std::move(reader));
        assert(reader.get() == -1);
        assert(fcntl(moved.get(), F_GETFD) >= 0);
        UniqueFd writer(pipes[1]);
        writer = std::move(moved);
        errno = 0;
        assert(fcntl(pipes[1], F_GETFD) == -1 && errno == EBADF);
        assert(moved.get() == -1);
    }
    errno = 0;
    assert(fcntl(pipes[0], F_GETFD) == -1 && errno == EBADF);
}
