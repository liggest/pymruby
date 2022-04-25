# from distutils.core import setup, Extension
from setuptools import setup, Extension #, find_packages
from os.path import isfile,exists

try:
    if isfile("MANIFEST"):
        import os
        os.unlink("MANIFEST")
except:
    pass

extra=[]
libpath=r"mruby/build/host/lib/libmruby"
if exists(f'{libpath}.a'):
    extra.append(f'{libpath}.a')
elif exists(f'{libpath}.lib'):
    extra.append(f'{libpath}.lib')


ext_modules = [
    Extension('pymruby', ['src/pymruby.c'],
              extra_compile_args = ['-g', '-fPIC', '-DMRB_ENABLE_DEBUG_HOOK'],
              library_dirs = ['mruby/build/host/lib'],
              include_dirs = ['mruby/include'],
              extra_objects = extra,
              depends = ['src/pymruby.h'])
]

setup(name = 'pymruby',
      version = '0.2',
      description = 'Python binding for mruby',
      author = 'Jason Snell',
      author_email = 'jason@snell.io',
      url = 'http://www.ruby.dj',
      license = 'MIT',
    #   packages=find_packages(exclude=["tests", "*.tests", "*.tests.*", "tests.*"]),
      ext_package = '',
      ext_modules = ext_modules
)
