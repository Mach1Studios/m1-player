#include "FFmpegHelpers.h"

std::string FFmpegHelpers::avErrorToString(int error_number)
{
       char errstr[AV_ERROR_MAX_STRING_SIZE] = { 0 };
       av_make_error_string(errstr, AV_ERROR_MAX_STRING_SIZE, error_number);
       return std::string(errstr);
}
