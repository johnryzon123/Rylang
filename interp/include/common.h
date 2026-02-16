#ifndef RY_COMMON_H
#define RY_COMMON_H

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace Backend {
	// Forward Declarations
	class Stmt;
	class Expr;
	class Token;
	class ExprVisitor;
	class StmtVisitor;
	class Environment;
} // namespace Backend

namespace Frontend {
	class Interpreter;
	class RyCallable;
	class RyFunction;
	class RyInstance;
	class RyClass;

	class RyOut;
	class RyInput;
	class RyLen;
	class RyPop;
	class RyExit;
	class RyUse;
	class ExternalNative;
} // namespace Frontend

class Resolver;

// Global configurations
struct RyValue {
	using List = std::shared_ptr<std::vector<RyValue>>;
	using Map = std::shared_ptr<Backend::Environment>;
	using Func = std::shared_ptr<Frontend::RyCallable>;
	using Instance = std::shared_ptr<Frontend::RyInstance>;

	using Variant = std::variant<std::monostate, double, bool, std::string, List, Map, Func, Instance>;

	Variant val;

	RyValue() : val(std::monostate{}) {}
	RyValue(double d) : val(d) {}
	RyValue(bool b) : val(b) {}
	RyValue(std::string s) : val(s) {}
	RyValue(const char *s) : val(std::string(s)) {}
	RyValue(List l) : val(l) {}
	RyValue(Map m) : val(m) {}
	RyValue(Func f) : val(f) {}
	RyValue(Instance i) : val(i) {}
	RyValue(std::nullptr_t) : val(std::monostate{}) {}

	bool isNil() const { return std::holds_alternative<std::monostate>(val); }
	bool isNumber() const { return std::holds_alternative<double>(val); }
	bool isBool() const { return std::holds_alternative<bool>(val); }
	bool isString() const { return std::holds_alternative<std::string>(val); }
	bool isList() const { return std::holds_alternative<List>(val); }
	bool isMap() const { return std::holds_alternative<Map>(val); }
	bool isFunction() const { return std::holds_alternative<Func>(val); }
	bool isInstance() const { return std::holds_alternative<Instance>(val); }
	bool isClass() const;

	double asNumber() const { return std::get<double>(val); }
	bool asBool() const { return std::get<bool>(val); }
	std::string asString() const { return std::get<std::string>(val); }
	List asList() const { return std::get<List>(val); }
	Map asMap() const { return std::get<Map>(val); }
	Func asFunction() const { return std::get<Func>(val); }
	Instance asInstance() const { return std::get<Instance>(val); }

	std::string toString() const;
	bool operator==(const RyValue &other) const { return val == other.val; }

	bool operator!=(const RyValue &other) const { return val != other.val; }

};

#endif
