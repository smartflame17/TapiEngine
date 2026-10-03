#pragma once

#include "Guid.h"
#include <optional>
#include <string>
#include <vector>
#include <stdexcept>

struct SerializationError
{
	std::string jsonPath = "$";
	std::string message;
	std::string objectName;
	std::optional<Guid> objectId;
	std::string componentType;
	std::optional<Guid> componentId;
};

struct LoadResult
{
	std::vector<SerializationError> errors;
	std::vector<SerializationError> warnings;
	bool Succeeded() const noexcept { return errors.empty(); }
	explicit operator bool() const noexcept { return Succeeded(); }
};

class SerializationException : public std::runtime_error
{
public:
	explicit SerializationException(SerializationError diagnostic)
		: std::runtime_error(diagnostic.jsonPath + ": " + diagnostic.message), diagnostic(std::move(diagnostic)) {}
	SerializationError diagnostic;
};
