Title: Tracing syscalls from Python with gdb
Date: 2024-08-09
Category: python
Tags: python, debugging
Slug: tracing-python-syscalls-with-gdb
Summary: Tracing syscalls made from Python


# How to trace network request or Using gdb to instrument syscalls made from Python

There have been a number of times I've inherited some Python project and I have no idea where network requests are being made. For example, I've inherited large unit testing suites where some, but not all, network requests are being mocked. Another example is dependencies making surprising network requests. The goals of this post are:

* Provide a tool to trackdown network requests in a Python process
* Determine the context of the network request, that is the Python backtrace. This can be useful because if we want to find tests missing mocks.

The technique discussed here can be leveraged generally to trace syscalls from Python.

# Strategy

There are different methods to track down network requests. There are 2 common ones I hear:

* Set up a network proxy
* Monkey patch the requests library (or some other library that is being used)

Neither approach gives us exactly what we want. Setting up a network proxy can tell us requests are being made but don't directly tell us where in our Python code those calls come from. Monkey patching assumes we know what library/libraries are being used and we know what to patch.

For this post I explore a different solution, using `gdb`. This has some benefits:

* This lets us observe low level events so we don't miss any network requests. In particular we will instrument syscalls made when a network request occurs.
* `gdb` can print the Python backtrace
* We can isolate instrumentation to the process of interest
* We get to learn about catching syscalls from Python in general

# Methodology

I've built a docker image, `bdirks/pytrace`, which is available on [Docker Hub](https://hub.docker.com/repository/docker/bdirks/pytrace/general) which allows you to follow along with the code in this section as well as use for your own debugging needs. The [Dockerfile and supporting scripts can be found here](https://github.com/billdirks/blog-code/blob/main/tracing_syscalls_from_python_with_gdb). To launch the docker image, run:

```
docker run -it --rm bdirks/pytrace bash
```

We are going to use `gdb` to catch a network syscall and print a backtrace when it occurs. To enable `gdb` to print Python symbols and backtraces we have installed the debug build of Python into our Docker image. We have also installed some other tools to help us explore and do some text processing: `ps`, `strace`, and `perl`.

Also included in the docker image is a Python script, `demo.py`, that makes a single http request and prints the results:

```
import urllib.request

def main():
    contents = urllib.request.urlopen("http://example.com").read()
    print(contents)

if __name__ == '__main__':
    main()
```

You can run this script by typing this command on the docker bash console:

```
python3 ./demo.py
```

If you want to see every syscall that is made during the execution of the script you can use `strace`:

```
strace python3 demo.py
```

Output:
```
execve("/usr/bin/python3", ["python3", "demo.py"], 0xffffec396d48 /* 7 vars */) = 0
brk(NULL)                               = 0x2964000
mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0xffffa9746000
faccessat(AT_FDCWD, "/etc/ld.so.preload", R_OK) = -1 ENOENT (No such file or directory)
openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY|O_CLOEXEC) = 3
newfstatat(3, "", {st_mode=S_IFREG|0644, st_size=19686, ...}, AT_EMPTY_PATH) = 0
mmap(NULL, 19686, PROT_READ, MAP_PRIVATE, 3, 0) = 0xffffa9741000
close(3)                                = 0
... LOTS OF OUTPUT ...
```

Wow, that's a lot of output. Glacing through the output we see each line begins with `<syscall_name>(<arguments>) ... `. To get a clearer view of what syscalls are being made we can parse the `strace` output and count the number of times each syscall is called:

```
strace python3 demo.py 2>&1 >/dev/null | perl -lane '@a = split(/\(/, $_); print($a[0])' | sort | uniq -c | sort
```

Output:
```
  1 +++ exited with 0 +++
  1 bind
  1 epoll_create1
  1 execve
  1 exit_group
  1 faccessat
  1 getegid
  1 geteuid
  1 getgid
  1 gettid
  1 getuid
  1 prlimit64
  1 rseq
  1 sendmmsg
  1 set_robust_list
  1 set_tid_address
  1 sysinfo
  1 uname
  1 write
  2 getcwd
  2 getsockname
  2 rt_sigprocmask
  2 sendto
  2 setsockopt
  3 getrandom
  3 ppoll
  3 recvfrom
  3 recvmsg
  4 fcntl
  4 readlinkat
  6 connect
  7 socket
 16 futex
 25 brk
 26 getdents64
 26 mprotect
 32 munmap
 56 mmap
 68 rt_sigaction
 76 ioctl
114 close
114 openat
147 lseek
165 read
503 newfstatat
```

Some syscalls with interesting sounding names are `connect` and `sendmmsg`. We can learn more about these by using `man`. For example, `man connect`, outputs a `NAME` section that reads `connect - initiate a connection on a socket` and `man sendmmsg` outputs `sendmmsg - send multiple messages on a socket`. Looking at the raw `strace` output we see what looks like a message being sent to example.com:

```
sendmmsg(... msg_iov=[{iov_base="\276\272\1\0\0\1\0\0\0\0\0\0\7example\3com\0\0\1\0\1", ...
```


We can use `gdb` interactively to catch this syscall and print the backtrace:
```
# gdb --args python3 demo.py
(gdb) catch syscall sendmmsg
Catchpoint 1 (syscall 'sendmmsg' [269])
(gdb) run

Starting program: /usr/bin/python3 demo.py
[Thread debugging using libthread_db enabled]
Using host libthread_db library "/lib/aarch64-linux-gnu/libthread_db.so.1".

Catchpoint 1 (call to syscall sendmmsg), 0x0000fffff7dcabcc in __GI___sendmmsg (fd=3, vmessages=vmessages@entry=0xffffffffd9e8,
    vlen=vlen@entry=2, flags=flags@entry=16384) at ../sysdeps/unix/sysv/linux/sendmmsg.c:30
30	../sysdeps/unix/sysv/linux/sendmmsg.c: No such file or directory.

(gdb) py-bt

Traceback (most recent call first):
  <built-in method getaddrinfo of module object at remote 0xfffff7512f20>
  File "/usr/lib/python3.11/socket.py", line 962, in getaddrinfo
    for res in _socket.getaddrinfo(host, port, family, type, proto, flags):
  File "/usr/lib/python3.11/socket.py", line 827, in create_connection
    for res in getaddrinfo(host, port, 0, SOCK_STREAM):
  File "/usr/lib/python3.11/http/client.py", line 941, in connect
    self.sock = self._create_connection(
  File "/usr/lib/python3.11/http/client.py", line 975, in send
    self.connect()
  File "/usr/lib/python3.11/http/client.py", line 1037, in _send_output
    self.send(msg)
  File "/usr/lib/python3.11/http/client.py", line 1277, in endheaders
    self._send_output(message_body, encode_chunked=encode_chunked)
  File "/usr/lib/python3.11/http/client.py", line 1328, in _send_request
    self.endheaders(body, encode_chunked=encode_chunked)
  File "/usr/lib/python3.11/http/client.py", line 1282, in request
    self._send_request(method, url, body, headers, encode_chunked)
  File "/usr/lib/python3.11/urllib/request.py", line 1348, in do_open
    h.request(req.get_method(), req.selector, req.data, headers,
  File "/usr/lib/python3.11/urllib/request.py", line 1377, in http_open
    return self.do_open(http.client.HTTPConnection, req)
  File "/usr/lib/python3.11/urllib/request.py", line 496, in _call_chain
    result = func(*args)
  File "/usr/lib/python3.11/urllib/request.py", line 536, in _open
    result = self._call_chain(self.handle_open, protocol, protocol +
  File "/usr/lib/python3.11/urllib/request.py", line 519, in open
    response = self._open(req, data)
  File "/usr/lib/python3.11/urllib/request.py", line 216, in urlopen
    return opener.open(url, data, timeout)
  File "//demo.py", line 4, in main
    contents = urllib.request.urlopen("http://example.com").read()
  File "//demo.py", line 8, in <module>
    main()

(gdb)
```

That works! Doing this interactively does get a bit arduous though. We hit this watchpoint multiple times for our single http request. If we were looking at a larger codebase, eg a large test suite, doing this interactively would be time consuming. Luckily, `gdb` is scriptable. I've included a `gdb` script in `gdb_cmd`:

```
set pagination off

catch syscall sendmmsg
run

while 1
  py-bt
  continue
end

# This deletes all breakpoints
delete
```

You can run it using:

```
# gdb --args /usr/bin/python3 ./demo.py < ./gdb_cmd > output.txt 2>&1
```

and then examine `output.txt`.

Note `gdb` has a flag, `-x` that lets us execute scripts, eg `gdb -x ./gdb_cmd --args /usr/bin/python3 ./demo.py`. I've chosen to use `<` to read the input because this will stream the commands 1 at a time and continue even if there is a failure. In our case, the `continue` command will fail after we reach the end of the `demo.py` and the script will stop, leaving us in `gdb`. Using `<` is a hack that allows us to execute the next expression and exit `gdb` even though we hit this error.

# Conclusion

We have a scriptable way of detecting every network request being made in a Python process and a way to get its associated backtrace using `gdb`. This methodology can be used to instrument any syscall. In addition, we discussed using `strace` to see examine all the syscalls from a process. Happy debugging!
