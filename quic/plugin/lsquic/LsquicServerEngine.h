#pragma once

#include "../../QuicPluginABI.h"

namespace mediakit {

IQuicServerEngine *createLsquicServerEngine(IQuicServerHost &host, const QuicServerConfig &config);

} // namespace mediakit
