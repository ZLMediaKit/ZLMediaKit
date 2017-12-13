//
//  Pipe.h
//  xzl
//
//  Created by xzl on 16/4/13.
//

#ifndef Pipe_h
#define Pipe_h

#include <stdio.h>
#include <functional>
#include "PipeWrap.h"
#include "EventPoller.h"

using namespace std;

namespace ZL {
namespace Poller {

class Pipe
{
public:
    Pipe(function<void(int size,const char *buf)> &&onRead=nullptr);
    virtual ~Pipe();
    void send(const char *send,int size=0);
private:
	std::shared_ptr<PipeWrap> _pipe;
};


}  // namespace Poller
}  // namespace ZL


#endif /* Pipe_h */
