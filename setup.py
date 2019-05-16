from setuptools import setup, find_packages, Extension
import sys

setup(
    name="pyinstrument_cext",
    ext_modules=[Extension('pyinstrument_cext', sources=['pyinstrument_cext.c'])],
    version="0.2.2",
    description="A CPython extension supporting pyinstrument",
    author='Joe Rickerby',
    author_email='joerick@mac.com',
    url='https://github.com/joerick/pyinstrument_cext',
    keywords=['profiling', 'profile', 'profiler', 'statistical', 'setstatprofile'],
    zip_safe=False,
    test_suite='nose.collector',
    tests_require=['nose'],
)
