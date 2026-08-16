- Read @README.md for project context
- Use light comments. Public interfaces and complex things should have comments,
  but don't comment everything.

# Python Instructions
- Use ty for validation, dataclasses for structs, and polars for tables
- When working with more than ~15MB of data, YOU MUST use polars and polars native operations.
  Never use heavy json.loads or for loops for processing large datasets when polars alternatives exist.
- Keep dependencies light, but do use industry standard libraries

# Terraform/Tofu instructions
- Use open tofu
- Never apply or plan yourself

# C Instructions
- All function declarations in headers should have doc comments.
- Break code into modules (module.h/module.c) that do one thing each
- Each module should have it's own error enum
- Follow esp idf conventions
- Use logging. When an error is returned, a log should occure
- Avoid allocation when possible
- Think about syncronization and parrallelism explicitly
- This is a battery powered device
- Use const whenever possible
- Write modern C11/C23
- For pure-logic module, write unit tests in `host_tests`
- Never write unit tests for drivers or the that directly touches hardware, only modules that don't rely on hardware
- Do structure code so that has much as reasonable can be tested

