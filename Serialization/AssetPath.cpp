#include "AssetPath.h"
#include <stdexcept>

namespace
{
bool Outside(const std::filesystem::path& relative)
{
	return relative.empty() || relative.is_absolute() || relative.has_root_name() ||
		*relative.begin() == "..";
}
}

std::filesystem::path AssetPath::Resolve(const std::string& reference, const std::filesystem::path& projectRoot)
{
	if (reference.empty() || reference.find('\0') != std::string::npos)
		throw std::invalid_argument("An asset reference must be a non-empty project-relative path.");
	const auto path = FromUtf8(reference);
	if (path.has_root_path() || path == ".")
		throw std::invalid_argument("An asset reference must be project-relative.");
	// Reference normalization must not touch disk: an injected asset loader may
	// resolve a virtual reference for which no physical file exists.
	const auto root = std::filesystem::absolute(projectRoot).lexically_normal();
	const auto resolved = (root / path).lexically_normal();
	const auto relative = resolved.lexically_relative(root);
	if (Outside(relative) || relative == ".")
		throw std::invalid_argument("Asset path is outside the project root.");
	return resolved;
}

std::string AssetPath::Reference(const std::filesystem::path& source, const std::filesystem::path& projectRoot)
{
	if (source.empty()) return {};
	const auto root = std::filesystem::absolute(projectRoot).lexically_normal();
	// Relative authored paths are relative to the project, not the scene file.
	const auto absolute = (source.is_absolute() ? source : root / source).lexically_normal();
	const auto relative = absolute.lexically_relative(root);
	if (Outside(relative) || relative == ".")
		throw std::invalid_argument("Asset path is outside the project root.");
	return ToUtf8(relative);
}
