#pragma once

/**
 * @file tools.h
 * @brief Controller/plugin utility functions for dates, host lookup, CLI parsing, and JSON conversion.
 */

#include <string>
#include <string_view>

#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>

/** @brief Return the current local timestamp string used by plugin logs and metadata. */
std::string date();
/** @brief Resolve an IPv4 address for a host name, returning an empty string on failure. */
std::string GetIPv4FromHostname(const std::string& name);
/** @brief Parse command-line options into a Boost variables_map. */
int ParseCommandLine(int argc, char* argv[], // NOLINT(cppcoreguidelines-avoid-c-arrays)
                     const boost::program_options::options_description& options,
                     boost::program_options::variables_map& vm);

/** @brief Parse a JSON string into a Boost property tree. */
boost::property_tree::ptree to_json(std::string_view s);
/** @brief Serialize a Boost property tree to JSON text. */
std::string to_string(const boost::property_tree::ptree& pt, bool pretty=true);
