#pragma once

/**
 * @file tools.h
 * @brief Controller/plugin utility functions for dates, host lookup, CLI parsing, and JSON conversion.
 */

#include <string>
#include <string_view>

#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>

std::string date();
std::string GetIPv4FromHostname(const std::string& name);
int ParseCommandLine(int argc, char* argv[], // NOLINT(cppcoreguidelines-avoid-c-arrays)
                     const boost::program_options::options_description& options,
                     boost::program_options::variables_map& vm);

boost::property_tree::ptree to_json(std::string_view s);
std::string to_string(const boost::property_tree::ptree& pt, bool pretty=true);
