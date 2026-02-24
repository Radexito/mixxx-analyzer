"""Produces a platform-specific wheel tagged py3-none-<platform>.

BinaryDistribution forces has_ext_modules()=True so bdist_wheel sets
Root-Is-Purelib: false (required by auditwheel). get_tag() then overrides
just the filename tag to py3-none-<platform> for broad compatibility.
"""

from setuptools import setup, Distribution
from wheel.bdist_wheel import bdist_wheel as _bdist_wheel


class BinaryDistribution(Distribution):
    def has_ext_modules(self):
        return True


class bdist_wheel(_bdist_wheel):
    def get_tag(self):
        _, _, plat = super().get_tag()
        return "py3", "none", plat  # py3-none-<platform>


setup(distclass=BinaryDistribution, cmdclass={"bdist_wheel": bdist_wheel})
