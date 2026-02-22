#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "common.h"
#include "env.h"
#include "func.h"
#include "runtime.h"

namespace Frontend {

	// Forward Declarations
	class RyClass;
	class RyInstance;

	// RyInstance Definition
	// Represents a specific object created from a class
	class RyInstance : public std::enable_shared_from_this<RyInstance> {
		std::shared_ptr<RyClass> klass;
		std::unordered_map<std::string, RyVariable> fields;

	public:
		RyInstance(std::shared_ptr<RyClass> klass);

		RyValue get(const Token &name);
		std::shared_ptr<RyClass> getClass() { return klass; }
		RyVariable getVariable(const Token &name);
		void set(const Token &name, RyVariable var);
	};

	// RyClass Definition
	// Acts as the blueprint for creating instances
	class RyClass : public RyCallable, public std::enable_shared_from_this<RyClass> {
	public:
		std::string name;
		std::unordered_map<std::string, std::shared_ptr<RyFunction>> methods;
		std::unordered_map<std::string, RyVariable> fieldBlueprints;
		std::shared_ptr<RyClass> superclass;


		RyClass(std::string name, std::unordered_map<std::string, std::shared_ptr<RyFunction>> methods,
						std::unordered_map<std::string, RyVariable> fieldBlueprints, std::shared_ptr<RyClass> superclass);

		int arity() override;
		RyValue call(Interpreter &interp, std::vector<RyValue> args) override;

		std::shared_ptr<RyFunction> findMethod(const std::string &name);

		std::string toString() override { return "<class " + name + ">"; }
		std::shared_ptr<RyFunction> bind(std::shared_ptr<RyInstance> instance) override { return nullptr; }
	};
} // namespace Frontend
