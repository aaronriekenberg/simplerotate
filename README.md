# simplerotate

A simple command line app to read from stdin and echo the output to a file "output".

When file reaches a max size (10 megabytes) the file is rotated:
* "output.1" -> "output.2"
* "output" -> "output.1"

A maximum of 10 output files are supported.

The flock() system call is used for simple file locking at startup of a file named "lock".

Useful as a simple app to record and rotate log files.
