#pragma once

#include "../../QuicPluginABI.h"

namespace mediakit {

IQuicClientEngine *createLsquicClientEngine(IQuicClientHost &host, const QuicClientConfig &config);

} // namespace mediakit
