
import pymruby
import time
import sys

# TODO: move these over to Nose

foo = pymruby.Pymruby()
print(foo.eval("'RUBY_VERSION: ' + RUBY_VERSION"))
print(foo.eval("n=''; n += 'a' * 10**5; 'hi'"))
print(foo.eval("__FILE__"))
print(foo.eval("loop {}"))
print(foo.eval("while true; end;"))
# print(foo.eval("while 1 do puts 'woah' end"))
# print(foo.eval("while 1 do print 'woah' end"))

print(foo.eval("def spinner(n); ['|', '\\\\', '-', '/'][n % 4]; end"))
i = 0

sys.stdout.write("\n")
# while True:
for i in range(10):
 sys.stdout.write("\033[1G")
 sys.stdout.write(foo.eval("spinner (%s) \r" % i))
 sys.stdout.flush()
 i= i + 1
 time.sleep(0.2)

try_code='''
begin
    puts "begin!"
    while true; end;
rescue
    puts "rescue!"
ensure
    puts "ensure!"
end
'''

print(foo.eval(try_code))

try_code='''
begin
    puts "begin!"
    while true; end;
rescue Exception
    puts "rescue!"
    while true; end;
ensure
    puts "ensure!"
end
'''

print(repr(foo.eval(try_code)))

print(pymruby.eval(try_code))