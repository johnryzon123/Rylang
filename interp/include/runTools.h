#pragma once
#include <fstream>
#include <sstream>
#include <string>
#include "common.h"

/**
 * Reads the contents of a file and returns it as a string.
 *
 * @throws std::runtime_error if the file could not be opened.
 * @param path the path to the file to be read.
 * @return the contents of the file as a string.
 */
inline std::string readFile(const std::string &path) {
	std::ifstream file(path);
	if (!file.is_open()) {
		throw std::runtime_error("Could not open file: " + path);
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

/// Compares two RyValue objects for equality.
/// Currently supports double, std::string, bool, and std::shared_ptr<std::vector<RyValue>>>
/// (which is used for nested arrays).
/// Will return false for unknown types.
///
inline bool areEqual(const RyValue &a, const RyValue &b) {
	// Nil handling: Two nils are equal.
	if (a.isNil() && b.isNil())
		return true;
	if (a.isNil() || b.isNil())
		return false;

	// If they aren't the same type, they aren't equal.
	if (a.val.index() != b.val.index())
		return false;

	// Content Check: Based on the type
	if (a.isBool())
		return a.asBool() == b.asBool();
	if (a.isNumber())
		return a.asNumber() == b.asNumber();
	if (a.isString())
		return a.asString() == b.asString();

	// List Equality
	if (a.isList()) {
		auto listA = a.asList();
		auto listB = b.asList();

		if (listA->size() != listB->size())
			return false;

		for (size_t i = 0; i < listA->size(); ++i) {
			// Recursive call to check every element
			if (!areEqual((*listA)[i], (*listB)[i]))
				return false;
		}
		return true;
	}

	// 5. Map/Environment Equality
	if (a.isMap()) {
		// Usually, maps are compared by reference or you can
		// implement a key-by-key comparison similar to the list.
		return a.asMap() == b.asMap();
	}

	return false;
}
struct ReturnSignal {
	RyValue value;
	explicit ReturnSignal(RyValue v) : value(std::move(v)) {}
};
