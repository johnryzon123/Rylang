#include "class.h"
#include "func.h"
#include "tools.h"

using namespace Frontend;

// Define the RyInstance constructor
RyInstance::RyInstance(std::shared_ptr<RyClass> klass) : klass(klass) {
	for (auto const &[name, variable]: klass->fieldBlueprints) {
		fields[name] = variable;
	}
}

// Define the RyClass constructor
RyClass::RyClass(std::string name, std::unordered_map<std::string, std::shared_ptr<RyFunction>> methods,
								 std::unordered_map<std::string, RyVariable> fieldBlueprints, std::shared_ptr<RyClass> superclass) :
		name(std::move(name)), methods(std::move(methods)), fieldBlueprints(std::move(fieldBlueprints)),
		superclass(std::move(superclass)) {
	if (this->superclass) {
		for (auto const &[key, val]: this->superclass->fieldBlueprints) {
			if (!this->fieldBlueprints.count(key)) {
				this->fieldBlueprints[key] = val;
			}
		}
	}
}

void RyInstance::set(const Token &name, RyVariable var) { fields[name.lexeme] = var; }

RyVariable RyInstance::getVariable(const Token &name) {
	// Check for Instance Fields (Data)
	if (fields.count(name.lexeme)) {
		return fields.at(name.lexeme);
	}

	// Check for Methods
	auto method = klass->findMethod(name.lexeme);
	if (method) {
		RyValue boundMethod = std::static_pointer_cast<RyCallable>(method->bind(shared_from_this()));
		return RyVariable(boundMethod, method->getPrivate(), std::nullopt);
	}

	// Not Found
	throw RyTools::RyRuntimeError(name, "Undefined property '" + name.lexeme + "'.");
}

int RyClass::arity() {
	auto initializer = findMethod("init");
	if (initializer)
		return initializer->arity();
	return 0;
}

std::shared_ptr<RyFunction> RyClass::findMethod(const std::string &name) {
	if (methods.count(name)) {
		return methods.at(name);
	}
	if (superclass) {
		return superclass->findMethod(name);
	}
	return nullptr;
}


RyValue RyClass::call(Interpreter &interp, std::vector<RyValue> args) {
	auto instance = std::make_shared<RyInstance>(shared_from_this());


	// Run the 'init' method if it exists
	auto initializer = findMethod("init");
	if (initializer) {
		initializer->bind(instance)->call(interp, args);
	}

	return RyValue(instance);
}
