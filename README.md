pyinstrument_cext
=================

[Pyinstrument][1]'s C extensions - reducing the overhead of statistical profilers.

API
---

### `pyinstrument_cext.setstatprofile(profilefunc, interval)`

Sets the statistal profiler callback. The function in the same manner as [setprofile][2], but
instead of being called every on every call and return, the function is throttled - called no more
than every `interval` seconds with the current stack.

[1]: https://github.com/joerick/pyinstrument
[2]: https://docs.python.org/3/library/sys.html#sys.setprofile

Changelog
---------

### 0.2.3

- Build wheels for Python 3.8 and Python 3.9

### 0.2.2

- Avoid the immediate callback to the profiler function, setstatprofile
  now waits for `interval` before calling the function.

### 0.2.0

- Add support for multi-threading. Profiling sessions are per-thread, and they
  don't conflict with each other.

### 0.1.6

- Fixes to CI configuration

### 0.1.3

- Added LICENSE to sdist

### 0.1.2

- Added CI build/test/deployment