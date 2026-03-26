#include "../../QuicPluginABI.h"

#include "LsquicClientEngine.h"
#include "LsquicCommon.h"
#include "LsquicServerEngine.h"

namespace mediakit {
namespace {

using namespace lsquicplugin;

class LsquicPlugin final : public IQuicPlugin {
public:
    LsquicPlugin() {
        _available = acquireRuntime();
    }

    ~LsquicPlugin() override {
        if (_available) {
            releaseRuntime();
        }
    }

    QuicPluginInfo pluginInfo() const override {
        QuicPluginInfo info;
        info.plugin_name = makeSlice("lsquic-shared-plugin");
        info.has_server = _available;
        info.has_client = _available;
        return info;
    }

    IQuicServerEngine *createServerEngine(IQuicServerHost &host, const QuicServerConfig &config) override {
        if (!_available) {
            host.log(QuicLogLevel::Error, makeSlice("failed to initialize private lsquic runtime"));
            return nullptr;
        }
        return createLsquicServerEngine(host, config);
    }

    void destroyServerEngine(IQuicServerEngine *engine) override {
        delete engine;
    }

    IQuicClientEngine *createClientEngine(IQuicClientHost &host, const QuicClientConfig &config) override {
        if (!_available) {
            host.log(QuicLogLevel::Error, makeSlice("failed to initialize private lsquic runtime"));
            return nullptr;
        }
        return createLsquicClientEngine(host, config);
    }

    void destroyClientEngine(IQuicClientEngine *engine) override {
        delete engine;
    }

private:
    bool _available = false;
};

IQuicPlugin *createPluginInstance() {
    return new LsquicPlugin();
}

} // namespace
} // namespace mediakit

extern "C" {

ZLM_QUIC_PLUGIN_API uint32_t zlm_quic_plugin_abi_version(void) {
    return mediakit::kQuicPluginABIVersion;
}

ZLM_QUIC_PLUGIN_API mediakit::IQuicPlugin *zlm_quic_plugin_create(void) {
    return mediakit::createPluginInstance();
}

ZLM_QUIC_PLUGIN_API void zlm_quic_plugin_destroy(mediakit::IQuicPlugin *plugin) {
    delete plugin;
}

}
