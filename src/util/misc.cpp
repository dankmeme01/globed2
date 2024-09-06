#include "misc.hpp"
#include <data/types/game.hpp>
#include <data/types/gd.hpp>

#include <asp/sync.hpp>
#include <util/simd.hpp>
#include <util/lowlevel.hpp>
#include <util/crypto.hpp>

#ifdef GLOBED_DEBUG
# include <util/time.hpp>
# include <util/format.hpp>
#endif

namespace util::misc {
    bool swapFlag(bool& target) {
        bool state = target;
        target = false;
        return state;
    }

    void callOnce(const char* key, std::function<void()> func) {
        static std::unordered_set<const char*> called;

        if (called.contains(key)) {
            return;
        }

        called.insert(key);
        func();
    }

    void callOnceSync(const char* key, std::function<void()> func) {
        static asp::Mutex<> mtx;

        auto guard = mtx.lock();
        callOnce(key, func);
    }

    float calculatePcmVolume(const float* pcm, size_t samples) {
        return simd::calcPcmVolume(pcm, samples);
    }

    float pcmVolumeSlow(const float* pcm, size_t samples) {
        double sum = 0.0f;
        for (size_t i = 0; i < samples; i++) {
            sum += static_cast<double>(std::abs(pcm[i]));
        }

        return static_cast<float>(sum / static_cast<double>(samples));
    }

    bool compareName(const std::string_view nv1, const std::string_view nv2) {
        std::string name1(nv1);
        std::string name2(nv2);

        std::transform(name1.begin(), name1.end(), name1.begin(), ::tolower);
        std::transform(name2.begin(), name2.end(), name2.begin(), ::tolower);

        return name1 < name2;
    }

    bool isEditorCollabLevel(LevelId levelId) {
        const LevelId bound = std::pow(2, 32);

        return levelId > bound;
    }

    ScopeGuard::~ScopeGuard() {
        f();
    }

    ScopeGuard scopeDestructor(const std::function<void()>& f) {
        return ScopeGuard(f);
    }

    ScopeGuard scopeDestructor(std::function<void()>&& f) {
        return ScopeGuard(std::move(f));
    }

    UniqueIdent::operator std::string() const {
        return this->getString();
    }

    std::array<uint8_t, 32> UniqueIdent::getRaw() const {
        return rawForm;
    }

    std::string UniqueIdent::getString() const {
        return util::crypto::hexEncode(rawForm.data(), rawForm.size());
    }

    const UniqueIdent& fingerprint() {
        static auto fingerprint = []{
#ifdef GLOBED_DEBUG
            auto now = util::time::now();
            auto _ = util::misc::scopeDestructor([now] {
                log::debug("Fingerprint computation took {}", util::format::duration(util::time::now() - now));
            });
#endif

            auto res = fingerprintImpl();

            if (!res) {
                log::error("Failed to compute fingerprint: {}", res.unwrapErr());

                // use a static predetermined value
                auto data = util::crypto::hexDecode("fecc8b3da5e8ed1b9dcd66f6213b6f891416c4c5f957a4a6d0c7fef540a5f05b").unwrap();
                std::array<uint8_t, 32> arr;
                std::copy_n(data.begin(), 32, arr.begin());

                return UniqueIdent(arr);
            }

            return res.unwrap();
        }();
        return fingerprint;
    }
}
