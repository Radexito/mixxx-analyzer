"""Produces a platform-specific wheel tagged py3-none-<platform>.

py3-none means "any Python 3, no C ABI" — correct for a wheel that only
bundles a pre-compiled native binary (not a Python C extension).
"""

from setuptools import setup
from wheel.bdist_wheel import bdist_wheel as _bdist_wheel


class bdist_wheel(_bdist_wheel):
    def finalize_options(self):
        super().finalize_options()
        self.root_is_pure = False  # platform-specific wheel

    def get_tag(self):
        _, _, plat = super().get_tag()
        return "py3", "none", plat  # py3-none-<platform>


setup(cmdclass={"bdist_wheel": bdist_wheel})
