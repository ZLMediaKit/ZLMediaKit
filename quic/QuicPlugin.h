#ifndef ZLMEDIAKIT_QUICPLUGIN_H
#define ZLMEDIAKIT_QUICPLUGIN_H

#include <string>

#include "QuicPluginABI.h"

namespace mediakit {

class QuicPluginRef {
public:
    QuicPluginRef() = default;
    explicit QuicPluginRef(IQuicPlugin *plugin) : _plugin(plugin) {}

    bool valid() const {
        return _plugin != nullptr;
    }

    uint32_t abiVersion() const {
        return valid() ? _plugin->pluginInfo().abi_version : 0;
    }

    std::string pluginName() const {
        if (!valid()) {
            return std::string();
        }
        auto info = _plugin->pluginInfo();
        if (!info.plugin_name.data) {
            return std::string();
        }
        return std::string(info.plugin_name.data, info.plugin_name.len);
    }

    bool hasServer() const {
        return valid() && _plugin->pluginInfo().has_server;
    }

    bool hasClient() const {
        return valid() && _plugin->pluginInfo().has_client;
    }

    IQuicPlugin *raw() const {
        return _plugin;
    }

    IQuicServerEngine *createServerEngine(IQuicServerHost &host, const QuicServerConfig &config) const {
        return _plugin ? _plugin->createServerEngine(host, config) : nullptr;
    }

    void destroyServerEngine(IQuicServerEngine *engine) const {
        if (_plugin) {
            _plugin->destroyServerEngine(engine);
        }
    }

    IQuicClientEngine *createClientEngine(IQuicClientHost &host, const QuicClientConfig &config) const {
        return _plugin ? _plugin->createClientEngine(host, config) : nullptr;
    }

    void destroyClientEngine(IQuicClientEngine *engine) const {
        if (_plugin) {
            _plugin->destroyClientEngine(engine);
        }
    }

private:
    IQuicPlugin *_plugin = nullptr;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_QUICPLUGIN_H
