#ifndef JSON_UTILS_HPP
#define JSON_UTILS_HPP

#include <string>

#include "models.hpp"

std::string to_json(const AllocationResult& result);
std::string to_json(const DemandPagingResult& result);
std::string to_json(const MultiLevelPagingResult& result);
std::string to_json(const PagingResult& result);
std::string to_json(const SegmentationResult& result);
std::string to_json(const ThrashingResult& result);
std::string to_json(const TranslationResult& result);
std::string to_json(const TlbTranslationResult& result);
std::string json_escape(const std::string& value);

#endif
