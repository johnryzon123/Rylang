#include "common.h"
#include "class.h"

bool RyValue::isClass() const {
	if (!isFunction())
		return false;
	return std::dynamic_pointer_cast<Frontend::RyClass>(asFunction()) != nullptr;
}

	std::string RyValue::toString() const {
		if (isNil())
			return "nil";
		if (isNumber()) {
			// Remove trailing zeros for a cleaner look
			std::string s = std::to_string(asNumber());
			s.erase(s.find_last_not_of('0') + 1, std::string::npos);
			if (s.back() == '.')
				s.pop_back();
			return s;
		}
		if (isBool())
			return asBool() ? "true" : "false";
		if (isString())
			return asString();
		if (isList())
			return "[list]";
		if (isMap())
			return "[map]";
		if (isFunction())
			return "[func]";
		if (isInstance())
			return "[instance]";
		return "[unknown]";
	}