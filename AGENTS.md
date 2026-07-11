- Read @README.md for project context
- Use light comments. Public interfaces and complex things should have comments,
  but don't comment everything.

# Python Instructions
- Use pylance for validation, dataclasses for structs, and polaris for tables
- Keep dependencies light, but do use industry standard libraries

# Terraform/Tofu instructions
- Use open tofu
- Never apply or plan yourself

# C Instructions
- Break code into modules (module.h/module.c) that do one thing each
- Each module should have it's own error enum
- Follow esp idf conventions
- Use logging. When an error is returned, a log should occure
- Avoid allocation when possible
- Think about syncronization and parrallelism explicitly
- This is a battery powered device

