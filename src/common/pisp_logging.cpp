#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/trivial.hpp>

namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;
namespace keywords = boost::log::keywords;
namespace trivial = boost::log::trivial;

#include "pisp_logging.h"

using namespace PiSP;

static boost::shared_ptr<sinks::synchronous_sink<sinks::text_ostream_backend>> console;

void PiSP::logging_init(void)
{
    logging::add_common_attributes();

    logging::formatter format =
        expr::format("[%1%] %2%")
        % expr::attr<trivial::severity_level>("Severity")
        % expr::smessage;

    /* console sink */
    console = logging::add_console_log(std::clog);
    console->set_formatter(format);
    console->set_filter(trivial::severity >= trivial::info);

    /* fs sink */
    auto fs_sink = logging::add_file_log(
         keywords::file_name = "log.txt",
         keywords::rotation_size = 1 * 1024 * 1024,
         keywords::open_mode = std::ios_base::trunc,
         keywords::filter = trivial::severity >= trivial::debug);
    fs_sink->set_formatter(format);
    fs_sink->locked_backend()->auto_flush(true);
}
