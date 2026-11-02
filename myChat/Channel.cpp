#include "Channel.h"
#include <unistd.h>

Channel::~Channel() = default;

void Channel::closeFd()
{
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
}
