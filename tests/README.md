# Tests

The actual test suite lives at
[`backend/tests/`](../backend/tests/) (GoogleTest, 63 tests across 6
files — see
[`docs/architecture/validation.md`](../docs/architecture/validation.md)
for the full breakdown and pass evidence on both Linux and Windows CI).

This top-level `tests/` folder exists only as a pointer, since the
project's actual test code is C++ and is naturally organized next to
the backend it tests (CMake/CTest convention), not duplicated here.

End-to-end UI test scripts (Playwright) are at
[`scripts/e2e_check.py`](../scripts/e2e_check.py) and
[`scripts/e2e_interactive.py`](../scripts/e2e_interactive.py); see
[`docs/technical_report/E2E_CHECK.md`](../docs/technical_report/E2E_CHECK.md).
