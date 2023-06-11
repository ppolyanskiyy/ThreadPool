#ifndef _RESULT_H_
#define _RESULT_H_

#include <string>


enum class Result
{
    OK,
    ERROR,
    CANCELED,
    TIMEOUT,
    UNIMPLEMENTED,
    UNDEFINED
};


static Result operator +=(Result & lhs, const Result rhs)
{
    // TODO
    return lhs;
}


static std::string resultToStr(const Result result)
{
    switch (result)
    {
        case Result::OK:            return "RESULT_OK";
        case Result::ERROR:         return "RESULT_ERROR";
        case Result::CANCELED:      return "RESULT_CANCELED";
        case Result::TIMEOUT:       return "RESULT_TIMEOUT";

        default:                    return "UNDEFINED";
    }
}

#endif // _RESULT_H_