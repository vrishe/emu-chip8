#pragma once

namespace std {

    template <class T> typename std::decay<T>::type decay_copy(T&& v)
    {
        return std::forward<T>(v);
    }
}