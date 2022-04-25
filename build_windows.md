## Attempt to build in Windows

#### MRuby side

```shell
git submodule update --init
cd mruby
```

Add `conf.cc.defines = %w(MRB_USE_DEBUG_HOOK)` as build configuration.

(Or simply using `default.rb` to replace `mruby\build_config\default.rb`)

In Visual Studio Command Prompt:

```shell
rake
```

#### pymruby side

```shell
python setup.py install
```