#include <assert.h>
#include <exception>

namespace rmixer
{

class Exception : public std::exception
{
public:
  Exception() {};
  Exception(const std::string& m) : msg_(m) {}
  virtual const char* msg() const { return msg_.c_str(); }

private:
  std::string msg_;
};

class FatalException : public Exception
{
public:
  using Exception::Exception;
};

}

//#define RMIXER_ASSERT(x)      assert(x)
#define RMIXER_ASSERT(x)        if (!(x)) { throw rmixer::Exception(); }
#define RMIXER_ASSERT_M(x, msg) if (!(x)) { throw rmixer::Exception(msg); }
#define RMIXER_THROW(msg)       throw rmixer::Exception(msg);
#define RMIXER_FATAL(msg)       throw rmixer::FatalException(msg);