#pragma once
#include <random>

#include <util/singleton.hpp>

namespace util::rng {
    class Random : public SingletonBase<Random> {
    protected:
        friend class SingletonBase;
        Random();

    public:
        std::mt19937_64& getEngine();

        template <typename T>
        T generate();

        // Generate a number `[0, max]`
        template <typename T>
        T generate(T max);

        // Generate a number `[min, max]`
        template <typename T>
        T generate(T min, T max);

        // Has a `(numerator/denominator)` chance of returning true.
        bool genRatio(uint32_t numerator, uint32_t denominator);

        // Has a `ratio%` chance of returning true. 1.0f is 100%, 0.0f is 0%
        bool genRatio(float ratio);
        // Has a `ratio%` chance of returning true. 1.0 is 100%, 0.0 is 0%
        bool genRatio(double ratio);

        std::string genString(const std::string_view alphabet, size_t size);

        template <typename T>
        void fill(T* ptr, size_t count) {
            for (size_t i = 0; i < count; i++) {
                ptr[i] = this->generate<T>();
            }
        }

    private:
        std::mt19937_64 engine;
    };
}