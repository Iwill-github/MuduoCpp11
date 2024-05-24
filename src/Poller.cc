#include "Poller.h"


Poller::Poller(EventLoop *loop)
    : owerLoop_(loop)
{ }


bool Poller::hasChannel(Channel* channel) const {
    auto it = channels_.find(channel->getFd());
    return it != channels_.end() && it->second == channel;
}
