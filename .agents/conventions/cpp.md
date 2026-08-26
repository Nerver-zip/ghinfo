# C++ Conventions

- Standard: C++23.
- Namespace: `ghinfo`.
- Headers under `include/ghinfo/`.
- Implementation under `src/`.
- Prefer `std::string_view` for non-owning read-only parameters only when lifetime is obvious.
- Prefer `std::optional<T>` to sentinel values.
- Use `std::chrono` types internally for time/durations.
- Use `std::uint64_t` for GitHub numeric IDs.
- Prefer `enum class`.
- Serialize enums through named conversion functions.
- Functions that cannot fail by exception and form a stable contract should be `noexcept`.
- No raw owning pointers.
- No global mutable state.
- Warnings are errors for project targets.
