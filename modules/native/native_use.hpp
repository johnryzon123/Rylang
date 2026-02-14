#pragma once
#include <iostream>
#include <memory>
#include "loader.h"
#include "native.hpp"
#include "runtime.h"

namespace Frontend {
	class RyUse : public RyCallable {
	public:
		int arity() override { return 1; }

		RyValue call(Interpreter &i, std::vector<RyValue> args) override {
			std::string libName = args[0].asString();

			// Create the module environment
			auto globalsPtr = i.getGlobals();
			auto moduleEnv = std::make_shared<Backend::Environment>(globalsPtr);

			// Load Cross-platform loader
			LibHandle handle = Backend::RyLoader::open(libName);

			if (!handle) {
				std::cerr << "Ry Library Error: " << Backend::RyLoader::getError() << std::endl;
				return moduleEnv;
			}

			typedef void (*InitFunc)(RegisterFn, void *);
			InitFunc init_module = (InitFunc) Backend::RyLoader::getSymbol(handle, "register_ry_module");

			if (init_module) {
				init_module(register_callback, moduleEnv.get());
			} else {
				std::cerr << "Ry Symbol Error: " << Backend::RyLoader::getError() << std::endl;
			}

			return moduleEnv;
		}
		std::shared_ptr<RyFunction> bind(std::shared_ptr<RyInstance> instance) override {
			// Native functions aren't methods, so they don't bind to instances.
			return nullptr;
		}

		std::string toString() override { return "<native use fn>"; }
	};
} // namespace Frontend
