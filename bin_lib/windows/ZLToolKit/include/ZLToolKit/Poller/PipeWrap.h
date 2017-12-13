#ifndef PipeWarp_h
#define PipeWarp_h

namespace ZL {
namespace Poller {

class PipeWrap {
public:
	PipeWrap();
	~PipeWrap();
	int write(const void *buf, int n);
	int read(void *buf, int n);
	int readFD() const {
		return _pipe_fd[0];
	}
	int writeFD() const {
		return _pipe_fd[1];
	}
private:
	int _pipe_fd[2] = { -1,-1 };
	void clearFD();
#if defined(_WIN32)
	int _listenerFd = -1;
#endif // defined(_WIN32)

		};

} /* namespace Poller */
} /* namespace ZL */

#endif // !PipeWarp_h

