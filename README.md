# pymruby
This is a quick and dirty Python module that embeds mruby.  It has basic constraints around mruby's `eval()` call including a memory and execution time watchdog.

## Building
Building this module requires some slight tweaks to mruby.  After cloning the repository, pull in the submodule by running

    git submodule update --init

Next, compile mruby

    cd mruby && rake

Now you're ready to build and install the module

    python setup.py install

If all goes well, you should be able to run a few minimal tests

    ./test.py
