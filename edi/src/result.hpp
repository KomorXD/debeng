#ifndef RESULT_HPP
#define RESULT_HPP

enum class GenericError {
    SUCCESS,
    FAILURE
};

/*  Basic result type, VALUE should only be accessed if ERROR is NONE (or any
    equivalent). */
template <typename ResultType, typename ErrorType = GenericError>
struct Result {
    ResultType value{};
    ErrorType error{};
};

#endif // RESULT_HPP
